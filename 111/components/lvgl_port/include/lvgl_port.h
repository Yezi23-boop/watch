/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize all LVGL port components
 * 
 * This function initializes:
 * - CO5300 LCD panel
 * - FT5x06 touch controller  
 * - LVGL library and display/touch drivers
 * - Creates default display interface
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t lvgl_port_init_all(void);

/**
 * @brief Deinitialize all LVGL port components
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t lvgl_port_deinit_all(void);

/**
 * @brief Get LVGL display object
 * 
 * @return Pointer to LVGL display object, NULL if not initialized
 */
lv_display_t* lvgl_port_get_display(void);

/**
 * @brief Get LVGL touch input device object
 * 
 * @return Pointer to LVGL touch input device object, NULL if not initialized
 */
lv_indev_t* lvgl_port_get_touch_indev(void);

#ifdef __cplusplus
}
#endif