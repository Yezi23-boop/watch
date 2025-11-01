/**
 * @file co5300_panel.c
 * @brief CO5300 LCD面板驱动实现文件
 *
 * 本文件实现了CO5300 LCD面板的初始化、控制和管理功能。
 * CO5300是一款支持QSPI接口的LCD控制器，分辨率为410x502像素。
 *
 * 主要功能：
 * - 面板初始化和配置
 * - TE（撕裂效应）信号处理
 * - 颜色传输完成回调管理
 * - 面板句柄获取
 *
 * @author ESP32开发团队
 * @date 2024
 */

#include "co5300_panel.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_co5300.h"
#include "esp_log.h"
#include "esp_check.h"
#include "co5300_panel_defaults.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// 日志标签，用于ESP_LOG系列函数
static const char *TAG = "co5300_panel";

/* ========== 全局变量定义 ========== */

/**
 * @brief LCD面板IO句柄
 * 用于管理SPI通信接口，处理命令和数据传输
 */
static esp_lcd_panel_io_handle_t s_io_handle = NULL;

/**
 * @brief LCD面板句柄
 * 用于控制面板的显示操作，如复位、初始化、开关显示等
 */
static esp_lcd_panel_handle_t s_panel_handle = NULL;

/**
 * @brief TE（撕裂效应）信号量
 * 用于同步显示刷新，避免画面撕裂现象
 * 当TE信号到来时，通过信号量通知应用层可以进行下一帧数据传输
 */
static SemaphoreHandle_t s_te_semaphore = NULL;

/**
 * @brief 初始化状态标志
 * true: 面板已初始化
 * false: 面板未初始化
 */
static bool s_initialized = false;

/* ========== 中断处理函数 ========== */

/**
 * @brief TE信号GPIO中断处理函数
 *
 * 当TE引脚检测到上升沿时触发此中断。
 * TE信号表示LCD内部扫描完成一帧，此时可以安全地开始下一帧数据传输，
 * 避免在扫描过程中更新数据导致的画面撕裂。
 *
 * @param arg 中断参数（本实现中未使用）
 *
 * @note 此函数在中断上下文中执行，使用IRAM_ATTR确保代码在内存中
 * @note 使用FromISR版本的FreeRTOS API函数
 */
static void IRAM_ATTR te_gpio_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // 检查信号量是否已创建
    if (s_te_semaphore != NULL)
    {
        // 在中断中释放信号量，通知等待的任务
        xSemaphoreGiveFromISR(s_te_semaphore, &xHigherPriorityTaskWoken);

        // 如果有更高优先级的任务被唤醒，则进行任务切换
        if (xHigherPriorityTaskWoken == pdTRUE)
        {
            portYIELD_FROM_ISR();
        }
    }
}

/* ========== 回调函数 ========== */

/**
 * @brief 默认颜色传输完成回调函数
 *
 * 当颜色数据传输完成时被调用。此默认实现为空操作，
 * 用户可以通过co5300_panel_register_color_done_callback注册自定义回调。
 *
 * @param panel_io 面板IO句柄
 * @param edata 事件数据
 * @param user_ctx 用户上下文
 * @return false 表示不需要进行任务切换
 */
static bool default_color_trans_done_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    // 默认实现为空，返回false表示不需要任务切换
    return false;
}

/* ========== 公共API函数实现 ========== */

/**
 * @brief 初始化CO5300面板
 *
 * 执行完整的面板初始化流程，包括：
 * 1. 创建TE信号量
 * 2. 配置TE引脚和中断
 * 3. 初始化QSPI总线
 * 4. 安装面板IO
 * 5. 安装CO5300面板驱动
 * 6. 执行面板复位、初始化和开启显示
 *
 * @return
 *     - ESP_OK: 初始化成功
 *     - ESP_ERR_NO_MEM: 内存不足
 *     - 其他错误码: 具体的初始化失败原因
 *
 * @note 此函数只能调用一次，重复调用会返回ESP_OK但不会重新初始化
 */
