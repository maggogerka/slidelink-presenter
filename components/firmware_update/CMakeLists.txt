idf_component_register(SRCS "firmware_update.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_update esp_app_format esp_partition)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
