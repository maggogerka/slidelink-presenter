idf_component_register(SRCS "presenter_button.c"
                       INCLUDE_DIRS "include"
                       REQUIRES command_router esp_driver_gpio esp_timer)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
