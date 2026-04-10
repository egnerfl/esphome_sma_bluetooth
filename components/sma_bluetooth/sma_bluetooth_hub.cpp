/* MIT License — based on work by Lupo135, darrylb123, keerekeerweere */

#include "sma_bluetooth_hub.h"
#include "sma_inverter_device.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esp_idf_version.h"
#include "nvs_flash.h"
#include "nvs.h"

#include <algorithm>

namespace esphome {
namespace sma_bluetooth {

static const char *const TAG = "sma_bluetooth";

SmaBluetoothHub *SmaBluetoothHub::instance_ = nullptr;

// ============================================================
//  ESPHome lifecycle
// ============================================================

void SmaBluetoothHub::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SMA Bluetooth Hub...");
  instance_ = this;

  // Default passwords if none configured
  if (passwords_.empty()) {
    passwords_.push_back("0000");
    passwords_.push_back("1111");
  }

  // Restore cached inverter MACs from NVS (for autodiscovery persistence across reboots)
  restore_cached_inverters_();

  // Pre-create devices + sensors for all known inverters (configured or cached)
  // so they are registered BEFORE the API sends list_entities to HA.
  pre_create_devices_();

  if (!init_bt_stack()) {
    ESP_LOGE(TAG, "BT stack init failed");
    mark_failed();
    return;
  }

  bt_initialized_ = true;

  // Start the FreeRTOS BT task on core 0
  xTaskCreatePinnedToCore(bt_task_entry, "sma_bt", 8192, this, 2,
                          (TaskHandle_t *)&bt_task_handle_, 0);
  ESP_LOGI(TAG, "BT task launched");
}

void SmaBluetoothHub::loop() {
  App.feed_wdt();

  // Publish sensor data from any device that has fresh data
  bool new_sensors_created = false;
  for (auto *device : devices_) {
    if (device->is_data_fresh()) {
      if (device->publish_sensors()) {
        new_sensors_created = true;
      }
      device->clear_data_fresh();
    }
  }

  // If new auto-sensors were just created at runtime, wait for the BT task to
  // finish one full poll cycle before rebooting. This minimizes reboot count by
  // letting all discovered inverters create their sensors in one pass.
  if (new_sensors_created) {
    reboot_pending_ = true;
    poll_cycle_complete_ = false;  // clear stale signal from previous cycle
    ESP_LOGI(TAG, "New sensors created — will reboot after current poll cycle completes...");
  }

  if (reboot_pending_ && poll_cycle_complete_) {
    poll_cycle_complete_ = false;
    reboot_pending_ = false;
    save_cached_inverters_();
    ESP_LOGW(TAG, "Poll cycle complete — rebooting in 3s so Home Assistant discovers all sensors...");
    this->set_timeout("reboot_for_sensors", 3000, []() {
      App.safe_reboot();
    });
  }

  // Monitor task health
  if (task_error_) {
    ESP_LOGE(TAG, "BT task error detected, restarting...");
    task_error_ = false;
    stop_task_ = true;
    vTaskDelay(pdMS_TO_TICKS(100));
    stop_task_ = false;
    xTaskCreatePinnedToCore(bt_task_entry, "sma_bt", 8192, this, 2,
                            (TaskHandle_t *)&bt_task_handle_, 0);
  }
}

void SmaBluetoothHub::dump_config() {
  ESP_LOGCONFIG(TAG, "SMA Bluetooth Hub:");
  ESP_LOGCONFIG(TAG, "  Discovery: %s", discovery_enabled_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Query delay: %u ms", query_delay_ms_);
  ESP_LOGCONFIG(TAG, "  Inter-inverter delay: %u ms", inter_inverter_delay_ms_);
  ESP_LOGCONFIG(TAG, "  Default passwords: %d configured", (int)passwords_.size());
  ESP_LOGCONFIG(TAG, "  Inverter configs: %d", (int)inverter_configs_.size());
  ESP_LOGCONFIG(TAG, "  Registered devices: %d", (int)devices_.size());
  ESP_LOGCONFIG(TAG, "  Free heap: %u bytes", esp_get_free_heap_size());
}

void SmaBluetoothHub::register_device(SmaInverterDevice *device) {
  devices_.push_back(device);
}

// ============================================================
//  BT stack initialization
// ============================================================

bool SmaBluetoothHub::init_bt_stack() {
  rx_stream_buf_ = xStreamBufferCreate(4096, 1);
  bt_event_group_ = xEventGroupCreate();
  if (!rx_stream_buf_ || !bt_event_group_) {
    ESP_LOGE(TAG, "Failed to create FreeRTOS primitives");
    return false;
  }

  // Release BLE memory — we only need Classic BT
  esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "mem_release BLE: %s", esp_err_to_name(ret));
  }

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ret = esp_bt_controller_init(&bt_cfg);
  if (ret != ESP_OK) { ESP_LOGE(TAG, "controller_init: %s", esp_err_to_name(ret)); return false; }

  ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
  if (ret != ESP_OK) { ESP_LOGE(TAG, "controller_enable: %s", esp_err_to_name(ret)); return false; }

