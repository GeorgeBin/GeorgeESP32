#include "ble_ancs_manager.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include "ancs_notification_parser.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "system_status.h"
#include "notification_filter.h"
#include "notification_rules.h"

#define ANCS_DEVICE_NAME "George_ANCS"
#define ANCS_MAX_DATA_SOURCE_LEN 1024
#define ANCS_ATTR_TITLE_MAX_LEN 128
#define ANCS_ATTR_SUBTITLE_MAX_LEN 128
#define ANCS_ATTR_MESSAGE_MAX_LEN 256
#define ANCS_HID_SERVICE_UUID 0x1812
#define ANCS_GENERIC_HID_APPEARANCE 0x03C0
#define ANCS_PENDING_SOURCE_COUNT 4

#define ANCS_COMMAND_GET_NOTIFICATION_ATTRIBUTES 0
#define ANCS_ATTRIBUTE_APP_IDENTIFIER 0
#define ANCS_ATTRIBUTE_TITLE 1
#define ANCS_ATTRIBUTE_SUBTITLE 2
#define ANCS_ATTRIBUTE_MESSAGE 3

typedef enum {
    ANCS_STATE_IDLE = 0,
    ANCS_STATE_BLE_INIT,
    ANCS_STATE_ADVERTISING,
    ANCS_STATE_CONNECTED,
    ANCS_STATE_BONDED,
    ANCS_STATE_SERVICE_DISCOVERED,
    ANCS_STATE_NOTIFICATION_SOURCE_SUBSCRIBED,
    ANCS_STATE_DATA_SOURCE_SUBSCRIBED,
    ANCS_STATE_READY,
    ANCS_STATE_DISCONNECTED,
    ANCS_STATE_ERROR,
} ancs_state_t;

typedef enum {
    ANCS_SUBSCRIBE_NOTIFICATION_SOURCE = 0,
    ANCS_SUBSCRIBE_DATA_SOURCE,
} ancs_subscribe_target_t;

typedef struct {
    uint8_t attribute_id;
    uint16_t max_len;
} ancs_attribute_request_t;

static const char *TAG = "ble_ancs";

static const ble_uuid128_t APPLE_NC_UUID = BLE_UUID128_INIT(
    0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4,
    0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79);
static const ble_uuid128_t NOTIFICATION_SOURCE_UUID = BLE_UUID128_INIT(
    0xbd, 0x1d, 0xa2, 0x99, 0xe6, 0x25, 0x58, 0x8c,
    0xd9, 0x42, 0x01, 0x63, 0x0d, 0x12, 0xbf, 0x9f);
static const ble_uuid128_t CONTROL_POINT_UUID = BLE_UUID128_INIT(
    0xd9, 0xd9, 0xaa, 0xfd, 0xbd, 0x9b, 0x21, 0x98,
    0xa8, 0x49, 0xe1, 0x45, 0xf3, 0xd8, 0xd1, 0x69);
static const ble_uuid128_t DATA_SOURCE_UUID = BLE_UUID128_INIT(
    0xfb, 0x7b, 0x7c, 0xce, 0x6a, 0xb3, 0x44, 0xbe,
    0xb5, 0x4b, 0xd6, 0x24, 0xe9, 0xc6, 0xea, 0x22);

static uint8_t s_own_addr_type;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_service_end_handle;
static uint16_t s_notification_source_handle;
static uint16_t s_data_source_handle;
static uint16_t s_control_point_handle;
static uint16_t s_mtu_size = 23;
static uint8_t s_data_source_buffer[ANCS_MAX_DATA_SOURCE_LEN];
static uint16_t s_data_source_len;
static esp_timer_handle_t s_data_flush_timer;
static bool s_started;
static ancs_source_event_t s_pending_source_events[ANCS_PENDING_SOURCE_COUNT];
static size_t s_next_pending_source_index;
static const ble_uuid16_t HID_SERVICE_UUID = BLE_UUID16_INIT(ANCS_HID_SERVICE_UUID);

static int ble_ancs_gap_event(struct ble_gap_event *event, void *arg);
void ble_store_config_init(void);

static const ancs_attribute_request_t ATTRIBUTE_REQUESTS[] = {
    {ANCS_ATTRIBUTE_APP_IDENTIFIER, 0},
    {ANCS_ATTRIBUTE_TITLE, ANCS_ATTR_TITLE_MAX_LEN},
    {ANCS_ATTRIBUTE_SUBTITLE, ANCS_ATTR_SUBTITLE_MAX_LEN},
    {ANCS_ATTRIBUTE_MESSAGE, ANCS_ATTR_MESSAGE_MAX_LEN},
};

