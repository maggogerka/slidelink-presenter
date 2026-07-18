idf_component_register(SRCS "command_router.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_timer freertos presenter usb_hid)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
