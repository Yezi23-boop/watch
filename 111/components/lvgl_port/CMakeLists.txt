idf_component_register(
    SRCS "lvgl_port.c"
    INCLUDE_DIRS "include" "."
    REQUIRES  touch_ft5x06 co5300_panel lvgl esp_lvgl_port
)
