idf_component_register(SRCS "presenter.c"
                       INCLUDE_DIRS "include"
                       REQUIRES usb_hid)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
