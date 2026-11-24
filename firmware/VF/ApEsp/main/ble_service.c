#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "APESP_BLE";
static uint16_t s_value_handle;
static const ble_uuid128_t s_service_uuid =
    BLE_UUID128_INIT(0x91, 0x7d, 0x1c, 0x40, 0x43, 0x88, 0x4c, 0xa2,
                     0x9b, 0x2d, 0x1e, 0x6a, 0x22, 0x3f, 0x17, 0x01);
static const ble_uuid128_t s_alert_uuid =
    BLE_UUID128_INIT(0x91, 0x7d, 0x1c, 0x40, 0x43, 0x88, 0x4c, 0xa2,
                     0x9b, 0x2d, 0x1e, 0x6a, 0x22, 0x3f, 0x17, 0x02);

static int alert_access(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle; (void)attr_handle; (void)arg;
    const uint8_t alert[] = "LTR_STATUS";
    return os_mbuf_append(ctxt->om, alert, sizeof(alert) - 1U);
}

static const struct ble_gatt_svc_def s_services[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &s_service_uuid.u,
     .characteristics = (struct ble_gatt_chr_def[]) {
         {.uuid = &s_alert_uuid.u, .access_cb = alert_access,
          .val_handle = &s_value_handle,
          .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY},
         {0},
     }},
    {0},
};

static void ble_on_sync(void)
{
    uint8_t address_type;
    ble_hs_id_infer_auto(0, &address_type);
    ble_svc_gap_device_name_set("LTR-ApEsp");
    ESP_LOGI(TAG, "BLE GATT listo");
}

static void ble_host_task(void *arg)
{
    (void)arg;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_service_start(void)
{
    nimble_port_init();
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_gatts_count_cfg(s_services);
    ble_gatts_add_svcs(s_services);
    nimble_port_freertos_init(ble_host_task);
}
