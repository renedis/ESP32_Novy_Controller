# ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0
# QEMU test app: the firmware's boot guard (main/boot_guard*.c) on the real partition table,
# without WiFi (QEMU cannot run WiFi apps on the C3). See ../README.md.
cmake_minimum_required(VERSION 3.16)
set(PROJECT_VER "bg-test")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(boot-guard-qemu)