static const char *ancs_state_to_string(ancs_state_t state)
{
    switch (state) {
    case ANCS_STATE_IDLE:
        return "IDLE";
    case ANCS_STATE_BLE_INIT:
        return "BLE_INIT";
    case ANCS_STATE_ADVERTISING:
        return "ADVERTISING";
    case ANCS_STATE_CONNECTED:
        return "CONNECTED";
    case ANCS_STATE_BONDED:
        return "BONDED";
    case ANCS_STATE_SERVICE_DISCOVERED:
        return "SERVICE_DISCOVERED";
    case ANCS_STATE_NOTIFICATION_SOURCE_SUBSCRIBED:
        return "NOTIFICATION_SOURCE_SUBSCRIBED";
    case ANCS_STATE_DATA_SOURCE_SUBSCRIBED:
        return "DATA_SOURCE_SUBSCRIBED";
    case ANCS_STATE_READY:
        return "READY";
    case ANCS_STATE_DISCONNECTED:
        return "DISCONNECTED";
    case ANCS_STATE_ERROR:
    default:
        return "ERROR";
    }
}

static void set_ancs_state(ancs_state_t state)
{
    ESP_LOGI(TAG, "[ANCS] state: %s", ancs_state_to_string(state));
    if (state == ANCS_STATE_READY) {
        system_status_set_ancs_connected(true);
    } else if (state == ANCS_STATE_DISCONNECTED || state == ANCS_STATE_ERROR) {
        system_status_set_ancs_connected(false);
    }
}

static esp_err_t init_nvs_once(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase failed");
        ret = nvs_flash_init();
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    return ret;
}

static void advertise(void)
{
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields = {0};
    struct ble_hs_adv_fields rsp_fields = {0};
    const char *name = ble_svc_gap_device_name();

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.uuids16 = &HID_SERVICE_UUID;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;
    fields.appearance = ANCS_GENERIC_HID_APPEARANCE;
    fields.appearance_is_present = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "set advertising fields failed: %d", rc);
        set_ancs_state(ANCS_STATE_ERROR);
        return;
    }

    rsp_fields.name = (const uint8_t *)name;
    rsp_fields.name_len = strlen(name);
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "set scan response fields failed: %d", rc);
        set_ancs_state(ANCS_STATE_ERROR);
        return;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           ble_ancs_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "start advertising failed: %d", rc);
        set_ancs_state(ANCS_STATE_ERROR);
        return;
    }

    system_status_set_ble(true, false);
    set_ancs_state(ANCS_STATE_ADVERTISING);
    ESP_LOGI(TAG, "[BLE] advertising started as %s", name);
}

static int subscribe_complete_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                 struct ble_gatt_attr *attr, void *arg)
{
    (void)conn_handle;
    (void)attr;

    const ancs_subscribe_target_t target = (ancs_subscribe_target_t)(uintptr_t)arg;
    if (error->status != 0) {
        ESP_LOGE(TAG, "subscribe failed: status=%d", error->status);
        set_ancs_state(ANCS_STATE_ERROR);
        return error->status;
    }

    if (target == ANCS_SUBSCRIBE_NOTIFICATION_SOURCE) {
        ESP_LOGI(TAG, "[ANCS] notification source subscribed");
        set_ancs_state(ANCS_STATE_NOTIFICATION_SOURCE_SUBSCRIBED);
    } else {
        ESP_LOGI(TAG, "[ANCS] data source subscribed");
        set_ancs_state(ANCS_STATE_DATA_SOURCE_SUBSCRIBED);
        set_ancs_state(ANCS_STATE_READY);
    }

    return 0;
}

static int descriptor_discovered_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                    uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc,
                                    void *arg)
{
    const ancs_subscribe_target_t target = (ancs_subscribe_target_t)(uintptr_t)arg;
    const uint8_t cccd_value[2] = {0x01, 0x00};

    (void)chr_val_handle;

    if (error->status != 0) {
        if (error->status == BLE_HS_EDONE) {
            return 0;
        }
        ESP_LOGE(TAG, "descriptor discovery failed: status=%d", error->status);
        set_ancs_state(ANCS_STATE_ERROR);
        return error->status;
    }

    if (ble_uuid_cmp(&dsc->uuid.u, BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16)) != 0) {
        return 0;
    }

    return ble_gattc_write_flat(conn_handle, dsc->handle, cccd_value, sizeof(cccd_value),
                                subscribe_complete_cb, (void *)(uintptr_t)target);
}

