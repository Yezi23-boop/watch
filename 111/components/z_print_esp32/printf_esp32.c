#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "printf_esp32.h"
/**
 * @brief 打印ESP32系统内存统计信息
 * @details 显示内部RAM和PSRAM的详细使用情况，包括LVGL内存池状态
 * @note 当前LVGL配置使用自定义内存管理器，LVGL内存池统计为0是正常现象
 */
void printf_esp32_memory_stats(void)
{
    // 1. 统计外部 PSRAM（SPI RAM）使用情况
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM); // PSRAM 总容量（字节）
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);   // PSRAM 当前空闲容量（字节）
    size_t psram_used = psram_total - psram_free;                     // PSRAM 已用容量（字节）

    // 2. 统计内部 RAM（片上 SRAM）使用情况
    size_t internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL); // 内部 RAM 总容量（字节）
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);   // 内部 RAM 当前空闲容量（字节）
    size_t internal_used = internal_total - internal_free;                 // 内部 RAM 已用容量（字节）

    // 3. 以 ESP-IDF 日志格式打印统计结果（等级：INFO，标签：TAG）
    ESP_LOGI(" ", "┌─────────────────────────────"); // 日志标题
    ESP_LOGI(" ", "│      📊 系统资源统计         ");
    ESP_LOGI(" ", "├─────────────────────────────");
    // 打印 PSRAM：已用/总容量（KB） + 使用率（保留1位小数），避免 PSRAM 不存在时除零错误
    ESP_LOGI(" ",
             "│ PSRAM: %6zu KB / %6zu KB (%.1f%%) ",
             psram_used / 1024,
             psram_total / 1024,
             psram_total > 0 ? (psram_used * 100.0f / psram_total) : 0);
    ESP_LOGI(" ",
             "│ RAM:   %6zu KB / %6zu KB (%.1f%%) ",
             internal_used / 1024,
             internal_total / 1024,
             internal_used * 100.0f / internal_total);
    ESP_LOGI(" ", "├─────────────────────────────");
    ESP_LOGI(" ", "│      ⚡ CPU任务统计           ");
    ESP_LOGI(" ", "└─────────────────────────────");

    // 4. 获取并打印CPU任务运行时统计信息（在未启用运行时统计时，打印提示）
#if (CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS)
    char stats_buffer[1024];
    vTaskGetRunTimeStats(stats_buffer);
    ESP_LOGI("CPU", "任务运行时统计:\n%s", stats_buffer);
#else
    ESP_LOGW("CPU", "未启用任务运行时统计。请在 sdkconfig 中开启 CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS ");
#endif
    ESP_LOGI(" ", "═══════════════════════════════"); // 日志结尾
}

/**
 * @brief 打印任务栈使用统计信息
 * @param task_handle 任务句柄
 * @param stack_size_bytes 任务栈大小（字节）
 * @param task_name 任务名称（用于日志显示）
 * @details 监控指定任务的栈使用情况，包括总大小、剩余空间、已使用最大栈和使用率
 */
void printf_esp32_task_stack_stats(TaskHandle_t task_handle, uint32_t stack_size_bytes, const char *task_name)
{
    if (task_handle == NULL || task_name == NULL)
    {
        ESP_LOGE(task_name, "无效的任务句柄或任务名称");
        return;
    }

    UBaseType_t watermark_words = uxTaskGetStackHighWaterMark(task_handle);
    uint32_t watermark_bytes = watermark_words * sizeof(StackType_t);

    float stack_total_kb = (float)stack_size_bytes / 1024.0f;
    uint32_t used_max_bytes = stack_size_bytes - watermark_bytes;
    float watermark_kb = (float)watermark_bytes / 1024.0f;
    float used_max_kb = (float)used_max_bytes / 1024.0f;
    float stack_usage_percent = ((float)used_max_bytes / stack_size_bytes) * 100.0f;

    ESP_LOGI(task_name, "┌─────────────────────────────────");
    ESP_LOGI(task_name, "│      %s Task Stack Stats        ", task_name);
    ESP_LOGI(task_name, "├─────────────────────────────────");
    ESP_LOGI(task_name, "│ Total:  %5.1f KB                ", stack_total_kb);
    ESP_LOGI(task_name, "│ Used:   %5.1f KB (%3.0f%%)      ", used_max_kb, stack_usage_percent);
    ESP_LOGI(task_name, "│ Free:   %5.1f KB                ", watermark_kb);
    ESP_LOGI(task_name, "└─────────────────────────────────");
}
