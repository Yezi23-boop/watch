/**
 * @file co5300_panel_defaults.h
 * @brief CO5300 LCD面板默认配置头文件
 *
 * 本文件定义了CO5300 LCD面板的默认配置参数，包括：
 * - GPIO引脚分配
 * - SPI主机配置
 * - 显示分辨率参数
 * - QSPI总线和IO配置宏
 *
 * 这些默认值可以根据具体硬件设计进行调整。
 *
 * @author ESP32开发团队
 * @date 2024
 */

#pragma once

#include "driver/spi_master.h"
#include "hal/spi_types.h"

/* ========== GPIO引脚定义 ========== */

/**
 * @brief QSPI时钟引脚
 * 用于SPI时钟信号，控制数据传输的时序
 */
#define CO5300_PANEL_PIN_PCLK 11

/**
 * @brief QSPI片选引脚
 * 用于选择CO5300设备，低电平有效
 */
#define CO5300_PANEL_PIN_CS 12

/**
 * @brief QSPI数据线0（MOSI）
 * 在单线模式下作为主输出从输入线，在四线模式下作为数据线0
 */
#define CO5300_PANEL_PIN_D0 4

/**
 * @brief QSPI数据线1
 * 在四线模式下作为数据线1，提高传输带宽
 */
#define CO5300_PANEL_PIN_D1 5

/**
 * @brief QSPI数据线2（WP）
 * 在四线模式下作为数据线2，通常也是写保护引脚
 */
#define CO5300_PANEL_PIN_D2 6

/**
 * @brief QSPI数据线3（HOLD）
 * 在四线模式下作为数据线3，通常也是保持引脚
 */
#define CO5300_PANEL_PIN_D3 7

/**
 * @brief LCD复位引脚
 * 用于硬件复位LCD控制器，低电平有效
 */
#define CO5300_PANEL_PIN_RST 8

/**
 * @brief TE（撕裂效应）信号引脚
 * LCD输出的同步信号，表示一帧扫描完成，用于避免画面撕裂
 */
#define CO5300_PANEL_PIN_TE 13

/* ========== SPI主机配置 ========== */

/**
 * @brief SPI主机编号
 * ESP32-S3有多个SPI主机，这里使用SPI2_HOST
 * SPI2_HOST通常用于外部设备通信
 */
#define CO5300_PANEL_HOST SPI2_HOST

/* ========== 显示分辨率参数 ========== */

/**
 * @brief LCD水平分辨率（像素）
 * CO5300控制器支持的水平像素数量
 */
#define CO5300_PANEL_H_RES 410

/**
 * @brief LCD垂直分辨率（像素）
 * CO5300控制器支持的垂直像素数量
 */
#define CO5300_PANEL_V_RES 502

/* ========== 显示控制参数 ========== */

/**
 * @brief 默认背光亮度
 * 亮度范围：0x00~0xFF
 * - 0x00: 最暗（关闭）
 * - 0xFF: 最亮
 */
#define CO5300_PANEL_DEFAULT_BRIGHTNESS 0xFF

/* 传输优化参数：优化单次颜色传输最大行数以提升性能 */
#define CO5300_PANEL_MAX_TRANSFER_LINES 80 // 从60增加到80行，提升传输效率

/**
 * @brief TE信号使能标志
 * 1: 启用TE信号同步，可以避免画面撕裂
 * 0: 禁用TE信号，可能出现画面撕裂但性能更高
 */
#define CO5300_PANEL_USE_TE_SIGNAL 1

/* ========== 性能优化配置 ========== */

/**
 * @brief 优化的SPI时钟频率（Hz）
 * 从40MHz提升到60MHz，在稳定性和性能间取得平衡
 * 注意：过高的频率可能导致信号完整性问题
 */
#define CO5300_PANEL_OPTIMIZED_PCLK_HZ (40 * 1000 * 1000)

/**
 * @brief 优化的传输队列深度
 * 从10增加到16，减少传输等待时间
 */
#define CO5300_PANEL_OPTIMIZED_TRANS_QUEUE_DEPTH 16

/* ========== LCD配置参数 ========== */

/**
 * @brief 每像素位数
 * 16位表示RGB565格式：
 * - 红色：5位
 * - 绿色：6位
 * - 蓝色：5位
 */
#define CO5300_PANEL_BIT_PER_PIXEL 16

/* ========== 优化的配置宏 ========== */

/**
 * @brief 优化的QSPI IO配置宏
 * 相比默认配置，提升了时钟频率和队列深度
 */
#define CO5300_PANEL_IO_QSPI_CONFIG_OPTIMIZED(cs, cb, cb_ctx)          \
    {                                                                  \
        .cs_gpio_num = cs,                                             \
        .dc_gpio_num = -1,                                             \
        .spi_mode = 0,                                                 \
        .pclk_hz = CO5300_PANEL_OPTIMIZED_PCLK_HZ,                     \
        .trans_queue_depth = CO5300_PANEL_OPTIMIZED_TRANS_QUEUE_DEPTH, \
        .on_color_trans_done = cb,                                     \
        .user_ctx = cb_ctx,                                            \
        .lcd_cmd_bits = 32,                                            \
        .lcd_param_bits = 8,                                           \
        .flags = {                                                     \
            .quad_mode = true,                                         \
        },                                                             \
    }

/* ========== 注意 ========== */
/*
 * QSPI配置宏已在 esp_lcd_co5300.h 中定义：
 * - CO5300_PANEL_BUS_QSPI_CONFIG
 * - CO5300_PANEL_IO_QSPI_CONFIG
 *
 * 新增的优化配置宏：
 * - CO5300_PANEL_IO_QSPI_CONFIG_OPTIMIZED
 *
 * 请根据实际需求选择使用标准配置或优化配置。
 */
