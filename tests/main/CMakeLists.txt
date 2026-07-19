idf_component_register(SRCS "test_main.c"
                            "test_command_parser.c"
                            "test_presenter_mapping.c"
                            "test_presenter_profile.c"
                            "test_command_router.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES unity presenter presenter_console command_router usb_hid
                       WHOLE_ARCHIVE)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