esp_err_t co5300_panel_init(void)
{
    // 检查是否已经初始化
    if (s_initialized)
    {
        ESP_LOGW(TAG, "Panel already initialized");
        return ESP_OK;
    }

    /* 步骤1: 初始化TE信号量 */
    ESP_LOGI(TAG, "Creating TE semaphore");
    s_te_semaphore = xSemaphoreCreateBinary();
    if (s_te_semaphore == NULL)
    {
        ESP_LOGE(TAG, "Failed to create TE semaphore");
        return ESP_ERR_NO_MEM;
    }

    /* 步骤2: 配置TE引脚和中断 */
    ESP_LOGI(TAG, "Configuring TE GPIO pin %d", CO5300_PANEL_PIN_TE);
    gpio_config_t te_gpio_config = {
        .pin_bit_mask = 1ULL << CO5300_PANEL_PIN_TE, // 设置引脚位掩码
        .mode = GPIO_MODE_INPUT,                     // 输入模式
        .pull_up_en = GPIO_PULLUP_DISABLE,           // 禁用上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,       // 禁用下拉
        .intr_type = GPIO_INTR_POSEDGE,              // 上升沿触发中断
    };
    ESP_RETURN_ON_ERROR(gpio_config(&te_gpio_config), TAG, "Configure TE GPIO failed");
    ESP_RETURN_ON_ERROR(gpio_install_isr_service(0), TAG, "Install GPIO ISR service failed");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(CO5300_PANEL_PIN_TE, te_gpio_isr_handler, NULL), TAG, "Add TE ISR handler failed");

    /* 步骤3: 初始化QSPI总线 */
    ESP_LOGI(TAG, "Initialize QSPI bus on host %d", CO5300_PANEL_HOST);
    const spi_bus_config_t buscfg = CO5300_PANEL_BUS_QSPI_CONFIG(CO5300_PANEL_PIN_PCLK,
                                                                 CO5300_PANEL_PIN_D0,
                                                                 CO5300_PANEL_PIN_D1,
                                                                 CO5300_PANEL_PIN_D2,
                                                                 CO5300_PANEL_PIN_D3,
                                                                 CO5300_PANEL_H_RES * CO5300_PANEL_MAX_TRANSFER_LINES * sizeof(uint16_t));
    ESP_RETURN_ON_ERROR(spi_bus_initialize(CO5300_PANEL_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI bus initialize failed");

    /* 步骤4: 安装面板IO */
    ESP_LOGI(TAG, "Install panel IO with CS pin %d", CO5300_PANEL_PIN_CS);
    // 使用优化的QSPI配置以提升传输性能
    const esp_lcd_panel_io_spi_config_t io_config = CO5300_PANEL_IO_QSPI_CONFIG_OPTIMIZED(CO5300_PANEL_PIN_CS,
                                                                                          default_color_trans_done_cb,
                                                                                          NULL);
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)CO5300_PANEL_HOST, &io_config, &s_io_handle), TAG, "New panel IO failed");

    /* 步骤5: 安装CO5300面板驱动 */
    ESP_LOGI(TAG, "Install CO5300 panel driver");
    co5300_vendor_config_t vendor_config = {
        // 使用默认初始化命令
        // 可以在这里添加自定义的初始化命令序列
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = CO5300_PANEL_PIN_RST,       // 复位引脚
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,   // RGB像素顺序
        .bits_per_pixel = CO5300_PANEL_BIT_PER_PIXEL, // 每像素位数
        .vendor_config = (void *)&vendor_config,      // 厂商特定配置
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_co5300(s_io_handle, &panel_config, &s_panel_handle), TAG, "New panel failed");

    /* 步骤6: 初始化面板 */
    ESP_LOGI(TAG, "Reset and initialize panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel_handle), TAG, "Panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel_handle), TAG, "Panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel_handle, true), TAG, "Panel display on failed");

    // 标记为已初始化
    s_initialized = true;
    ESP_LOGI(TAG, "CO5300 panel initialized successfully");
    return ESP_OK;
}

