idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES command_router esp_app_format presenter_button presenter_console usb_hid)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