  ret = esp_bluedroid_init();
  if (ret != ESP_OK) { ESP_LOGE(TAG, "bluedroid_init: %s", esp_err_to_name(ret)); return false; }

  ret = esp_bluedroid_enable();
  if (ret != ESP_OK) { ESP_LOGE(TAG, "bluedroid_enable: %s", esp_err_to_name(ret)); return false; }

  ret = esp_bt_gap_register_callback(gap_callback);
  if (ret != ESP_OK) { ESP_LOGE(TAG, "gap_register: %s", esp_err_to_name(ret)); return false; }

  ret = esp_spp_register_callback(spp_callback);
  if (ret != ESP_OK) { ESP_LOGE(TAG, "spp_register: %s", esp_err_to_name(ret)); return false; }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  esp_spp_cfg_t spp_cfg = {
      .mode = ESP_SPP_MODE_CB,
      .enable_l2cap_ertm = true,
      .tx_buffer_size = 0,
  };
  ret = esp_spp_enhanced_init(&spp_cfg);
#else
  ret = esp_spp_init(ESP_SPP_MODE_CB);
#endif
  if (ret != ESP_OK) { ESP_LOGE(TAG, "spp_init: %s", esp_err_to_name(ret)); return false; }

  const char pin[] = "0000";
  esp_bt_gap_set_device_name("ESP32toSMA");
  esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, 4, (uint8_t *)pin);
  esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);

  return true;
}

// ============================================================
//  Static BT callbacks
// ============================================================

void SmaBluetoothHub::spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  auto *self = instance_;
  if (!self) return;

  switch (event) {
    case ESP_SPP_INIT_EVT:
      ESP_LOGD(TAG, "SPP_INIT_EVT status=%d", param->init.status);
      xEventGroupSetBits(self->bt_event_group_, BT_EVT_SPP_INIT);
      break;

    case ESP_SPP_DISCOVERY_COMP_EVT:
      if (param->disc_comp.status == ESP_SPP_SUCCESS && param->disc_comp.scn_num > 0) {
        self->discovered_scn_ = param->disc_comp.scn[0];
        ESP_LOGD(TAG, "SPP discovery: SCN=%d", self->discovered_scn_);
      } else {
        self->discovered_scn_ = 1;  // fallback
        ESP_LOGW(TAG, "SPP discovery failed, using SCN=1");
      }
      xEventGroupSetBits(self->bt_event_group_, BT_EVT_DISC_DONE);
      break;

    case ESP_SPP_OPEN_EVT:
      if (param->open.status == ESP_SPP_SUCCESS) {
        self->spp_handle_ = param->open.handle;
        self->bt_connected_ = true;
        ESP_LOGD(TAG, "SPP connected, handle=%lu", (unsigned long)self->spp_handle_);
      } else {
        ESP_LOGE(TAG, "SPP open failed: %d", param->open.status);
        self->bt_connected_ = false;
      }
      xEventGroupSetBits(self->bt_event_group_, BT_EVT_CONNECTED);
      break;

    case ESP_SPP_CLOSE_EVT:
      self->bt_connected_ = false;
      self->spp_handle_ = 0;
      ESP_LOGD(TAG, "SPP disconnected");
      xEventGroupSetBits(self->bt_event_group_, BT_EVT_DISCONNECTED);
      break;

    case ESP_SPP_DATA_IND_EVT:
      if (param->data_ind.len > 0 && self->rx_stream_buf_) {
        xStreamBufferSend(self->rx_stream_buf_, param->data_ind.data,
                          param->data_ind.len, pdMS_TO_TICKS(100));
      }
      break;

    default:
      break;
  }
}

