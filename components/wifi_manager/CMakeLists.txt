idf_component_register(SRCS "wifi_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_event esp_netif esp_wifi mdns session_manager)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
