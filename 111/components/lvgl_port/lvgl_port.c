/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lvgl_port.c
 * @brief LVGL移植层实现文件
 *
 * 本文件实现了LVGL图形库在ESP32平台上的移植，集成了：
 * - CO5300 LCD显示面板驱动
 * - FT5x06触摸控制器驱动
 * - LVGL显示和触摸输入设备的初始化和配置
 *
 * 主要功能：
 * 1. 初始化CO5300 LCD面板
 * 2. 初始化FT5x06触摸控制器
 * 3. 配置LVGL显示驱动程序
 * 4. 配置LVGL触摸输入驱动程序
 * 5. 创建默认的用户界面
 */

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lvgl_port.h"
#include "lvgl_port.h"

// 集成自定义组件头文件
#include "co5300_panel.h"          // CO5300 LCD面板驱动
#include "touch_ft5x06.h"          // FT5x06触摸控制器驱动
#include "co5300_panel_defaults.h" // CO5300面板默认配置

// 日志标签
static const char *TAG = "LVGL_PORT";

// LVGL图像声明（预留，当前未使用）
LV_IMG_DECLARE(esp_logo)

/* LCD IO和面板句柄 */
static esp_lcd_panel_io_handle_t lcd_io = NULL;    // LCD IO句柄
static esp_lcd_panel_handle_t lcd_panel = NULL;    // LCD面板句柄
static esp_lcd_touch_handle_t touch_handle = NULL; // 触摸控制器句柄

/* LVGL显示和触摸设备对象 */
static lv_display_t *lvgl_disp = NULL;      // LVGL显示对象
static lv_indev_t *lvgl_touch_indev = NULL; // LVGL触摸输入设备对象

/**
 * @brief 初始化LCD显示面板
 *
 * 该函数负责初始化CO5300 LCD显示面板，包括：
 * 1. 调用CO5300面板组件的初始化函数
 * 2. 获取底层LCD IO和面板句柄供LVGL使用
 *
 * @return ESP_OK 成功
 * @return 其他错误码 初始化失败
 */
static esp_err_t app_lcd_init(void)
{
    ESP_LOGI(TAG, "开始初始化CO5300 LCD面板");

    // 使用CO5300面板组件进行初始化
    // 这会配置SPI接口、GPIO引脚、面板参数等
    ESP_RETURN_ON_ERROR(co5300_panel_init(), TAG, "CO5300面板初始化失败");

    // 获取底层句柄供LVGL使用
    // lcd_io: LCD IO句柄，用于LVGL与LCD硬件通信
    // lcd_panel: LCD面板句柄，用于LVGL控制面板操作
    ESP_RETURN_ON_ERROR(co5300_panel_get_raw(&lcd_io, &lcd_panel), TAG, "获取CO5300面板句柄失败");

    // 设置显示偏移 - 向右偏移20像素
    ESP_LOGI(TAG, "设置显示向右偏移20像素");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(lcd_panel, 23, 0), TAG, "设置显示偏移失败");

    ESP_LOGI(TAG, "CO5300 LCD面板初始化成功");
    return ESP_OK;
}

/**
 * @brief 初始化触摸控制器
 *
 * 该函数负责初始化FT5x06触摸控制器，包括：
 * 1. 调用FT5x06触摸组件的初始化函数
 * 2. 获取触摸控制器句柄供LVGL使用
 *
 * @return ESP_OK 成功
 * @return 其他错误码 初始化失败
 */
static esp_err_t app_touch_init(void)
{
    ESP_LOGI(TAG, "开始初始化FT5x06触摸控制器");

    // 使用FT5x06触摸组件进行初始化
    // 这会配置I2C接口、GPIO引脚、触摸参数等
    ESP_RETURN_ON_ERROR(touch_ft5x06_init(), TAG, "FT5x06触摸控制器初始化失败");

    // 获取触摸句柄供LVGL使用
    // touch_handle: 触摸控制器句柄，用于LVGL读取触摸数据
    ESP_RETURN_ON_ERROR(touch_ft5x06_get_handle(&touch_handle), TAG, "获取FT5x06触摸句柄失败");

    ESP_LOGI(TAG, "FT5x06触摸控制器初始化成功");
    return ESP_OK;
}

/**
 * @brief 初始化LVGL图形库
 *
 * 该函数负责初始化LVGL图形库并配置显示和触摸驱动，包括：
 * 1. 初始化LVGL核心库和任务
 * 2. 配置并添加LCD显示驱动
 * 3. 配置并添加触摸输入驱动
 *
 * @return ESP_OK 成功
 * @return 其他错误码 初始化失败
 */
