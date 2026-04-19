file(TO_CMAKE_PATH "$ENV{IDF_PATH}/components/bt/include/esp32/include" NANO_SWITCH_BT_ESP32_INCLUDE)
file(TO_CMAKE_PATH "$ENV{IDF_PATH}/components/bt/host/bluedroid/api/include/api" NANO_SWITCH_BT_BLUEDROID_INCLUDE)

set(NANO_SWITCH_BT_INCLUDE_DIRS
    "."
    "../../shared/include"
    "${NANO_SWITCH_BT_ESP32_INCLUDE}"
    "${NANO_SWITCH_BT_BLUEDROID_INCLUDE}"
)

idf_component_register(
    SRCS
        "app_main.c"
        "bridge_uart.c"
        "switch_hid.c"
    INCLUDE_DIRS
        ${NANO_SWITCH_BT_INCLUDE_DIRS}
    REQUIRES
        bt
        driver
        nvs_flash
)
