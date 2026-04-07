#pragma once

/* MIT License — based on work by Lupo135, darrylb123, keerekeerweere */

#include "esphome/core/component.h"
#include "sma_protocol.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "freertos/event_groups.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"

#include <string>
#include <vector>

namespace esphome {
namespace sma_bluetooth {

class SmaInverterDevice;  // forward

struct InverterConfig {
  uint8_t     mac[6]{};
  std::string mac_string;
  std::string name;       // friendly name prefix for sensors
  std::string password;   // per-inverter override (empty = use defaults)
  uint32_t    serial{0};  // cached serial number for naming

  void parse_mac_from_string() {
    if (mac_string.size() >= 17) {
      sscanf(mac_string.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
             &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
    }
  }
};

struct DiscoveredDevice {
  uint8_t     mac[6];
  std::string name;       // BT device name from GAP discovery
};

class SmaBluetoothHub : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  // Configuration setters (called from generated code)
  void set_discovery_enabled(bool v) { discovery_enabled_ = v; }
  void set_query_delay_ms(uint32_t v) { query_delay_ms_ = v; }
  void set_inter_inverter_delay_ms(uint32_t v) { inter_inverter_delay_ms_ = v; }
  void add_password(const std::string &pw) { passwords_.push_back(pw); }
  void add_inverter_config(InverterConfig entry) {
    entry.parse_mac_from_string();
    inverter_configs_.push_back(entry);
  }

  // Device registration
  void register_device(SmaInverterDevice *device);

  // BT I/O — called by device protocol methods from BT task context
  void bt_send_packet(uint8_t *data, uint16_t len);
  uint8_t bt_get_byte(uint32_t timeout_ms);
  bool bt_read_timeout() const { return read_timeout_; }
  void bt_flush_rx();

  // SPP connection management — called from BT task
  bool spp_connect(const uint8_t mac[6]);
  void spp_disconnect();
  bool is_bt_connected() const { return bt_connected_; }

  // Discovery
  void trigger_discovery();

  // Erase NVS cache (call from on_boot lambda to force rediscovery)
  void erase_nvs_cache();

  const std::vector<std::string> &get_passwords() const { return passwords_; }
  uint32_t query_delay_ms() const { return query_delay_ms_; }

 protected:
  // Static callbacks (BT stack context)
  static void spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);
  static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

  // FreeRTOS task
  static void bt_task_entry(void *param);
  void bt_task_loop();

  // BT stack init
  bool init_bt_stack();

  // Discovery
  void run_discovery_scan();
  bool is_configured(const uint8_t mac[6]) const;
  const InverterConfig *find_inverter_config(const uint8_t mac[6]) const;
  SmaInverterDevice *find_device_by_mac(const uint8_t mac[6]) const;
  SmaInverterDevice *create_discovered_device(const uint8_t mac[6], const std::string &bt_name);

  // Night mode
  bool is_nighttime() const;

  // Pre-create devices/sensors during setup (before API connects)
  void pre_create_devices_();

  // NVS cache for discovered inverter MACs (survive reboots)
  void save_cached_inverters_();
  void restore_cached_inverters_();

  // Registered devices
  std::vector<SmaInverterDevice *> devices_;

  // Discovery results (populated in GAP callback, consumed in task)
  std::vector<DiscoveredDevice> discovered_;
  volatile bool discovery_complete_{false};

  // FreeRTOS handles
  StreamBufferHandle_t rx_stream_buf_{nullptr};
  EventGroupHandle_t   bt_event_group_{nullptr};
  volatile TaskHandle_t bt_task_handle_{nullptr};

  // SPP state
  volatile uint32_t spp_handle_{0};
  uint8_t  discovered_scn_{1};
  volatile bool bt_connected_{false};
  volatile bool task_error_{false};
  volatile bool stop_task_{false};
  bool read_timeout_{false};
  bool bt_initialized_{false};

  // Configuration
  bool discovery_enabled_{true};
  uint32_t query_delay_ms_{200};
  uint32_t inter_inverter_delay_ms_{5000};
  std::vector<std::string> passwords_;
  std::vector<InverterConfig> inverter_configs_;
  std::vector<InverterConfig> cached_inverter_configs_;  // restored from NVS

  // Reboot deferral: wait for all devices to be polled before rebooting
  bool reboot_pending_{false};
  uint32_t reboot_defer_start_ms_{0};

  // Singleton instance for static callbacks
  static SmaBluetoothHub *instance_;
};

}  // namespace sma_bluetooth
}  // namespace esphome
