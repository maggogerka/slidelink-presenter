idf_component_register(SRCS "profile_store.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_hw_support freertos nvs_flash presenter)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
