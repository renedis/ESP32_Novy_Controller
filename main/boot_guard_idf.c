/* ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_app_desc.h"
#include "esp_image_format.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "applog.h"
#include "boot_guard.h"
#include "nvs_config.h"
#include "restart.h"
#include "boot_guard_idf.h"

static const char *TAG = "bootguard";

#define KEY "bootguard"

static bool                   s_nvs_ok;
static bg_record_t            s_rec;
static const esp_partition_t *s_running;
static uint32_t               s_addr;
static bool                   s_pending_verify;
static bool                   s_stable_done;
static bool                   s_proven;          /* record: this image was proven, now or before */
static bool                   s_confirmed;       /* this boot: proven and marked valid */
static bool                   s_switching;
static uint32_t               s_next_deadline_check;
static portMUX_TYPE           s_mux = portMUX_INITIALIZER_UNLOCKED;

static bool load(bg_record_t *r)
{
    nvs_handle_t h;
    size_t       len = sizeof(*r);
    if (nvs_open(NVS_CONFIG_NS, NVS_READONLY, &h) != ESP_OK) return false;
    esp_err_t err = nvs_get_blob(h, KEY, r, &len);
    nvs_close(h);
    return err == ESP_OK && len == sizeof(*r);
}

static void save(void)
{
    nvs_handle_t h;
    bg_record_t  copy;
    if (!s_nvs_ok) return;
    taskENTER_CRITICAL(&s_mux);
    copy = s_rec;
    taskEXIT_CRITICAL(&s_mux);
    if (nvs_open(NVS_CONFIG_NS, NVS_READWRITE, &h) != ESP_OK) return;
    if (nvs_set_blob(h, KEY, &copy, sizeof(copy)) == ESP_OK) nvs_commit(h);
    nvs_close(h);
}

/* bg_probe_fn: the other OTA slot, checked like the bootloader would (and its otadata state). */
static bool probe(void *ctx, bg_other_t *out)
{
    (void)ctx;
    const esp_partition_t *other = esp_ota_get_next_update_partition(NULL);
    if (!other || !s_running || other->address == s_running->address) return false;
    memset(out, 0, sizeof(*out));
    out->addr = other->address;
    esp_app_desc_t desc;
    if (esp_ota_get_partition_description(other, &desc) != ESP_OK) {
        ESP_LOGW(TAG, "%s: no app description", other->label);
        return true;
    }
    memcpy(out->sha, desc.app_elf_sha256, BG_SHA_LEN);
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(other, &st) == ESP_OK && (st == ESP_OTA_IMG_INVALID || st == ESP_OTA_IMG_ABORTED)) {
        ESP_LOGW(TAG, "%s: marked invalid in otadata", other->label);
        return true;
    }
    esp_partition_pos_t  pos = { .offset = other->address, .size = other->size };
    esp_image_metadata_t meta;
    if (esp_image_verify(ESP_IMAGE_VERIFY, &pos, &meta) != ESP_OK) {
        ESP_LOGW(TAG, "%s: image does not verify", other->label);
        return true;
    }
    ESP_LOGI(TAG, "%s: %s %s verifies", other->label, desc.project_name, desc.version);
    out->image_ok = true;
    return true;
}

static void switch_slot(const char *why)
{
    const esp_partition_t *other = esp_ota_get_next_update_partition(NULL);
    s_switching = true;
    save();
    esp_err_t err = other ? esp_ota_set_boot_partition(other) : ESP_ERR_NOT_FOUND;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Switching to the other slot failed: %s", esp_err_to_name(err));
        s_switching = false;
        return;
    }
    ESP_LOGE(TAG, "%s: booting %s", why, other->label);
}