void SmaBluetoothHub::gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  auto *self = instance_;
  if (!self) return;

  switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
      // Device discovered during GAP inquiry
      DiscoveredDevice dev;
      memcpy(dev.mac, param->disc_res.bda, 6);

      // Extract device name from properties
      for (int i = 0; i < param->disc_res.num_prop; i++) {
        if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_BDNAME) {
          const char *name = (const char *)param->disc_res.prop[i].val;
          int len = param->disc_res.prop[i].len;
          dev.name = std::string(name, len);
          break;
        }
      }

      // Filter: only accept devices with "SMA" prefix
      if (dev.name.size() >= 3 &&
          (dev.name[0] == 'S' || dev.name[0] == 's') &&
          (dev.name[1] == 'M' || dev.name[1] == 'm') &&
          (dev.name[2] == 'A' || dev.name[2] == 'a')) {
        ESP_LOGD(TAG, "Discovered SMA device: %s [%02X:%02X:%02X:%02X:%02X:%02X]",
                 dev.name.c_str(),
                 dev.mac[0], dev.mac[1], dev.mac[2],
                 dev.mac[3], dev.mac[4], dev.mac[5]);
        self->discovered_.push_back(dev);
      }
      break;
    }

    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
      if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
        ESP_LOGD(TAG, "GAP discovery completed, found %d SMA device(s)",
                 (int)self->discovered_.size());
        self->discovery_complete_ = true;
      }
      break;

    case ESP_BT_GAP_AUTH_CMPL_EVT:
      if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
        ESP_LOGD(TAG, "BT auth success: %s", param->auth_cmpl.device_name);
      } else {
        ESP_LOGW(TAG, "BT auth failed: status=%d", param->auth_cmpl.stat);
      }
      break;

    case ESP_BT_GAP_PIN_REQ_EVT: {
      esp_bt_pin_code_t pin = {'0', '0', '0', '0'};
      esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin);
      break;
    }

    default:
      break;
  }
}

// ============================================================
//  BT I/O methods (called from device protocol methods)
// ============================================================

void SmaBluetoothHub::bt_send_packet(uint8_t *data, uint16_t len) {
  if (spp_handle_ == 0) {
    ESP_LOGW(TAG, "bt_send_packet: not connected");
    return;
  }
  esp_err_t err = esp_spp_write((uint32_t)spp_handle_, len, data);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_spp_write: %s", esp_err_to_name(err));
  }
}

uint8_t SmaBluetoothHub::bt_get_byte(uint32_t timeout_ms) {
  uint8_t byte = 0;
  size_t received = xStreamBufferReceive(rx_stream_buf_, &byte, 1,
                                          pdMS_TO_TICKS(timeout_ms));
  if (received == 0) {
    read_timeout_ = true;
    return 0;
  }
  read_timeout_ = false;
  return byte;
}

