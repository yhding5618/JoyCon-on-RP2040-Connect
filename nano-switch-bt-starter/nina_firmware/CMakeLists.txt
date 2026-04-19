cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(nano_switch_nina)

if(NOT IDF_TARGET STREQUAL "esp32")
    message(FATAL_ERROR
        "This project targets the Nano RP2040 Connect NINA-W102, which is an ESP32. "
        "Run 'idf.py set-target esp32' in the nina_firmware directory, then rebuild.")
endif()