static int characteristic_discovered_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                        const struct ble_gatt_chr *chr, void *arg)
{
    (void)arg;

    if (error->status != 0) {
        if (error->status == BLE_HS_EDONE) {
            if (s_notification_source_handle == 0 || s_data_source_handle == 0 ||
                s_control_point_handle == 0) {
                ESP_LOGE(TAG, "ANCS characteristics missing: ns=%u ds=%u cp=%u",
                         s_notification_source_handle, s_data_source_handle,
                         s_control_point_handle);
                set_ancs_state(ANCS_STATE_ERROR);
            }
            return 0;
        }
        ESP_LOGE(TAG, "characteristic discovery failed: status=%d", error->status);
        set_ancs_state(ANCS_STATE_ERROR);
        return error->status;
    }

    if ((chr->properties & BLE_GATT_CHR_PROP_NOTIFY) &&
        ble_uuid_cmp(&chr->uuid.u, &NOTIFICATION_SOURCE_UUID.u) == 0) {
        s_notification_source_handle = chr->val_handle;
        ESP_LOGI(TAG, "[ANCS] notification source found");
        return ble_gattc_disc_all_dscs(conn_handle, chr->val_handle, s_service_end_handle,
                                       descriptor_discovered_cb,
                                       (void *)(uintptr_t)ANCS_SUBSCRIBE_NOTIFICATION_SOURCE);
    }
    if ((chr->properties & BLE_GATT_CHR_PROP_NOTIFY) &&
        ble_uuid_cmp(&chr->uuid.u, &DATA_SOURCE_UUID.u) == 0) {
        s_data_source_handle = chr->val_handle;
        ESP_LOGI(TAG, "[ANCS] data source found");
        return ble_gattc_disc_all_dscs(conn_handle, chr->val_handle, s_service_end_handle,
                                       descriptor_discovered_cb,
                                       (void *)(uintptr_t)ANCS_SUBSCRIBE_DATA_SOURCE);
    }
    if ((chr->properties & BLE_GATT_CHR_PROP_WRITE) &&
        ble_uuid_cmp(&chr->uuid.u, &CONTROL_POINT_UUID.u) == 0) {
        s_control_point_handle = chr->val_handle;
        ESP_LOGI(TAG, "[ANCS] control point found");
    }

    return 0;
}

static int service_discovered_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                 const struct ble_gatt_svc *svc, void *arg)
{
    (void)arg;

    if (error->status != 0) {
        if (error->status == BLE_HS_EDONE) {
            ESP_LOGW(TAG, "ANCS service not found");
            return 0;
        }
        ESP_LOGE(TAG, "service discovery failed: status=%d", error->status);
        set_ancs_state(ANCS_STATE_ERROR);
        return error->status;
    }
    if (svc == NULL) {
        ESP_LOGW(TAG, "ANCS service not found");
        return 0;
    }

    s_service_end_handle = svc->end_handle;
    set_ancs_state(ANCS_STATE_SERVICE_DISCOVERED);
    ESP_LOGI(TAG, "[ANCS] service discovered");
    return ble_gattc_disc_all_chrs(conn_handle, svc->start_handle, svc->end_handle,
                                   characteristic_discovered_cb, NULL);
}

static void request_notification_attributes(uint32_t notification_uid)
{
    uint8_t command[32] = {0};
    size_t offset = 0;

    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || s_control_point_handle == 0) {
        ESP_LOGW(TAG, "cannot request notification attributes before ANCS is ready");
        return;
    }

    command[offset++] = ANCS_COMMAND_GET_NOTIFICATION_ATTRIBUTES;
    command[offset++] = (uint8_t)(notification_uid & 0xFF);
    command[offset++] = (uint8_t)((notification_uid >> 8) & 0xFF);
    command[offset++] = (uint8_t)((notification_uid >> 16) & 0xFF);
    command[offset++] = (uint8_t)((notification_uid >> 24) & 0xFF);

    for (size_t i = 0; i < sizeof(ATTRIBUTE_REQUESTS) / sizeof(ATTRIBUTE_REQUESTS[0]); ++i) {
        command[offset++] = ATTRIBUTE_REQUESTS[i].attribute_id;
        if (ATTRIBUTE_REQUESTS[i].max_len > 0) {
            command[offset++] = (uint8_t)(ATTRIBUTE_REQUESTS[i].max_len & 0xFF);
            command[offset++] = (uint8_t)((ATTRIBUTE_REQUESTS[i].max_len >> 8) & 0xFF);
        }
    }

    const int rc = ble_gattc_write_flat(s_conn_handle, s_control_point_handle,
                                        command, offset, NULL, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "write notification attribute request failed: rc=%d", rc);
    }
}