void SmaBluetoothHub::bt_flush_rx() {
  if (!rx_stream_buf_) return;
  uint8_t tmp[64];
  while (xStreamBufferReceive(rx_stream_buf_, tmp, sizeof(tmp),
                               pdMS_TO_TICKS(10)) > 0) {
    // discard
  }
}

// ============================================================
//  SPP connection management
// ============================================================

bool SmaBluetoothHub::spp_connect(const uint8_t mac[6]) {
  xEventGroupClearBits(bt_event_group_, BT_EVT_DISC_DONE | BT_EVT_CONNECTED | BT_EVT_DISCONNECTED);
  bt_flush_rx();

  // Phase 1: SPP service discovery
  ESP_LOGD(TAG, "SPP discovery for %02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  esp_err_t err = esp_spp_start_discovery((uint8_t *)mac);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "spp_start_discovery: %s", esp_err_to_name(err));
    return false;
  }

  EventBits_t bits = xEventGroupWaitBits(bt_event_group_, BT_EVT_DISC_DONE,
                                          pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
  if (!(bits & BT_EVT_DISC_DONE)) {
    ESP_LOGE(TAG, "SPP discovery timeout");
    return false;
  }

  // Phase 2: SPP connect
  ESP_LOGD(TAG, "SPP connecting, SCN=%d", discovered_scn_);
  xEventGroupClearBits(bt_event_group_, BT_EVT_CONNECTED | BT_EVT_DISCONNECTED);

  err = esp_spp_connect(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_MASTER,
                         discovered_scn_, (uint8_t *)mac);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "spp_connect: %s", esp_err_to_name(err));
    return false;
  }

  bits = xEventGroupWaitBits(bt_event_group_, BT_EVT_CONNECTED | BT_EVT_DISCONNECTED,
                              pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));
  if (!(bits & BT_EVT_CONNECTED)) {
    ESP_LOGE(TAG, "SPP connect timeout");
    return false;
  }

  ESP_LOGD(TAG, "SPP connected");
  bt_flush_rx();
  return true;
}

void SmaBluetoothHub::spp_disconnect() {
  if (spp_handle_ != 0) {
    esp_spp_disconnect((uint32_t)spp_handle_);
    // Wait briefly for disconnect event
    xEventGroupWaitBits(bt_event_group_, BT_EVT_DISCONNECTED,
                         pdFALSE, pdFALSE, pdMS_TO_TICKS(3000));
  }
  bt_connected_ = false;
}

// ============================================================
//  Discovery
// ============================================================

void SmaBluetoothHub::run_discovery_scan() {
  discovered_.clear();
  discovery_complete_ = false;

  ESP_LOGD(TAG, "Starting GAP discovery scan...");
  esp_err_t err = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "gap_start_discovery: %s", esp_err_to_name(err));
    return;
  }

  // Wait for discovery to complete (max ~13s)
  uint32_t start = xTaskGetTickCount();
  while (!discovery_complete_ && !stop_task_) {
    vTaskDelay(pdMS_TO_TICKS(500));
    if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(15000)) {
      ESP_LOGD(TAG, "GAP discovery timeout, cancelling");
      esp_bt_gap_cancel_discovery();
      break;
    }
  }

  // Process discovered devices
  for (const auto &disc : discovered_) {
    // Check configured inverters list
    if (!inverter_configs_.empty() && !is_configured(disc.mac)) {
      ESP_LOGD(TAG, "Skipping %s (not in inverters list)", disc.name.c_str());
      continue;
    }

    // Check if already registered
    if (find_device_by_mac(disc.mac) != nullptr) {
      ESP_LOGD(TAG, "Device %s already registered", disc.name.c_str());
      continue;
    }

    // Create new device
    auto *dev = create_discovered_device(disc.mac, disc.name);
    if (dev) {
      ESP_LOGD(TAG, "Auto-registered device: %s", disc.name.c_str());
    }
  }
}

