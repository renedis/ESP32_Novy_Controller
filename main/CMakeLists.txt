# ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0
idf_component_register(
    SRCS
        "main.c"
        "rcswitch.c"
        "rf_wave.c"
        "rf_cmd.c"
        "devstate.c"
        "discovery.c"
        "light.c"
        "led_logic.c"
        "backoff.c"
        "evlog.c"
        "status_fmt.c"
        "boot_guard.c"
        "net_config.c"
        "config_validate.c"
        "applog.c"
        "nvs_config.c"
        "restart.c"
        "time_sync.c"
        "wifi_mgr.c"
        "boot_guard_idf.c"
        "rf_tx.c"
        "status_led.c"
        "mqtt_pub.c"
        "web_server.c"
        "peers.c"
        "peers_web.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES
        "web/index.html"
        "web/config.html"
        "web/peers.js"
)

# RF codes: .local/rf_codes.h (git-ignored, your hood channel and fan remote) when it exists,
# otherwise the made-up values of main/rf_codes_example.h. Run idf.py reconfigure after adding it.
get_filename_component(rf_codes "${CMAKE_CURRENT_LIST_DIR}/../.local/rf_codes.h" ABSOLUTE)
if(EXISTS "${rf_codes}")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE RF_CODES_H="${rf_codes}")
    message(STATUS "RF codes: .local/rf_codes.h")
else()
    message(STATUS "RF codes: main/rf_codes_example.h (example values, see README)")
endif()