static void handle_notification_source(const uint8_t *data, size_t len)
{
    ancs_source_event_t source_event = {0};

    if (!ancs_parse_notification_source(data, len, &source_event)) {
        ESP_LOGW(TAG, "parse notification source failed");
        return;
    }

    ESP_LOGI(TAG, "[ANCS] source event: uid=%" PRIu32 " event=%s category=%u",
             source_event.notification_uid,
             ancs_event_action_to_string(source_event.action),
             source_event.category_id);

    if (source_event.action == ANCS_EVENT_ADDED) {
        s_pending_source_events[s_next_pending_source_index] = source_event;
        s_next_pending_source_index =
            (s_next_pending_source_index + 1) % ANCS_PENDING_SOURCE_COUNT;
        request_notification_attributes(source_event.notification_uid);
    }
}

static bool pop_pending_source_event(uint32_t notification_uid, ancs_source_event_t *source_event)
{
    for (size_t i = 0; i < ANCS_PENDING_SOURCE_COUNT; ++i) {
        if (s_pending_source_events[i].notification_uid == notification_uid) {
            if (source_event != NULL) {
                *source_event = s_pending_source_events[i];
            }
            memset(&s_pending_source_events[i], 0, sizeof(s_pending_source_events[i]));
            return true;
        }
    }
    return false;
}

static void handle_data_source_packet(const uint8_t *data, size_t len)
{
    ancs_notification_event_t event = {0};
    ancs_source_event_t source_event = {0};

    if (!ancs_parse_notification_attributes(data, len, &event)) {
        ESP_LOGW(TAG, "parse notification attributes failed");
        return;
    }

    if (pop_pending_source_event(event.notification_uid, &source_event)) {
        event.category_id = source_event.category_id;
        event.category_count = source_event.category_count;
        event.event_flags = source_event.event_flags;
        event.action = source_event.action;
    }

    led_notify_type_t type = notification_filter_map_to_led_type(&event);
    ESP_LOGI(TAG, "[ANCS] detail: app_id=%s title=%s subtitle=%s message=%s",
             event.app_id, event.title, event.subtitle, event.message);
    ESP_LOGI(TAG, "[FILTER] type=%s", notification_filter_type_to_string(type));
    system_status_set_ancs_notification(&event);
    if (notification_rules_apply_event(&event) == ESP_OK) {
        ESP_LOGI(TAG, "[LED] applied notification rule");
    }
}

static void flush_data_source_timer_cb(void *arg)
{
    (void)arg;
    esp_timer_stop(s_data_flush_timer);
    if (s_data_source_len > 0) {
        handle_data_source_packet(s_data_source_buffer, s_data_source_len);
        memset(s_data_source_buffer, 0, s_data_source_len);
        s_data_source_len = 0;
    }
}

static void append_data_source(const uint8_t *data, size_t len)
{
    if ((s_data_source_len + len) > sizeof(s_data_source_buffer)) {
        ESP_LOGE(TAG, "data source buffer overflow");
        s_data_source_len = 0;
        return;
    }

    memcpy(&s_data_source_buffer[s_data_source_len], data, len);
    s_data_source_len += len;

    if (len == (size_t)(s_mtu_size - 3)) {
        esp_timer_stop(s_data_flush_timer);
        esp_timer_start_once(s_data_flush_timer, 500000);
    } else {
        esp_timer_stop(s_data_flush_timer);
        handle_data_source_packet(s_data_source_buffer, s_data_source_len);
        memset(s_data_source_buffer, 0, s_data_source_len);
        s_data_source_len = 0;
    }
}

