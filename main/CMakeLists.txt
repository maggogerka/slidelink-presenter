idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES command_router esp_app_format nvs_flash presenter
                                presenter_button presenter_console profile_store
                                session_manager usb_hid web_server wifi_manager)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