bool SmaBluetoothHub::is_configured(const uint8_t mac[6]) const {
  for (const auto &entry : inverter_configs_) {
    if (memcmp(entry.mac, mac, 6) == 0) return true;
  }
  return false;
}

const InverterConfig *SmaBluetoothHub::find_inverter_config(const uint8_t mac[6]) const {
  for (const auto &entry : inverter_configs_) {
    if (memcmp(entry.mac, mac, 6) == 0) return &entry;
  }
  return nullptr;
}

SmaInverterDevice *SmaBluetoothHub::find_device_by_mac(const uint8_t mac[6]) const {
  for (auto *dev : devices_) {
    if (memcmp(dev->bt_address(), mac, 6) == 0) return dev;
  }
  return nullptr;
}

SmaInverterDevice *SmaBluetoothHub::create_discovered_device(const uint8_t mac[6],
                                                               const std::string &bt_name) {
  auto *dev = new SmaInverterDevice();

  char mac_str[18];
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  dev->set_mac_address(std::string(mac_str));

  // Check inverter configs for per-inverter settings
  const auto *cfg = find_inverter_config(mac);
  if (cfg) {
    if (!cfg->password.empty()) {
      dev->set_password(cfg->password);
    }
    if (!cfg->name.empty()) {
      dev->set_name_prefix(cfg->name);
    }
  }

  // If no name prefix set from user config, leave it empty.
  // poll() will use serial number once it's known.

  register_device(dev);
  return dev;
}

void SmaBluetoothHub::trigger_discovery() {
  // Can be called from a button/lambda
  run_discovery_scan();
}

void SmaBluetoothHub::erase_nvs_cache() {
  nvs_handle_t handle;
  if (nvs_open("sma_bt", NVS_READWRITE, &handle) == ESP_OK) {
    nvs_erase_all(handle);
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGW(TAG, "NVS cache erased — will rediscover on next cycle");
  }
}

// ============================================================
//  Night mode (simplified — based on time of day)
// ============================================================

bool SmaBluetoothHub::is_nighttime() const {
  time_t now = time(nullptr);
  if (now < MIN_VALID_TIME) return false;  // time not yet synced, don't sleep

  struct tm local_tm;
  localtime_r(&now, &local_tm);
  int hour = local_tm.tm_hour;
  // Consider night from 22:00 to 05:00
  return (hour >= 22 || hour < 5);
}

// ============================================================
//  FreeRTOS BT task
// ============================================================

void SmaBluetoothHub::bt_task_entry(void *param) {
  auto *self = static_cast<SmaBluetoothHub *>(param);
  self->bt_task_loop();
}

