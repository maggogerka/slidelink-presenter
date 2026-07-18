idf_component_register(SRCS "command_parser.c" "presenter_console.c"
                       INCLUDE_DIRS "include"
                       REQUIRES command_router esp_app_format esp_driver_uart esp_timer vfs usb_hid)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
