#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include "esp_lcd_panel_io.h"
#ifdef __cplusplus
extern "C"
{
#endif

    // 单一初始化接口：内部自管句柄
    esp_err_t co5300_panel_init(void);

    // 提供底层句柄（只读用途），便于其它组件集成（如 LVGL）。
    // 如未初始化或句柄无效，返回 ESP_ERR_INVALID_STATE。
    struct esp_lcd_panel_io_t; // fwd decl to avoid leaking headers
    struct esp_lcd_panel_t;    // fwd decl
    esp_err_t co5300_panel_get_raw(struct esp_lcd_panel_io_t **io, struct esp_lcd_panel_t **panel);

    // TE同步：等待TE上升沿，超时单位ms；返回true表示在超时前收到TE
    bool co5300_panel_wait_for_te(uint32_t timeout_ms);

    // 注册传输完成回调：用于在颜色数据传输完成时回调
    // 回调类型与结构来自 ESP-IDF 的 esp_lcd_panel_io.h
    // user_ctx 可传入 LVGL 的 disp_drv 指针以便回调中调用 lv_disp_flush_ready
    esp_err_t co5300_panel_register_color_done_callback(const esp_lcd_panel_io_callbacks_t *cbs, void *user_ctx);

#ifdef __cplusplus
}
#endif