void SmaBluetoothHub::bt_task_loop() {
  static const char *TTAG = "sma_bt_task";

  ESP_LOGD(TTAG, "BT task started, waiting for SPP init...");

  // Wait for SPP stack ready
  EventBits_t bits = xEventGroupWaitBits(bt_event_group_, BT_EVT_SPP_INIT,
                                          pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));
  if (!(bits & BT_EVT_SPP_INIT)) {
    ESP_LOGE(TTAG, "Timeout waiting for SPP init");
    task_error_ = true;
    bt_task_handle_ = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  ESP_LOGI(TTAG, "SPP ready, starting poll loop");
  uint32_t last_discovery_ms = 0;
  bool nvs_passwords_saved = false;

  while (!stop_task_) {
    // Night mode check
    if (is_nighttime()) {
      ESP_LOGD(TTAG, "Night mode, sleeping 30 min");
      vTaskDelay(pdMS_TO_TICKS(30 * 60 * 1000));
      continue;
    }

    // Run discovery periodically if enabled
    if (discovery_enabled_) {
      uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
      if (last_discovery_ms == 0 || (now_ms - last_discovery_ms) > 5 * 60 * 1000) {
        run_discovery_scan();
        last_discovery_ms = now_ms;
      }
    }

    // No devices? Wait and retry
    if (devices_.empty()) {
      ESP_LOGD(TTAG, "No devices registered, waiting 30s...");
      vTaskDelay(pdMS_TO_TICKS(30000));
      continue;
    }

    // Poll each device sequentially
    bool any_polled = false;
    for (auto *device : devices_) {
      if (stop_task_) break;

      // Check if it's time to poll this device
      uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
      if (device->last_poll_ms() != 0 &&
          (now_ms - device->last_poll_ms()) < device->update_interval_ms()) {
        continue;
      }

      // Exponential backoff on errors
      if (device->consecutive_errors() >= 3) {
        uint32_t backoff = device->consecutive_errors() * 30000;
        if (backoff > 300000) backoff = 300000;  // cap at 5 min
        if (device->last_poll_ms() != 0 &&
            (now_ms - device->last_poll_ms()) < backoff) {
          continue;
        }
      }

      ESP_LOGD(TTAG, "Polling %s...", device->mac_string().c_str());

      bool ok = device->poll(this);
      device->set_last_poll_ms(xTaskGetTickCount() * portTICK_PERIOD_MS);

      if (ok) {
        device->reset_errors();
        ESP_LOGD(TTAG, "Poll OK for %s", device->mac_string().c_str());
      } else {
        device->increment_errors();
        device->publish_unavailable();
        if (device->consecutive_errors() <= 3) {
          ESP_LOGD(TTAG, "Poll failed for %s (errors: %u)",
                   device->mac_string().c_str(), device->consecutive_errors());
        } else {
          ESP_LOGW(TTAG, "Poll failed for %s (errors: %u)",
                   device->mac_string().c_str(), device->consecutive_errors());
        }
      }

      any_polled = true;

      // Inter-inverter delay
      if (!stop_task_) {
        vTaskDelay(pdMS_TO_TICKS(inter_inverter_delay_ms_));
      }
    }

    // Signal main loop that one full pass through all devices is done.
    // Only signal if at least one device was actually polled successfully
    // (at night all inverters are off, so polls fail — don't signal then).
    if (any_polled) {
      poll_cycle_complete_ = true;

      // Save working passwords to NVS after first successful cycle
      if (!nvs_passwords_saved) {
        save_cached_inverters_();
        nvs_passwords_saved = true;
      }
    }

    if (!any_polled) {
      // All devices recently polled; sleep until next one is due
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }

  ESP_LOGI(TTAG, "BT task stopping");
  bt_task_handle_ = nullptr;
  vTaskDelete(nullptr);
}

// ============================================================
//  NVS cache — persist discovered inverter MACs across reboots
// ============================================================

static const char *NVS_NAMESPACE = "sma_bt";
static const char *NVS_KEY_COUNT = "inv_count";
// Keys: "inv_mac_0", "inv_mac_1", ... store MAC strings
// Keys: "inv_name_0", "inv_name_1", ... store name prefixes
// Keys: "inv_pw_0", "inv_pw_1", ... store working passwords

void SmaBluetoothHub::save_cached_inverters_() {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "NVS open failed: %s", esp_err_to_name(err));
    return;
  }

  uint8_t count = (uint8_t) devices_.size();
  nvs_set_u8(handle, NVS_KEY_COUNT, count);

  for (uint8_t i = 0; i < count; i++) {
    char key_mac[16];
    snprintf(key_mac, sizeof(key_mac), "inv_mac_%u", i);
    nvs_set_str(handle, key_mac, devices_[i]->mac_string().c_str());

    // Save working password if known
    if (devices_[i]->has_password()) {
      char key_pw[16];
      snprintf(key_pw, sizeof(key_pw), "inv_pw_%u", i);
      nvs_set_str(handle, key_pw, devices_[i]->password());
    }
  }

  nvs_commit(handle);
  nvs_close(handle);
  ESP_LOGD(TAG, "Cached %u inverter(s) to NVS", count);
}