/**
 * @brief 获取面板的原始句柄
 *
 * 返回底层的面板IO和面板句柄，供高级应用直接操作。
 * 这些句柄可以用于调用ESP-IDF的原生LCD API。
 *
 * @param io 输出参数，返回面板IO句柄（可以为NULL）
 * @param panel 输出参数，返回面板句柄（可以为NULL）
 * @return
 *     - ESP_OK: 成功获取句柄
 *     - ESP_ERR_INVALID_STATE: 面板未初始化或句柄无效
 *
 * @note 调用此函数前必须先调用co5300_panel_init()
 * @note 如果不需要某个句柄，可以传入NULL
 */
esp_err_t co5300_panel_get_raw(struct esp_lcd_panel_io_t **io, struct esp_lcd_panel_t **panel)
{
    // 检查初始化状态和句柄有效性
    if (!s_initialized || s_io_handle == NULL || s_panel_handle == NULL)
    {
        ESP_LOGE(TAG, "Panel not initialized or handles invalid");
        return ESP_ERR_INVALID_STATE;
    }

    // 返回IO句柄（如果请求）
    if (io != NULL)
    {
        *io = s_io_handle;
    }

    // 返回面板句柄（如果请求）
    if (panel != NULL)
    {
        *panel = s_panel_handle;
    }

    return ESP_OK;
}

/**
 * @brief 等待TE信号
 *
 * 阻塞等待下一个TE（撕裂效应）信号。TE信号表示LCD完成了一帧的扫描，
 * 此时开始新的数据传输可以避免画面撕裂。
 *
 * @param timeout_ms 超时时间（毫秒）
 * @return
 *     - true: 成功接收到TE信号
 *     - false: 超时或信号量未初始化
 *
 * @note 此函数会阻塞调用任务直到TE信号到来或超时
 * @note 建议在每次屏幕刷新前调用此函数以获得最佳显示效果
 */
bool co5300_panel_wait_for_te(uint32_t timeout_ms)
{
    // 检查TE信号量是否已初始化
    if (s_te_semaphore == NULL)
    {
        ESP_LOGW(TAG, "TE semaphore not initialized");
        return false;
    }

    // 将毫秒转换为FreeRTOS时钟节拍
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    // 等待信号量，返回是否成功获取
    return xSemaphoreTake(s_te_semaphore, timeout_ticks) == pdTRUE;
}

/**
 * @brief 注册颜色传输完成回调函数
 *
 * 注册一个回调函数，当颜色数据传输完成时被调用。
 * 这对于实现双缓冲、性能监控或其他高级显示功能很有用。
 *
 * @param cbs 回调函数结构体指针
 * @param user_ctx 用户上下文，会传递给回调函数
 * @return
 *     - ESP_OK: 成功注册回调
 *     - ESP_ERR_INVALID_STATE: 面板未初始化
 *     - ESP_ERR_INVALID_ARG: 回调结构体为NULL
 *     - 其他错误码: 注册失败的具体原因
 *
 * @note 调用此函数前必须先调用co5300_panel_init()
 * @note 回调函数在中断上下文中执行，应保持简短
 */
esp_err_t co5300_panel_register_color_done_callback(const esp_lcd_panel_io_callbacks_t *cbs, void *user_ctx)
{
    // 检查初始化状态
    if (!s_initialized || s_io_handle == NULL)
    {
        ESP_LOGE(TAG, "Panel not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 检查回调结构体参数
    if (cbs == NULL)
    {
        ESP_LOGE(TAG, "Callback structure is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // 注册回调到面板IO
    return esp_lcd_panel_io_register_event_callbacks(s_io_handle, cbs, user_ctx);
}
