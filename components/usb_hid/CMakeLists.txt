idf_component_register(SRCS "usb_hid.c" "usb_descriptors.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_timer esp_tinyusb esp_system freertos)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
