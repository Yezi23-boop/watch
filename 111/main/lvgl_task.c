#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl_port.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "gui_guider.h"
#include "events_init.h"
#include "printf_esp32.h"
#include "esp_timer.h"
#include "esp_freertos_hooks.h"
#include "simple_wifi_sta.h"
#include "hptts.h"
#include "time_weather.h"
#include "nvs_flash.h"
#include "lvgl_task.h"
static const char *TAG = "lvgl_task";

lv_ui guider_ui;
// CPU使用率监控相关变量
static TaskHandle_t cpu_monitor_task_handle = NULL;
// CPU使用率监控任务
static void cpu_monitor_task(void *arg)
{

    while (1)
    {
        // 等待5秒
        vTaskDelay(pdMS_TO_TICKS(5000));
        // 打印内存统计信息
        printf_esp32_memory_stats();
    }
}
void lvgl_task(void *pvParameter)
{

    ESP_LOGI(TAG, "Starting application");
    lvgl_port_init_all();

    lvgl_port_lock(0);
    // lv_demo_benchmark();
    setup_ui(&guider_ui);
    events_init(&guider_ui);
    lvgl_port_unlock();

    // 创建CPU使用率监控任务
    xTaskCreatePinnedToCore(
        cpu_monitor_task,         // 任务函数
        "cpu_monitor",            // 任务名称
        4096,                     // 栈大小
        NULL,                     // 参数
        1,                        // 优先级 (低优先级，避免影响其他任务)
        &cpu_monitor_task_handle, // 任务句柄
        1                         // 在CPU1上运行
    );

    // LVGL任务主循环 - 保持任务持续运行
    while (1)
    {
        // 让LVGL处理定时器和其他任务
        vTaskDelay(pdMS_TO_TICKS(10)); // 延迟10ms，给其他任务运行机会
    }
}