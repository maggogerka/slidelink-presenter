cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS
    "../components/command_router"
    "../components/presenter"
    "../components/presenter_console"
    "components/usb_hid")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(slidelink_tests)
