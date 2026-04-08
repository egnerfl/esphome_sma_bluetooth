#pragma once

/* MIT License — based on work by Lupo135, darrylb123, keerekeerweere */

#include "sma_protocol.h"
#include "esphome/core/defines.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "esphome/core/entity_base.h"
#ifdef USE_DEVICES
#include "esphome/core/device.h"
#endif

#include <string>

namespace esphome {
namespace sma_bluetooth {

class SmaBluetoothHub;

// Wrapper subclasses for dynamic sensor creation at runtime.
// Uses configure_entity_() (protected, accessible from subclass) to set name,
// compute the object_id_hash, and pack device_class/UOM/icon into entity_fields.
// Optionally assigns sensors to an ESPHome Device for sub-device support.

// Pack device_class, UOM, and icon indices into entity_fields bitfield.
// Layout: bits 0-7 = dc_idx, 8-15 = uom_idx, 16-23 = icon_idx, 26-27 = entity_category
static constexpr uint32_t sma_entity_fields(uint8_t dc_idx, uint8_t uom_idx, uint8_t icon_idx,
                                             uint8_t entity_category = 0) {
  return (uint32_t(dc_idx) << 0) | (uint32_t(uom_idx) << 8) |
         (uint32_t(icon_idx) << 16) | (uint32_t(entity_category) << 26);
}

class DynamicSensor : public sensor::Sensor {
 public:
#ifdef USE_DEVICES
  explicit DynamicSensor(const std::string &name, uint32_t entity_fields = 0, Device *device = nullptr)
#else
  explicit DynamicSensor(const std::string &name, uint32_t entity_fields = 0)
#endif
      : owned_name_(strdup(name.c_str())) {
    this->configure_entity_(owned_name_, 0, entity_fields);
#ifdef USE_DEVICES
    if (device) this->set_device_(device);
#endif
  }
  ~DynamicSensor() { free(owned_name_); }
 protected:
  char *owned_name_;
};

#ifdef USE_TEXT_SENSOR
class DynamicTextSensor : public text_sensor::TextSensor {
 public:
#ifdef USE_DEVICES
  explicit DynamicTextSensor(const std::string &name, uint32_t entity_fields = 0, Device *device = nullptr)
#else
  explicit DynamicTextSensor(const std::string &name, uint32_t entity_fields = 0)
#endif
      : owned_name_(strdup(name.c_str())) {
    this->configure_entity_(owned_name_, 0, entity_fields);
#ifdef USE_DEVICES
    if (device) this->set_device_(device);
#endif
  }
  ~DynamicTextSensor() { free(owned_name_); }
 protected:
  char *owned_name_;
};
#endif

#ifdef USE_BINARY_SENSOR
class DynamicBinarySensor : public binary_sensor::BinarySensor {
 public:
#ifdef USE_DEVICES
  explicit DynamicBinarySensor(const std::string &name, uint32_t entity_fields = 0, Device *device = nullptr)
#else
  explicit DynamicBinarySensor(const std::string &name, uint32_t entity_fields = 0)
#endif
      : owned_name_(strdup(name.c_str())) {
    this->configure_entity_(owned_name_, 0, entity_fields);
#ifdef USE_DEVICES
    if (device) this->set_device_(device);
#endif
  }
  ~DynamicBinarySensor() { free(owned_name_); }
 protected:
  char *owned_name_;
};
#endif

class SmaInverterDevice {
 public:
  // Configuration (called from generated code or discovery)
  void set_mac_address(const std::string &mac);
  void set_password(const std::string &pw);
  void set_name_prefix(const std::string &name) { name_prefix_ = name; }
  void set_update_interval_ms(uint32_t v) { update_interval_ms_ = v; }

  // Called by hub's BT task — full poll cycle
  bool poll(SmaBluetoothHub *hub);

  // Called from main loop to publish cached data to ESPHome sensors
  // Returns true if new auto-sensors were just created (caller should reboot)
  bool publish_sensors();

  // Create auto-sensors for autodiscovery mode
  void create_auto_sensors(const std::string &prefix);

  // Handle missing values (derive power from V*I)
  void handle_missing_values();