static void handle_notification_rx(const struct ble_gap_event *event)
{
    uint16_t packet_len = OS_MBUF_PKTLEN(event->notify_rx.om);
    uint8_t buffer[ANCS_MAX_DATA_SOURCE_LEN];

    if (packet_len > sizeof(buffer)) {
        ESP_LOGW(TAG, "notification packet too large: %u", packet_len);
        return;
    }
    if (os_mbuf_copydata(event->notify_rx.om, 0, packet_len, buffer) != 0) {
        ESP_LOGW(TAG, "failed to copy notification packet");
        return;
    }

    if (event->notify_rx.attr_handle == s_notification_source_handle) {
        handle_notification_source(buffer, packet_len);
    } else if (event->notify_rx.attr_handle == s_data_source_handle) {
        append_data_source(buffer, packet_len);
    } else {
        ESP_LOGI(TAG, "notification from unknown handle=%u", event->notify_rx.attr_handle);
    }
}

static int ble_ancs_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            ESP_LOGW(TAG, "[BLE] connection failed: status=%d", event->connect.status);
            advertise();
            return 0;
        }

        s_conn_handle = event->connect.conn_handle;
        system_status_set_ble(true, true);
        set_ancs_state(ANCS_STATE_CONNECTED);
        ESP_LOGI(TAG, "[BLE] connected");
        int rc = ble_gap_security_initiate(event->connect.conn_handle);
        if (rc != 0) {
            ESP_LOGW(TAG, "security initiate failed: rc=%d", rc);
            return ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "[BLE] disconnected: reason=%d", event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_notification_source_handle = 0;
        s_data_source_handle = 0;
        s_control_point_handle = 0;
        s_data_source_len = 0;
        memset(s_pending_source_events, 0, sizeof(s_pending_source_events));
        s_next_pending_source_index = 0;
        system_status_set_ble(true, false);
        set_ancs_state(ANCS_STATE_DISCONNECTED);
        advertise();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        if (event->enc_change.status != 0) {
            ESP_LOGW(TAG, "encryption failed: status=%d", event->enc_change.status);
            set_ancs_state(ANCS_STATE_ERROR);
            return 0;
        }
        set_ancs_state(ANCS_STATE_BONDED);
        ESP_LOGI(TAG, "[BLE] bonded");
        return ble_gattc_disc_svc_by_uuid(event->enc_change.conn_handle, &APPLE_NC_UUID.u,
                                          service_discovered_cb, NULL);

    case BLE_GAP_EVENT_NOTIFY_RX:
        handle_notification_rx(event);
        return 0;

    case BLE_GAP_EVENT_MTU:
        s_mtu_size = event->mtu.value;
        ESP_LOGI(TAG, "MTU updated: %u", s_mtu_size);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
    {
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    default:
        return 0;
    }
}

static void ble_ancs_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE host reset: %d", reason);
    set_ancs_state(ANCS_STATE_ERROR);
}

static void ble_ancs_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ensure BLE address failed: %d", rc);
        set_ancs_state(ANCS_STATE_ERROR);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "infer BLE address failed: %d", rc);
        set_ancs_state(ANCS_STATE_ERROR);
        return;
    }

    ESP_LOGI(TAG, "[BLE] host synced; starting advertising");
    advertise();
}

static void ble_ancs_host_task(void *param)
{
    (void)param;
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_ancs_manager_start(void)
{
    const esp_timer_create_args_t timer_args = {
        .callback = flush_data_source_timer_cb,
        .name = "ancs_flush",
    };

    if (s_started) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(init_nvs_once(), TAG, "nvs init failed");
    set_ancs_state(ANCS_STATE_BLE_INIT);

    esp_err_t ret = nimble_port_init();
    ESP_RETURN_ON_ERROR(ret, TAG, "nimble init failed");

    ble_hs_cfg.reset_cb = ble_ancs_on_reset;
    ble_hs_cfg.sync_cb = ble_ancs_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_sc = 0;

    int rc = ble_svc_gap_device_name_set(ANCS_DEVICE_NAME);
    ESP_RETURN_ON_FALSE(rc == 0, ESP_FAIL, TAG, "set device name failed: %d", rc);
    ble_store_config_init();

    ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &s_data_flush_timer), TAG,
                        "create ANCS flush timer failed");

    system_status_set_ble(true, false);
    system_status_set_ancs_connected(false);
    nimble_port_freertos_init(ble_ancs_host_task);
    s_started = true;
    ESP_LOGI(TAG, "[BLE] init ok");
    return ESP_OK;
}