void SmaBluetoothHub::restore_cached_inverters_() {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    ESP_LOGI(TAG, "NVS open for read failed: %s (first boot?)", esp_err_to_name(err));
    return;
  }

  uint8_t count = 0;
  if (nvs_get_u8(handle, NVS_KEY_COUNT, &count) != ESP_OK || count == 0) {
    ESP_LOGI(TAG, "NVS: no cached inverter count (first boot)");
    nvs_close(handle);
    return;
  }

  ESP_LOGD(TAG, "NVS: found %u cached inverter(s)", count);

  for (uint8_t i = 0; i < count; i++) {
    char key_mac[16];
    snprintf(key_mac, sizeof(key_mac), "inv_mac_%u", i);

    char mac_buf[20] = {0};
    size_t mac_len = sizeof(mac_buf);

    esp_err_t mac_err = nvs_get_str(handle, key_mac, mac_buf, &mac_len);

    if (mac_err == ESP_OK) {
      InverterConfig cfg;
      cfg.mac_string = mac_buf;
      cfg.parse_mac_from_string();

      // Restore working password if saved
      char key_pw[16];
      snprintf(key_pw, sizeof(key_pw), "inv_pw_%u", i);
      char pw_buf[16] = {0};
      size_t pw_len = sizeof(pw_buf);
      if (nvs_get_str(handle, key_pw, pw_buf, &pw_len) == ESP_OK && pw_buf[0] != '\0') {
        cfg.password = pw_buf;
        ESP_LOGD(TAG, "  Restored password for inverter[%u]", i);
      }

      cached_inverter_configs_.push_back(cfg);
      ESP_LOGD(TAG, "  Restored cached inverter[%u]: %s", i, mac_buf);
    } else {
      ESP_LOGW(TAG, "  NVS read failed for inverter[%u]: %s", i,
               esp_err_to_name(mac_err));
    }
  }

  nvs_close(handle);
}

// ============================================================
//  Pre-create devices + sensors during setup()
// ============================================================

void SmaBluetoothHub::pre_create_devices_() {
  // Merge configured inverters and cached (discovered) inverters.
  // Configured entries take priority; cached ones fill in the rest.
  std::vector<InverterConfig *> to_create;

  ESP_LOGD(TAG, "pre_create_devices_: %d configured, %d cached",
           (int)inverter_configs_.size(), (int)cached_inverter_configs_.size());

  for (auto &cfg : inverter_configs_) {
    to_create.push_back(&cfg);
  }

  for (auto &cached : cached_inverter_configs_) {
    // Skip if already in configured list
    bool already = false;
    for (const auto &cfg : inverter_configs_) {
      if (cfg.mac_string == cached.mac_string) {
        already = true;
        break;
      }
    }
    if (!already) {
      to_create.push_back(&cached);
    }
  }

  ESP_LOGD(TAG, "pre_create_devices_: will create %d device(s)", (int)to_create.size());

  for (auto *cfg : to_create) {
    // Create the device
    auto *dev = new SmaInverterDevice();
    dev->set_mac_address(cfg->mac_string);
    if (!cfg->password.empty()) dev->set_password(cfg->password);
    if (!cfg->name.empty()) dev->set_name_prefix(cfg->name);

    // Build device name: user-configured name > MAC (always stable)
    std::string prefix;
    if (!cfg->name.empty()) {
      prefix = cfg->name;
    } else {
      // MAC without colons for clean entity IDs
      std::string mac_clean = cfg->mac_string;
      mac_clean.erase(std::remove(mac_clean.begin(), mac_clean.end(), ':'), mac_clean.end());
      prefix = "SMA " + mac_clean;
    }
    dev->create_auto_sensors(prefix);

    register_device(dev);
    ESP_LOGI(TAG, "Pre-created device + sensors: '%s' (%s)",
             prefix.c_str(), cfg->mac_string.c_str());
  }
}

}  // namespace sma_bluetooth
}  // namespace esphome