  // Accessors
  const uint8_t *bt_address() const { return bt_address_; }
  const std::string &mac_string() const { return mac_string_; }
  const std::string &name_prefix() const { return name_prefix_; }
  uint32_t serial() const { return inv_data_.Serial; }
  bool is_data_fresh() const { return data_fresh_; }
  void clear_data_fresh() { data_fresh_ = false; }
  uint32_t last_poll_ms() const { return last_poll_ms_; }
  void set_last_poll_ms(uint32_t v) { last_poll_ms_ = v; }
  uint32_t update_interval_ms() const { return update_interval_ms_; }
  uint32_t consecutive_errors() const { return consecutive_errors_; }
  void increment_errors() { consecutive_errors_++; }
  void reset_errors() { consecutive_errors_ = 0; }
  bool has_password() const { return password_set_; }
  const char *password() const { return password_; }

  // Set working password after successful login
  void set_working_password(const std::string &pw);

  // Sensor setters (called from generated code)
  void set_voltage_sensor(uint8_t phase, sensor::Sensor *s) {
    if (phase < 3) phases_[phase].voltage = s;
  }
  void set_current_sensor(uint8_t phase, sensor::Sensor *s) {
    if (phase < 3) phases_[phase].current = s;
  }
  void set_active_power_sensor(uint8_t phase, sensor::Sensor *s) {
    if (phase < 3) phases_[phase].active_power = s;
  }
  void set_voltage_sensor_pv(uint8_t pv, sensor::Sensor *s) {
    if (pv < 2) pvs_[pv].voltage = s;
  }
  void set_current_sensor_pv(uint8_t pv, sensor::Sensor *s) {
    if (pv < 2) pvs_[pv].current = s;
  }
  void set_active_power_sensor_pv(uint8_t pv, sensor::Sensor *s) {
    if (pv < 2) pvs_[pv].active_power = s;
  }
  void set_grid_frequency_sensor(sensor::Sensor *s) { grid_frequency_sensor_ = s; }
  void set_today_production_sensor(sensor::Sensor *s) { today_production_ = s; }
  void set_total_energy_production_sensor(sensor::Sensor *s) { total_energy_production_ = s; }
  void set_inverter_module_temp_sensor(sensor::Sensor *s) { inverter_module_temp_ = s; }
  void set_bt_signal_strength_sensor(sensor::Sensor *s) { bt_signal_strength_ = s; }
  void set_today_generation_time_sensor(sensor::Sensor *s) { today_generation_time_ = s; }
  void set_total_generation_time_sensor(sensor::Sensor *s) { total_generation_time_ = s; }
  void set_wakeup_time_sensor(sensor::Sensor *s) { wakeup_time_ = s; }
#ifdef USE_TEXT_SENSOR
  void set_inverter_status_sensor(text_sensor::TextSensor *s) { status_text_sensor_ = s; }
  void set_serial_number_sensor(text_sensor::TextSensor *s) { serial_number_ = s; }
  void set_software_version_sensor(text_sensor::TextSensor *s) { software_version_ = s; }
  void set_device_type_sensor(text_sensor::TextSensor *s) { device_type_ = s; }
  void set_device_class_sensor(text_sensor::TextSensor *s) { device_class_ = s; }
  void set_inverter_time_sensor(text_sensor::TextSensor *s) { inverter_time_sensor_ = s; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_grid_relay_sensor(binary_sensor::BinarySensor *s) { grid_relay_ = s; }
#endif

 protected:
  // Protocol operations (run inside hub's BT task, use hub for I/O)
  E_RC initialize_connection(SmaBluetoothHub *hub);
  E_RC logon(SmaBluetoothHub *hub, const char *password, uint8_t user);
  void logoff(SmaBluetoothHub *hub);
  E_RC get_inverter_data(SmaBluetoothHub *hub, getInverterDataType type);
  E_RC get_inverter_data_cfl(SmaBluetoothHub *hub, uint32_t command, uint32_t first, uint32_t last);
  E_RC get_packet(SmaBluetoothHub *hub, const uint8_t exp_addr[6], int wait4_command);
  bool get_bt_signal_strength(SmaBluetoothHub *hub);

  // Packet building helpers
  void write_packet_header(uint8_t *buf, uint16_t control, const uint8_t *dest);
  void write_packet(uint8_t *buf, uint8_t longwords, uint8_t ctrl,
                    uint16_t ctrl2, uint16_t dst_susyid, uint32_t dst_serial);
  void write_packet_trailer(uint8_t *buf);
  void write_packet_length(uint8_t *buf);
  void write_byte(uint8_t *buf, uint8_t v);
  void write_32(uint8_t *buf, uint32_t v);
  void write_16(uint8_t *buf, uint16_t v);
  void write_array(uint8_t *buf, const uint8_t bytes[], int count);
  bool validate_checksum();
  bool is_crc_valid(uint8_t lb, uint8_t hb) const;
  bool is_valid_sender(const uint8_t exp[6], const uint8_t is[6]) const;

  // Sensor publish helpers
  void publish_sensor(sensor::Sensor *s, float v);
  void publish_sensor(sensor::Sensor *s, uint64_t v);
#ifdef USE_TEXT_SENSOR
  void publish_sensor(text_sensor::TextSensor *s, const std::string &v);
#endif
#ifdef USE_BINARY_SENSOR
  void publish_sensor(binary_sensor::BinarySensor *s, bool v);
#endif

  // Per-inverter identity
  uint8_t bt_address_[6]{};
  uint8_t esp_bt_address_[6]{};  // learned during init handshake
  char    password_[13]{};       // 12 chars + null
  bool    password_set_{false};
  std::string mac_string_;
  std::string name_prefix_;
  uint32_t update_interval_ms_{60000};

  // Protocol data
  InverterData inv_data_{};
  DisplayData  disp_data_{};

  // Packet buffers
  uint8_t  pkt_buf_[COMMBUFSIZE]{};
  uint8_t  rd_buf_[COMMBUFSIZE]{};
  uint16_t pkt_buf_pos_{0};
  uint16_t fcs_checksum_{0xFFFF};
  uint16_t pkt_id_{1};
  bool     read_timeout_{false};

  int32_t  value32_{0};
  int64_t  value64_{0};
  char     time_buf_[24]{};
  char     char_buf_[64]{};
  char     inverter_version_[24]{};

  // Freshness / timing
  volatile bool data_fresh_{false};
  uint32_t last_poll_ms_{0};
  uint32_t consecutive_errors_{0};
  uint32_t consecutive_stuck_{0};  // tracks "all queries failed" cycles specifically

  // EToday computation: ETotal baseline at start of day
  uint64_t etotal_baseline_{0};       // ETotal (Wh) at start of current day
  uint8_t  etotal_baseline_day_{0};   // day-of-month when baseline was set (1-31, 0=unset)

  // Protocol constants for this device
  const uint16_t app_susyid_{125};
  uint32_t app_serial_{0};

  // Sensor pointers
  struct Phase {
    sensor::Sensor *voltage{nullptr};
    sensor::Sensor *current{nullptr};
    sensor::Sensor *active_power{nullptr};
  } phases_[3];

  struct PV {
    sensor::Sensor *voltage{nullptr};
    sensor::Sensor *current{nullptr};
    sensor::Sensor *active_power{nullptr};
  } pvs_[2];

  sensor::Sensor *total_ac_power_{nullptr};
  sensor::Sensor *grid_frequency_sensor_{nullptr};
  sensor::Sensor *today_production_{nullptr};
  sensor::Sensor *total_energy_production_{nullptr};
  sensor::Sensor *inverter_module_temp_{nullptr};
  sensor::Sensor *bt_signal_strength_{nullptr};
  sensor::Sensor *today_generation_time_{nullptr};
  sensor::Sensor *total_generation_time_{nullptr};
  sensor::Sensor *wakeup_time_{nullptr};

#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *status_text_sensor_{nullptr};
  text_sensor::TextSensor *serial_number_{nullptr};
  text_sensor::TextSensor *software_version_{nullptr};
  text_sensor::TextSensor *device_type_{nullptr};
  text_sensor::TextSensor *device_class_{nullptr};
  text_sensor::TextSensor *inverter_time_sensor_{nullptr};
  text_sensor::TextSensor *mac_address_sensor_{nullptr};
#endif

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *grid_relay_{nullptr};
#endif

  // Flag: auto-sensors created (to avoid duplicating)
  bool auto_sensors_created_{false};

  // Deferred auto-sensor creation: BT task sets the prefix,
  // main loop creates & registers the sensors (thread safety).
  std::string pending_auto_sensor_prefix_;
  volatile bool pending_auto_sensors_{false};

#ifdef USE_DEVICES
  Device *ha_device_{nullptr};
#endif
};

}  // namespace sma_bluetooth
}  // namespace esphome