void boot_guard_early(void)
{
    s_running = esp_ota_get_running_partition();
    s_addr    = s_running ? s_running->address : 0;
    esp_ota_img_states_t st;
    s_pending_verify = s_running && esp_ota_get_state_partition(s_running, &st) == ESP_OK &&
                       st == ESP_OTA_IMG_PENDING_VERIFY;

    esp_err_t err = nvs_flash_init();                  /* never erased: Arduino leftovers stay */
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS unavailable (%s); boot guard off", esp_err_to_name(err));
        return;
    }
    s_nvs_ok = true;
    if (!load(&s_rec)) bg_init(&s_rec);
    const uint8_t *sha = esp_app_get_description()->app_elf_sha256;
    bg_action_t    act = bg_on_boot(&s_rec, s_addr, sha, probe, NULL);
    bg_slot_t     *me  = bg_find(&s_rec, s_addr);
    s_proven = me && me->proven;
    if (act == BG_SWITCH) {
        switch_slot("Too many boots ending within 60 s");
        if (s_switching) esp_restart();
    }
    save();
    ESP_LOGI(TAG, "%s, boot %u of this image within %d s%s%s", s_running ? s_running->label : "?",
             me ? me->fast_boots : 0, BG_STABLE_S, s_proven ? ", proven" : "",
             s_pending_verify ? ", pending verify" : "");
}

/* Also when the record already says proven: the same image can come back in the same slot as a
 * fresh OTA (PENDING_VERIFY again), and the bootloader needs the mark every time. */
static void prove(const char *why)
{
    if (s_confirmed) return;
    s_confirmed = true;
    s_proven    = true;
    taskENTER_CRITICAL(&s_mux);
    bg_on_proven(&s_rec, s_addr);
    taskEXIT_CRITICAL(&s_mux);
    save();
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    applog("Firmware confirmed (%s)%s", why, err == ESP_OK ? "" : ", marking the app valid failed");
}

void boot_guard_tick(uint32_t uptime_s, bool has_ip)
{
    if (!s_nvs_ok) {                                    /* no record: still confirm for the bootloader */
        if (!s_confirmed && uptime_s >= BG_STABLE_S && has_ip) {
            s_confirmed = true;
            esp_ota_mark_app_valid_cancel_rollback();
            applog("Firmware confirmed (60 s up with WiFi), boot guard off: NVS unavailable");
        }
        return;
    }
    if (s_switching) return;
    if (!s_stable_done && uptime_s >= BG_STABLE_S) {
        s_stable_done = true;
        taskENTER_CRITICAL(&s_mux);
        bg_on_stable(&s_rec, s_addr);
        taskEXIT_CRITICAL(&s_mux);
        save();
    }
    if (!s_confirmed && uptime_s >= BG_STABLE_S && has_ip) prove("60 s up with WiFi");
    if (!s_proven && !has_ip && uptime_s >= BG_NET_DEADLINE_S && uptime_s >= s_next_deadline_check) {
        s_next_deadline_check = uptime_s + 60;          /* the probe verifies a whole image */
        if (bg_on_tick(&s_rec, s_addr, uptime_s, has_ip, probe, NULL) == BG_SWITCH) {
            applog("No WiFi for %d s on a new firmware, switching to the other slot", BG_NET_DEADLINE_S);
            switch_slot("No network");
            if (s_switching) restart_schedule("boot guard: no network on a new firmware");
        }
    }
}

void boot_guard_confirm(const char *why)
{
    if (s_nvs_ok && !s_switching) prove(why);
    else if (!s_nvs_ok) esp_ota_mark_app_valid_cancel_rollback();
}

void boot_guard_restart_hook(void)
{
    if (!s_nvs_ok || s_switching) return;
    taskENTER_CRITICAL(&s_mux);
    bg_on_stable(&s_rec, s_addr);
    taskEXIT_CRITICAL(&s_mux);
    save();
}

void boot_guard_get_info(boot_guard_info_t *out)
{
    memset(out, 0, sizeof(*out));
    out->nvs_ok         = s_nvs_ok;
    out->pending_verify = s_pending_verify;
    taskENTER_CRITICAL(&s_mux);
    bg_slot_t *me = bg_find(&s_rec, s_addr);
    out->fast_boots = me ? me->fast_boots : 0;
    out->proven     = me && me->proven;
    taskEXIT_CRITICAL(&s_mux);
    if (s_running) strlcpy(out->running, s_running->label, sizeof(out->running));
    const esp_partition_t *other = esp_ota_get_next_update_partition(NULL);
    if (other) {
        esp_app_desc_t desc;
        strlcpy(out->other, other->label, sizeof(out->other));
        if (esp_ota_get_partition_description(other, &desc) == ESP_OK) {
            strlcpy(out->other_project, desc.project_name, sizeof(out->other_project));
            strlcpy(out->other_version, desc.version, sizeof(out->other_version));
        }
    }
}
