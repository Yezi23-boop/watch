# 自动查找所有 C 源文件（递归 main 与 ui 目录）
file(GLOB_RECURSE srcs
    ${CMAKE_CURRENT_LIST_DIR}/*.c
    ${CMAKE_CURRENT_LIST_DIR}/ui/*.c
)

# 设置包含目录
set(include_dirs
    .
    ui/custom
    ui/generated
)

# 注册组件
idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES  lvgl_port z_print_esp32
)