static esp_err_t app_lvgl_init(void)
{
    // /* 初始化LVGL核心库 */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,       /* LVGL任务优先级 - 使用标准优先级 */
        .task_stack = 1024 * 8,   /* LVGL任务堆栈大小(字节) - 保持8KB */
        .task_affinity = 1,       /* LVGL任务CPU1亲和性 - 绑定到CPU1 */
        .task_max_sleep_ms = 500, /* LVGL任务最大休眠时间(毫秒) - 使用标准值 */
        .timer_period_ms = 5      /* LVGL定时器周期(毫秒)  */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL端口初始化失败");

    /* 添加LCD显示屏到LVGL */
    ESP_LOGI(TAG, "将CO5300 LCD显示屏添加到LVGL");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_io,                    // LCD IO句柄
        .panel_handle = lcd_panel,              // LCD面板句柄
        .buffer_size = CO5300_PANEL_H_RES * 20, // 显示缓冲区大小(像素数)，使用40行避免传输问题
        .double_buffer = false,                 // 暂时禁用双缓冲，避免SPI传输问题
        .hres = CO5300_PANEL_H_RES,             // 水平分辨率(410像素)
        .vres = CO5300_PANEL_V_RES,             // 垂直分辨率(502像素)
        .monochrome = false,                    // 彩色显示(非单色)
        .color_format = LV_COLOR_FORMAT_RGB565, // 颜色格式RGB565
        .rotation = {
            .mirror_x = false, // 不镜像X轴
            .mirror_y = false, // 不镜像Y轴
        },
        .flags = {
            .buff_dma = true,     // 禁用DMA，避免传输问题
            .buff_spiram = false, // 暂时使用内部RAM，确保SPI传输稳定
            .swap_bytes = true,   // 交换字节序
        }};
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    /* 添加触摸输入到LVGL */
    ESP_LOGI(TAG, "将FT5x06触摸输入添加到LVGL");
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,      // 关联的显示对象
        .handle = touch_handle, // 触摸控制器句柄
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);

    ESP_LOGI(TAG, "LVGL初始化完成");
    return ESP_OK;
}

/* ========================================================================== */
/*                              公共接口函数                                    */
/* ========================================================================== */

/**
 * @brief 初始化所有LVGL端口组件
 *
 * 这是LVGL端口的主要初始化函数，按顺序执行以下操作：
 * 1. 初始化CO5300 LCD显示面板
 * 2. 初始化FT5x06触摸控制器
 * 3. 初始化LVGL图形库和驱动程序
 * 4. 创建默认的用户界面
 *
 * @return ESP_OK 所有组件初始化成功
 * @return 其他错误码 某个组件初始化失败
 *
 * @note 此函数应在主程序中调用一次，在系统启动时进行初始化
 * @note 如果初始化失败，应检查硬件连接和配置参数
 */
esp_err_t lvgl_port_init_all(void)
{
    ESP_LOGI(TAG, "开始LVGL端口初始化");

    // 步骤1: 初始化LCD面板
    ESP_RETURN_ON_ERROR(app_lcd_init(), TAG, "LCD初始化失败");

    // 步骤2: 初始化触摸控制器
    ESP_RETURN_ON_ERROR(app_touch_init(), TAG, "触摸控制器初始化失败");

    // 步骤3: 初始化LVGL图形库
    ESP_RETURN_ON_ERROR(app_lvgl_init(), TAG, "LVGL初始化失败");

    ESP_LOGI(TAG, "LVGL端口初始化完成");
    return ESP_OK;
}

/**
 * @brief 去初始化所有LVGL端口组件
 *
 * 释放LVGL端口使用的所有资源，包括：
 * - LVGL图形库资源
 * - 显示驱动程序资源
 * - 触摸驱动程序资源
 * - 硬件资源
 *
 * @return ESP_OK 去初始化成功
 * @return 其他错误码 去初始化失败
 *
 * @note 当前实现为占位符，实际项目中需要根据具体需求实现完整的资源释放
 * @note 调用此函数后，需要重新调用lvgl_port_init_all()才能再次使用LVGL
 */
esp_err_t lvgl_port_deinit_all(void)
{
    ESP_LOGI(TAG, "开始LVGL端口去初始化");

    // 注意：这里只是示例，实际的去初始化需要根据具体需求实现
    // 应该包括以下步骤：
    // 1. 停止LVGL任务
    // 2. 释放显示缓冲区
    // 3. 去初始化触摸驱动
    // 4. 去初始化LCD驱动
    // 5. 释放GPIO和SPI资源
    //
    // 示例代码：
    // lvgl_port_deinit();
    // touch_ft5x06_deinit();
    // co5300_panel_deinit();

    ESP_LOGI(TAG, "LVGL端口去初始化完成");
    return ESP_OK;
}

/**
 * @brief 获取LVGL显示对象
 *
 * 返回当前活动的LVGL显示对象指针，可用于：
 * - 设置显示属性（亮度、旋转等）
 * - 创建与特定显示关联的对象
 * - 获取显示状态信息
 *
 * @return lv_display_t* LVGL显示对象指针
 * @return NULL 显示对象未初始化或初始化失败
 *
 * @note 只有在成功调用lvgl_port_init_all()后，此函数才会返回有效指针
 * @note 返回的指针在LVGL端口生命周期内保持有效
 */
lv_display_t *lvgl_port_get_display(void)
{
    return lvgl_disp;
}

/**
 * @brief 获取LVGL触摸输入设备对象
 *
 * 返回当前活动的LVGL触摸输入设备对象指针，可用于：
 * - 配置触摸参数（灵敏度、校准等）
 * - 获取触摸状态信息
 * - 手动处理触摸事件
 *
 * @return lv_indev_t* LVGL触摸输入设备对象指针
 * @return NULL 触摸输入设备未初始化或初始化失败
 *
 * @note 只有在成功调用lvgl_port_init_all()后，此函数才会返回有效指针
 * @note 返回的指针在LVGL端口生命周期内保持有效
 */
lv_indev_t *lvgl_port_get_touch_indev(void)
{
    return lvgl_touch_indev;
}