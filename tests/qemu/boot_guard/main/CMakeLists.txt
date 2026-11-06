# ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0
idf_component_register(
    SRCS
        "test_main.c"
        "../../../../main/boot_guard.c"
        "../../../../main/boot_guard_idf.c"
    INCLUDE_DIRS "." "../../../../main"
    PRIV_REQUIRES nvs_flash app_update bootloader_support esp_app_format
)
