# ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0
cmake_minimum_required(VERSION 3.16)
set(PROJECT_VER "3.0.0")
# Optional, git-ignored overrides, read when a fresh sdkconfig is generated (idf.py set-target,
# or after deleting sdkconfig):
#   sdkconfig.secrets       WiFi, MQTT and setup AP password (tools/make_secrets.py)
#   .local/sdkconfig.local  your own device name, setup AP SSID, static IP defaults, ...
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
foreach(extra "sdkconfig.secrets" ".local/sdkconfig.local")
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/${extra}")
        list(APPEND SDKCONFIG_DEFAULTS "${extra}")
    endif()
endforeach()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-novy-rf)
