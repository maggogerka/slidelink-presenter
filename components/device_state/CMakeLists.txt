idf_component_register(SRCS "device_state.c"
                       INCLUDE_DIRS "include"
                       REQUIRES command_router esp_system esp_timer presenter
                                usb_hid wifi_manager)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
