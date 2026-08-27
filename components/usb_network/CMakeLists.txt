idf_component_register(SRCS "usb_network.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_netif esp_system esp_tinyusb freertos lwip)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
