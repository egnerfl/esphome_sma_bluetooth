/* MIT License — based on work by Lupo135, darrylb123, keerekeerweere */

#include "sma_inverter_device.h"
#include "sma_bluetooth_hub.h"
#include "esphome/core/defines.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <ctime>

namespace esphome {
namespace sma_bluetooth {

static const char *const TAG = "sma_bluetooth.device";

// ============================================================
//  Configuration
// ============================================================

void SmaInverterDevice::set_mac_address(const std::string &mac) {
  mac_string_ = mac;
  sscanf(mac.c_str(), "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
         &bt_address_[0], &bt_address_[1], &bt_address_[2],
         &bt_address_[3], &bt_address_[4], &bt_address_[5]);
  // SMA protocol uses reversed byte order
  for (int i = 0; i < 6; i++)
    inv_data_.btAddress[i] = bt_address_[5 - i];
}

void SmaInverterDevice::set_password(const std::string &pw) {
  memset(password_, 0, sizeof(password_));
  strncpy(password_, pw.c_str(), 12);
  password_set_ = true;
}

void SmaInverterDevice::set_working_password(const std::string &pw) {
  set_password(pw);
  ESP_LOGI(TAG, "[%s] Working password cached", mac_string_.c_str());
}

// ============================================================
//  Main poll cycle: connect → init → logon → query → disconnect
// ============================================================

bool SmaInverterDevice::poll(SmaBluetoothHub *hub) {
  // Connect
  if (!hub->spp_connect(bt_address_)) {
    return false;
  }

  pkt_id_ = 1;
  hub->bt_flush_rx();

  // Initialize SMA connection
  E_RC rc = initialize_connection(hub);
  if (rc != E_OK) {
    ESP_LOGE(TAG, "[%s] init failed: %d", mac_string_.c_str(), rc);
    hub->spp_disconnect();
    return false;
  }

  // Logon — try passwords
  bool logged_on = false;
  if (password_set_) {
    // Use specific password
    rc = logon(hub, password_, USERGROUP);
    if (rc == E_OK) logged_on = true;
  }

  if (!logged_on) {
    // Try default passwords
    for (const auto &pw : hub->get_passwords()) {
      rc = logon(hub, pw.c_str(), USERGROUP);
      if (rc == E_OK) {
        set_working_password(pw);
        logged_on = true;
        break;
      }
      ESP_LOGD(TAG, "[%s] Password '%s' failed, trying next...",
               mac_string_.c_str(), pw.c_str());
      // Reconnect needed after failed logon
      hub->spp_disconnect();
      vTaskDelay(pdMS_TO_TICKS(2000));
      if (!hub->spp_connect(bt_address_)) return false;
      pkt_id_ = 1;
      hub->bt_flush_rx();
      rc = initialize_connection(hub);
      if (rc != E_OK) return false;
    }
  }

  if (!logged_on) {
    ESP_LOGE(TAG, "[%s] All passwords failed", mac_string_.c_str());
    hub->spp_disconnect();
    return false;
  }

  // Query once-types (TypeLabel, SoftwareVersion)
  for (int i = 0; i < NUM_ONCE_TYPES; i++) {
    get_inverter_data(hub, ONCE_QUERY_TYPES[i]);
    vTaskDelay(pdMS_TO_TICKS(hub->query_delay_ms()));
  }

  // Create auto-sensors after we have device info
  // (deferred to main loop for thread safety — App.register_sensor() is not thread-safe)
  if (!auto_sensors_created_ && !inv_data_.DeviceName.empty()) {
    std::string prefix = name_prefix_;
    if (prefix.empty()) {
      // Use MAC address without colons — always available, never changes between reboots
      // e.g. "SMA 0080251AF252" → entity_id: sma_0080251af252_voltage_l1
      std::string mac_clean = mac_string_;
      mac_clean.erase(std::remove(mac_clean.begin(), mac_clean.end(), ':'), mac_clean.end());
      prefix = "SMA " + mac_clean;
    }
    pending_auto_sensor_prefix_ = prefix;
    pending_auto_sensors_ = true;
  }

  // BT signal strength
  get_bt_signal_strength(hub);

  // Query fast types (power, voltage, energy, frequency)
  for (int i = 0; i < NUM_FAST_TYPES; i++) {
    rc = get_inverter_data(hub, FAST_QUERY_TYPES[i]);
    if (rc != E_OK) {
      ESP_LOGD(TAG, "[%s] Fast query %d failed: %d", mac_string_.c_str(),
               FAST_QUERY_TYPES[i], rc);
    }
    vTaskDelay(pdMS_TO_TICKS(hub->query_delay_ms()));
  }

  // Query slow types (status, relay, temp)
  for (int i = 0; i < NUM_SLOW_TYPES; i++) {
    rc = get_inverter_data(hub, SLOW_QUERY_TYPES[i]);
    if (rc != E_OK) {
      ESP_LOGD(TAG, "[%s] Slow query %d failed: %d", mac_string_.c_str(),
               SLOW_QUERY_TYPES[i], rc);
    }
    vTaskDelay(pdMS_TO_TICKS(hub->query_delay_ms()));
  }

  // Compute derived values
  handle_missing_values();

  // Disconnect
  logoff(hub);
  hub->spp_disconnect();

  // Signal main loop
  data_fresh_ = true;
  return true;
}

// ============================================================
//  SMA protocol: initialize connection
// ============================================================

E_RC SmaInverterDevice::initialize_connection(SmaBluetoothHub *hub) {
  ESP_LOGD(TAG, "[%s] Initializing SMA connection", mac_string_.c_str());

  // Step 1: Receive packet → extract NetID
  get_packet(hub, inv_data_.btAddress, 2);
  inv_data_.NetID = pkt_buf_[22];
  ESP_LOGD(TAG, "[%s] NetID=0x%02X", mac_string_.c_str(), inv_data_.NetID);

  // Step 2: Send NetID config
  write_packet_header(pkt_buf_, 0x02, inv_data_.btAddress);
  write_32(pkt_buf_, 0x00700400);
  write_byte(pkt_buf_, inv_data_.NetID);
  write_32(pkt_buf_, 0);
  write_32(pkt_buf_, 1);
  write_packet_length(pkt_buf_);
  hub->bt_send_packet(pkt_buf_, pkt_buf_pos_);

  // Step 3: Receive response → extract ESP BT address
  get_packet(hub, inv_data_.btAddress, 5);
  memcpy(esp_bt_address_, pkt_buf_ + 26, 6);
  ESP_LOGD(TAG, "[%s] ESP BT addr: %02X:%02X:%02X:%02X:%02X:%02X",
           mac_string_.c_str(),
           esp_bt_address_[5], esp_bt_address_[4], esp_bt_address_[3],
           esp_bt_address_[2], esp_bt_address_[1], esp_bt_address_[0]);

  // Step 4: Query inverter identity
  pkt_id_++;
  write_packet_header(pkt_buf_, 0x01, FFS_6);
  write_packet(pkt_buf_, 0x09, 0xA0, 0, 0xFFFF, 0xFFFFFFFF);
  write_32(pkt_buf_, 0x00000200);
  write_32(pkt_buf_, 0);
  write_32(pkt_buf_, 0);
  write_packet_trailer(pkt_buf_);
  write_packet_length(pkt_buf_);
  hub->bt_send_packet(pkt_buf_, pkt_buf_pos_);

  if (get_packet(hub, inv_data_.btAddress, 1) != E_OK) return E_INIT;
  if (!validate_checksum()) return E_CHKSUM;

  inv_data_.Serial = get_u32(pkt_buf_ + 57);
  ESP_LOGD(TAG, "[%s] Serial: %lu", mac_string_.c_str(), (unsigned long)inv_data_.Serial);

  return E_OK;
}

// ============================================================
//  SMA protocol: logon
// ============================================================

E_RC SmaInverterDevice::logon(SmaBluetoothHub *hub, const char *password, uint8_t user) {
  uint8_t pw[12];
  uint8_t enc_char = (user == UG_USER) ? 0x88 : 0xBB;

  uint8_t idx;
  for (idx = 0; password[idx] != 0 && idx < 12; idx++)
    pw[idx] = password[idx] + enc_char;
  for (; idx < 12; idx++)
    pw[idx] = enc_char;

  pkt_id_++;
  time_t now = time(nullptr);

  write_packet_header(pkt_buf_, 0x01, FFS_6);
  write_packet(pkt_buf_, 0x0E, 0xA0, 0x0100, 0xFFFF, 0xFFFFFFFF);
  write_32(pkt_buf_, 0xFFFD040C);
  write_32(pkt_buf_, user);
  write_32(pkt_buf_, 0x00000384);
  write_32(pkt_buf_, (uint32_t)now);
  write_32(pkt_buf_, 0);
  write_array(pkt_buf_, pw, sizeof(pw));
  write_packet_trailer(pkt_buf_);
  write_packet_length(pkt_buf_);
  hub->bt_send_packet(pkt_buf_, pkt_buf_pos_);

  E_RC rc = get_packet(hub, FFS_6, 1);
  if (rc != E_OK) return rc;
  if (!validate_checksum()) return E_CHKSUM;

  uint16_t rcv_pkt_id = get_u16(pkt_buf_ + 27) & 0x7FFF;
  if (pkt_id_ == rcv_pkt_id && get_u32(pkt_buf_ + 41) == (uint32_t)now) {
    inv_data_.SUSyID = get_u16(pkt_buf_ + 15);
    inv_data_.Serial = get_u32(pkt_buf_ + 17);
    ESP_LOGD(TAG, "[%s] Logon OK (SUSyID=0x%02X Serial=%lu)",
             mac_string_.c_str(), inv_data_.SUSyID, (unsigned long)inv_data_.Serial);
    return E_OK;
  }

  ESP_LOGD(TAG, "[%s] Logon response mismatch", mac_string_.c_str());
  return E_INVRESP;
}

// ============================================================
//  SMA protocol: logoff
// ============================================================

void SmaInverterDevice::logoff(SmaBluetoothHub *hub) {
  pkt_id_++;
  write_packet_header(pkt_buf_, 0x01, FFS_6);
  write_packet(pkt_buf_, 0x08, 0xA0, 0x0300, 0xFFFF, 0xFFFFFFFF);
  write_32(pkt_buf_, 0xFFFD010E);
  write_32(pkt_buf_, 0xFFFFFFFF);
  write_packet_trailer(pkt_buf_);
  write_packet_length(pkt_buf_);
  hub->bt_send_packet(pkt_buf_, pkt_buf_pos_);
}

// ============================================================
//  SMA protocol: data query dispatcher
// ============================================================

E_RC SmaInverterDevice::get_inverter_data(SmaBluetoothHub *hub, getInverterDataType type) {
  uint32_t command, first, last;

  switch (type) {
    case EnergyProduction:
      command = 0x54000200; first = 0x00260100; last = 0x002622FF; break;
    case SpotDCPower:
      command = 0x53800200; first = 0x00251E00; last = 0x00251EFF; break;
    case SpotDCVoltage:
      command = 0x53800200; first = 0x00451F00; last = 0x004521FF; break;
    case SpotACPower:
      command = 0x51000200; first = 0x00464000; last = 0x004642FF; break;
    case SpotACVoltage:
      command = 0x51000200; first = 0x00464800; last = 0x004655FF; break;
    case SpotGridFrequency:
      command = 0x51000200; first = 0x00465700; last = 0x004657FF; break;
    case SpotACTotalPower:
      command = 0x51000200; first = 0x00263F00; last = 0x00263FFF; break;
    case TypeLabel:
      command = 0x58000200; first = 0x00821E00; last = 0x008220FF; break;
    case SoftwareVersion:
      command = 0x58000200; first = 0x00823400; last = 0x008234FF; break;
    case DeviceStatus:
      command = 0x51800200; first = 0x00214800; last = 0x002148FF; break;
    case GridRelayStatus:
      command = 0x51800200; first = 0x00416400; last = 0x004164FF; break;
    case OperationTime:
      command = 0x54000200; first = 0x00462E00; last = 0x00462FFF; break;
    case InverterTemp:
      command = 0x52000200; first = 0x00237700; last = 0x002377FF; break;
    case MeteringGridMsTotW:
      command = 0x51000200; first = 0x00463600; last = 0x004637FF; break;
    default:
      return E_BADARG;
  }

  for (uint8_t retries = 0; retries < 2; retries++) {
    E_RC rc = get_inverter_data_cfl(hub, command, first, last);
    if (rc == E_OK) return rc;
    if (retries == 0) {
      ESP_LOGD(TAG, "[%s] Query retry", mac_string_.c_str());
    }
  }
  return E_NODATA;
}

// ============================================================
//  SMA protocol: low-level data query
// ============================================================

E_RC SmaInverterDevice::get_inverter_data_cfl(SmaBluetoothHub *hub, uint32_t command,
                                                uint32_t first, uint32_t last) {
  // Build and send query packet
  do {
    pkt_id_++;
    write_packet_header(pkt_buf_, 0x01, FFS_6);
    write_packet(pkt_buf_, 0x09, 0xA0, 0, inv_data_.SUSyID, inv_data_.Serial);
    write_32(pkt_buf_, command);
    write_32(pkt_buf_, first);
    write_32(pkt_buf_, last);
    write_packet_trailer(pkt_buf_);
    write_packet_length(pkt_buf_);
  } while (!is_crc_valid(pkt_buf_[pkt_buf_pos_ - 3], pkt_buf_[pkt_buf_pos_ - 2]));

  hub->bt_send_packet(pkt_buf_, pkt_buf_pos_);

  // Receive and parse response
  bool valid_pkt_id = false;
  do {
    uint8_t pkt_count = 0;
    do {
      inv_data_.status = get_packet(hub, inv_data_.btAddress, 0x0001);
      if (inv_data_.status != E_OK) return inv_data_.status;

      if (!validate_checksum()) {
        inv_data_.status = E_CHKSUM;
        return inv_data_.status;
      }

      if ((inv_data_.status = (E_RC)get_u16(pkt_buf_ + 23)) != E_OK) {
        return inv_data_.status;
      }

      uint8_t i_pdc = 0, i_udc = 0, i_idc = 0;
      pkt_count = get_u16(pkt_buf_ + 25);
      uint16_t rcv_pkt_id = get_u16(pkt_buf_ + 27) & 0x7FFF;

      if (pkt_id_ != rcv_pkt_id) {
        ESP_LOGD(TAG, "PktID mismatch: exp=0x%04X got=0x%04X", pkt_id_, rcv_pkt_id);
        valid_pkt_id = false;
        pkt_count = 0;
        continue;
      }

      if (get_u16(pkt_buf_ + 15) != inv_data_.SUSyID ||
          get_u32(pkt_buf_ + 17) != inv_data_.Serial) {
        ESP_LOGD(TAG, "Wrong SUSyID/Serial in response");
        continue;
      }

      valid_pkt_id = true;

      uint16_t recordsize = 4 * ((uint32_t)pkt_buf_[5] - 9) /
                            (get_u32(pkt_buf_ + 37) - get_u32(pkt_buf_ + 33) + 1);
      if (recordsize == 0) {
        ESP_LOGE(TAG, "Invalid recordsize=0");
        break;
      }

      // Parse records
      for (uint16_t ii = 41; ii < pkt_buf_pos_ - 3; ii += recordsize) {
        if (ii + 20 > pkt_buf_pos_) break;

        uint8_t *recptr = pkt_buf_ + ii;
        uint32_t code = get_u32(recptr);
        uint16_t lri = (code & 0x00FFFF00) >> 8;
        uint8_t  data_type = code >> 24;
        time_t   datetime = (time_t)get_u32(recptr + 4);

        if (recordsize == 16) {
          value64_ = get_u64(recptr + 8);
          ESP_LOGV(TAG, "[%s] rec: code=0x%08X lri=0x%04X dt=%u val64=%llu",
                   mac_string_.c_str(), code, lri, data_type,
                   (unsigned long long)value64_);
          if (is_NaN(value64_) || is_NaN((uint64_t)value64_)) value64_ = 0;
        } else if (data_type != DT_STRING && data_type != DT_STATUS) {
          value32_ = get_u32(recptr + 16);
          if (is_NaN(value32_) || is_NaN((uint32_t)value32_)) value32_ = 0;
        }

        switch (lri) {
          case GridMsTotW:
            inv_data_.LastTime = datetime;
            inv_data_.TotalPac = toW(value32_);
            disp_data_.TotalPac = tokW(value32_);
            break;
          case GridMsWphsA:
            inv_data_.Pac1 = toW(value32_); disp_data_.Pac1 = tokW(value32_); break;
          case GridMsWphsB:
            inv_data_.Pac2 = toW(value32_); disp_data_.Pac2 = tokW(value32_); break;
          case GridMsWphsC:
            inv_data_.Pac3 = toW(value32_); disp_data_.Pac3 = tokW(value32_); break;
          case GridMsPhVphsA:
            inv_data_.Uac1 = value32_; disp_data_.Uac1 = toVolt(value32_); break;
          case GridMsPhVphsB:
            inv_data_.Uac2 = value32_; disp_data_.Uac2 = toVolt(value32_); break;
          case GridMsPhVphsC:
            inv_data_.Uac3 = value32_; disp_data_.Uac3 = toVolt(value32_); break;
          case GridMsAphsA_1:
          case GridMsAphsA:
            inv_data_.Iac1 = value32_; disp_data_.Iac1 = toAmp(value32_); break;
          case GridMsAphsB_1:
          case GridMsAphsB:
            inv_data_.Iac2 = value32_; disp_data_.Iac2 = toAmp(value32_); break;
          case GridMsAphsC_1:
          case GridMsAphsC:
            inv_data_.Iac3 = value32_; disp_data_.Iac3 = toAmp(value32_); break;
          case GridMsHz:
            inv_data_.GridFreq = value32_; disp_data_.GridFreq = toHz(value32_); break;
          case DcMsWatt:
            if (i_pdc == 0) { inv_data_.Pdc1 = toW(value32_); disp_data_.Pdc1 = tokW(value32_); }
            else if (i_pdc == 1) { inv_data_.Pdc2 = toW(value32_); disp_data_.Pdc2 = tokW(value32_); }
            i_pdc++;
            break;
          case DcMsVol:
            if (i_udc == 0) { inv_data_.Udc1 = value32_; disp_data_.Udc1 = toVolt(value32_); }
            else if (i_udc == 1) { inv_data_.Udc2 = value32_; disp_data_.Udc2 = toVolt(value32_); }
            i_udc++;
            break;
          case DcMsAmp:
            if (i_idc == 0) { inv_data_.Idc1 = value32_; disp_data_.Idc1 = toAmp(value32_); }
            else if (i_idc == 1) { inv_data_.Idc2 = value32_; disp_data_.Idc2 = toAmp(value32_); }
            i_idc++;
            break;
          case MeteringDyWhOut:
            inv_data_.EToday = value64_; disp_data_.EToday = tokWh(value64_); break;
          case MeteringTotWhOut:
            inv_data_.ETotal = value64_; disp_data_.ETotal = tokWh(value64_); break;
          case MeteringTotOpTms:
            inv_data_.OperationTime = value64_; break;
          case MeteringTotFeedTms:
            inv_data_.FeedInTime = value64_; break;
          case NameplateLocation: {
            inv_data_.WakeupTime = datetime;
            if (recordsize > 8) {
              const char *nameptr = (const char *)recptr + 8;
              size_t max_copy = recordsize - 8;
              if (max_copy > 63) max_copy = 63;
              strncpy(char_buf_, nameptr, max_copy);
              char_buf_[max_copy] = '\0';
              inv_data_.DeviceName = std::string(char_buf_);
            }
            break;
          }
          case NameplatePkgRev:
            get_version(get_u32(recptr + 24), inverter_version_);
            inv_data_.SWVersion = std::string(inverter_version_);
            break;
          case NameplateModel: {
            uint32_t attr = get_attribute(recptr) & 0x00FFFFFF;  // strip data type byte
            inv_data_.DeviceType = attr;
            break;
          }
          case NameplateMainModel: {
            uint32_t attr = get_attribute(recptr) & 0x00FFFFFF;  // strip data type byte
            inv_data_.DeviceClass = attr;
            break;
          }
          case CoolsysTmpNom:
            inv_data_.InvTemp = value32_; disp_data_.InvTemp = toTemp(value32_); break;
          case OperationHealth: {
            uint32_t attr = get_attribute(recptr);
            inv_data_.DevStatus = attr;
            break;
          }
          case OperationGriSwStt: {
            uint32_t attr = get_attribute(recptr) & 0x00FFFFFF;  // strip data type byte
            inv_data_.GridRelay = attr;
            break;
          }
          case MeteringGridMsTotWOut_LRI:
            inv_data_.MeteringGridMsTotWOut = value32_; break;
          case MeteringGridMsTotWIn_LRI:
            inv_data_.MeteringGridMsTotWIn = value32_; break;
          default:
            break;
        }
      }  // for records
    } while (pkt_count > 0);
  } while (!valid_pkt_id);

  return inv_data_.status;
}

// ============================================================
//  SMA protocol: packet reception
// ============================================================

E_RC SmaInverterDevice::get_packet(SmaBluetoothHub *hub, const uint8_t exp_addr[6],
                                    int wait4_command) {
  int index = 0;
  bool has_l2 = false;
  E_RC rc = E_OK;
  L1Hdr *hdr = (L1Hdr *)rd_buf_;

  do {
    // Read L1 header (18 bytes)
    uint8_t cnt = 0;
    for (cnt = 0; cnt < 18; cnt++) {
      rd_buf_[cnt] = hub->bt_get_byte(5000);
      if (hub->bt_read_timeout()) break;
    }

    if (cnt < 17) return E_NODATA;

    // Validate L1 checksum
    if (!((rd_buf_[0] ^ rd_buf_[1] ^ rd_buf_[2]) == rd_buf_[3])) {
      ESP_LOGD(TAG, "Wrong L1 CRC");
    }

    if (hdr->pkLength > sizeof(L1Hdr)) {
      if (hdr->pkLength > COMMBUFSIZE) {
        ESP_LOGE(TAG, "pkLength too large: %d", hdr->pkLength);
        hub->bt_flush_rx();
        rc = E_RETRY;
        continue;
      }

      // Read remaining bytes
      for (cnt = 18; cnt < hdr->pkLength; cnt++) {
        rd_buf_[cnt] = hub->bt_get_byte(5000);
        if (hub->bt_read_timeout()) break;
      }

      if (!is_valid_sender(exp_addr, hdr->SourceAddr)) {
        rc = E_RETRY;
        continue;
      }

      rc = E_OK;

      if (!has_l2 && rd_buf_[18] == 0x7E &&
          get_u32(rd_buf_ + 19) == BTH_L2SIGNATURE) {
        has_l2 = true;
      }

      if (has_l2) {
        // Unescape L2 payload
        bool esc_next = false;
        for (int i = sizeof(L1Hdr); i < hdr->pkLength; i++) {
          pkt_buf_[index] = rd_buf_[i];
          if (esc_next) {
            pkt_buf_[index] ^= 0x20;
            esc_next = false;
            index++;
          } else {
            if (pkt_buf_[index] == 0x7D)
              esc_next = true;
            else
              index++;
          }
          if (index >= MAX_PCKT_BUF_SIZE) {
            ESP_LOGE(TAG, "pkt_buf overflow");
            index = 0;
            rc = E_RETRY;
            break;
          }
        }
        pkt_buf_pos_ = index;
      } else {
        memcpy(pkt_buf_, rd_buf_, cnt);
        pkt_buf_pos_ = cnt;
      }
    } else {
      // L1-only packet
      if (!is_valid_sender(exp_addr, hdr->SourceAddr)) {
        rc = E_RETRY;
        continue;
      }
      rc = E_OK;
      memcpy(pkt_buf_, rd_buf_, sizeof(L1Hdr));
      pkt_buf_pos_ = sizeof(L1Hdr);
    }

    if (rd_buf_[0] != 0x7E) {
      hub->bt_flush_rx();
    }

  } while (((hdr->command != wait4_command) || (rc == E_RETRY)) &&
           (0xFF != wait4_command));

  return rc;
}

// ============================================================
//  BT signal strength
// ============================================================

bool SmaInverterDevice::get_bt_signal_strength(SmaBluetoothHub *hub) {
  write_packet_header(pkt_buf_, 0x03, inv_data_.btAddress);
  write_byte(pkt_buf_, 0x05);
  write_byte(pkt_buf_, 0x00);
  write_packet_length(pkt_buf_);
  hub->bt_send_packet(pkt_buf_, pkt_buf_pos_);
  get_packet(hub, inv_data_.btAddress, 4);
  disp_data_.BTSigStrength = ((float)rd_buf_[22] * 100.0f / 255.0f);
  return true;
}

// ============================================================
//  Packet building
// ============================================================

void SmaInverterDevice::write_packet_header(uint8_t *buf, uint16_t control,
                                              const uint8_t *dest) {
  pkt_buf_pos_ = 0;
  fcs_checksum_ = 0xFFFF;
  buf[pkt_buf_pos_++] = 0x7E;
  buf[pkt_buf_pos_++] = 0;  // length placeholder
  buf[pkt_buf_pos_++] = 0;
  buf[pkt_buf_pos_++] = 0;  // checksum placeholder
  for (int i = 0; i < 6; i++) buf[pkt_buf_pos_++] = esp_bt_address_[i];
  for (int i = 0; i < 6; i++) buf[pkt_buf_pos_++] = dest[i];
  buf[pkt_buf_pos_++] = (uint8_t)(control & 0xFF);
  buf[pkt_buf_pos_++] = (uint8_t)(control >> 8);
}

void SmaInverterDevice::write_packet(uint8_t *buf, uint8_t longwords, uint8_t ctrl,
                                       uint16_t ctrl2, uint16_t dst_susyid, uint32_t dst_serial) {
  buf[pkt_buf_pos_++] = 0x7E;
  write_32(buf, BTH_L2SIGNATURE);
  write_byte(buf, longwords);
  write_byte(buf, ctrl);
  write_16(buf, dst_susyid);
  write_32(buf, dst_serial);
  write_16(buf, ctrl2);
  write_16(buf, app_susyid_);
  write_32(buf, app_serial_);
  write_16(buf, ctrl2);
  write_16(buf, 0);
  write_16(buf, 0);
  write_16(buf, pkt_id_ | 0x8000);
}

void SmaInverterDevice::write_packet_trailer(uint8_t *buf) {
  fcs_checksum_ ^= 0xFFFF;
  buf[pkt_buf_pos_++] = fcs_checksum_ & 0xFF;
  buf[pkt_buf_pos_++] = (fcs_checksum_ >> 8) & 0xFF;
  buf[pkt_buf_pos_++] = 0x7E;
}

void SmaInverterDevice::write_packet_length(uint8_t *buf) {
  buf[1] = pkt_buf_pos_ & 0xFF;
  buf[2] = (pkt_buf_pos_ >> 8) & 0xFF;
  buf[3] = buf[0] ^ buf[1] ^ buf[2];
}

void SmaInverterDevice::write_byte(uint8_t *buf, uint8_t v) {
  fcs_checksum_ = (fcs_checksum_ >> 8) ^ FCS_TABLE[(fcs_checksum_ ^ v) & 0xFF];
  if (v == 0x7D || v == 0x7E || v == 0x11 || v == 0x12 || v == 0x13) {
    buf[pkt_buf_pos_++] = 0x7D;
    buf[pkt_buf_pos_++] = v ^ 0x20;
  } else {
    buf[pkt_buf_pos_++] = v;
  }
}

void SmaInverterDevice::write_32(uint8_t *buf, uint32_t v) {
  write_byte(buf, (uint8_t)((v >>  0) & 0xFF));
  write_byte(buf, (uint8_t)((v >>  8) & 0xFF));
  write_byte(buf, (uint8_t)((v >> 16) & 0xFF));
  write_byte(buf, (uint8_t)((v >> 24) & 0xFF));
}

void SmaInverterDevice::write_16(uint8_t *buf, uint16_t v) {
  write_byte(buf, (uint8_t)((v >> 0) & 0xFF));
  write_byte(buf, (uint8_t)((v >> 8) & 0xFF));
}

void SmaInverterDevice::write_array(uint8_t *buf, const uint8_t bytes[], int count) {
  for (int i = 0; i < count; i++) write_byte(buf, bytes[i]);
}

// ============================================================
//  Checksum validation
// ============================================================

bool SmaInverterDevice::validate_checksum() {
  uint16_t fcs = 0xFFFF;
  for (int i = 1; i <= pkt_buf_pos_ - 4; i++) {
    fcs = (fcs >> 8) ^ FCS_TABLE[(fcs ^ pkt_buf_[i]) & 0xFF];
  }
  fcs ^= 0xFFFF;
  if (get_u16(pkt_buf_ + pkt_buf_pos_ - 3) == fcs) return true;
  ESP_LOGD(TAG, "Checksum mismatch: got=0x%04X exp=0x%04X", fcs,
           get_u16(pkt_buf_ + pkt_buf_pos_ - 3));
  return false;
}

bool SmaInverterDevice::is_crc_valid(uint8_t lb, uint8_t hb) const {
  // Check that CRC bytes don't contain escape-sensitive values
  if (lb == 0x7D || lb == 0x7E || lb == 0x11 || lb == 0x12 || lb == 0x13) return false;
  if (hb == 0x7D || hb == 0x7E || hb == 0x11 || hb == 0x12 || hb == 0x13) return false;
  return true;
}

bool SmaInverterDevice::is_valid_sender(const uint8_t exp[6], const uint8_t is[6]) const {
  // Accept broadcast or matching address
  if (memcmp(exp, FFS_6, 6) == 0) return true;
  return memcmp(exp, is, 6) == 0;
}

// ============================================================
//  Missing value calculation (derive power from V*I)
// ============================================================

void SmaInverterDevice::handle_missing_values() {
  auto &d = disp_data_;
  auto &r = inv_data_;

  // DC power: fallback from V × A when SpotDCPower query fails
  if (d.Pdc1 == 0.0f && d.Udc1 != 0.0f && d.Idc1 != 0.0f) {
    d.Pdc1 = d.Udc1 * d.Idc1 / 1000.0f;  // kW
    r.Pdc1 = (int32_t)(d.Udc1 * d.Idc1);  // W
    d.needsMissingValues = true;
  }
  if (d.Pdc2 == 0.0f && d.Udc2 != 0.0f && d.Idc2 != 0.0f) {
    d.Pdc2 = d.Udc2 * d.Idc2 / 1000.0f;
    r.Pdc2 = (int32_t)(d.Udc2 * d.Idc2);
    d.needsMissingValues = true;
  }
  // AC power: fallback from V × A when SpotACPower query fails
  if (d.Pac1 == 0.0f && d.Uac1 != 0.0f && d.Iac1 != 0.0f) {
    d.Pac1 = d.Uac1 * d.Iac1 / 1000.0f;
    r.Pac1 = (int32_t)(d.Uac1 * d.Iac1);
    d.needsMissingValues = true;
  }
  if (d.Pac2 == 0.0f && d.Uac2 != 0.0f && d.Iac2 != 0.0f) {
    d.Pac2 = d.Uac2 * d.Iac2 / 1000.0f;
    r.Pac2 = (int32_t)(d.Uac2 * d.Iac2);
    d.needsMissingValues = true;
  }
  if (d.Pac3 == 0.0f && d.Uac3 != 0.0f && d.Iac3 != 0.0f) {
    d.Pac3 = d.Uac3 * d.Iac3 / 1000.0f;
    r.Pac3 = (int32_t)(d.Uac3 * d.Iac3);
    d.needsMissingValues = true;
  }
}

// ============================================================
//  Sensor publishing (called from main ESPHome loop)
// ============================================================

void SmaInverterDevice::publish_sensor(sensor::Sensor *s, float v) {
  if (s != nullptr && v >= 0.0f) s->publish_state(v);
}

void SmaInverterDevice::publish_sensor(sensor::Sensor *s, uint64_t v) {
  if (s != nullptr) s->publish_state((float)v);
}

#ifdef USE_TEXT_SENSOR
void SmaInverterDevice::publish_sensor(text_sensor::TextSensor *s, const std::string &v) {
  if (s != nullptr && !v.empty()) s->publish_state(v);
}
#endif

#ifdef USE_BINARY_SENSOR
void SmaInverterDevice::publish_sensor(binary_sensor::BinarySensor *s, bool v) {
  if (s != nullptr) s->publish_state(v);
}
#endif

bool SmaInverterDevice::publish_sensors() {
  // Create auto-sensors on main loop if BT task flagged it
  bool sensors_created = false;
  if (pending_auto_sensors_) {
    pending_auto_sensors_ = false;
    create_auto_sensors(pending_auto_sensor_prefix_);
    sensors_created = true;
  }

  // Compute EToday from ETotal delta if inverter doesn't report MeteringDyWhOut
  if (inv_data_.ETotal > 0) {
    if (inv_data_.EToday > 0) {
      // Inverter reports daily energy natively
      publish_sensor(today_production_, disp_data_.EToday);
    } else {
      // Compute from ETotal baseline — reset at midnight
      time_t now = time(nullptr);
      struct tm tm_now;
      localtime_r(&now, &tm_now);
      uint8_t today = (now > 86400) ? tm_now.tm_mday : 0;  // 0 if time not synced yet

      if (today > 0 && today != etotal_baseline_day_) {
        // New day (or first run) — set baseline
        etotal_baseline_ = inv_data_.ETotal;
        etotal_baseline_day_ = today;
        ESP_LOGD(TAG, "[%s] EToday baseline reset: ETotal=%llu Wh (day=%u)",
                 mac_string_.c_str(), (unsigned long long)etotal_baseline_, today);
      }
      if (etotal_baseline_ > 0 && inv_data_.ETotal >= etotal_baseline_) {
        float e_today_kwh = (float)(inv_data_.ETotal - etotal_baseline_) / 1000.0f;
        publish_sensor(today_production_, e_today_kwh);
      }
    }
  }
  publish_sensor(total_energy_production_, disp_data_.ETotal);
  publish_sensor(grid_frequency_sensor_, disp_data_.GridFreq);

  publish_sensor(pvs_[0].voltage, disp_data_.Udc1);
  publish_sensor(pvs_[0].current, disp_data_.Idc1);
  publish_sensor(pvs_[0].active_power, (float)inv_data_.Pdc1);  // W (not kW)

  publish_sensor(pvs_[1].voltage, disp_data_.Udc2);
  publish_sensor(pvs_[1].current, disp_data_.Idc2);
  publish_sensor(pvs_[1].active_power, (float)inv_data_.Pdc2);  // W (not kW)

  publish_sensor(phases_[0].voltage, disp_data_.Uac1);
  publish_sensor(phases_[0].current, disp_data_.Iac1);
  publish_sensor(phases_[0].active_power, (float)inv_data_.Pac1);  // W (not kW)

  publish_sensor(phases_[1].voltage, disp_data_.Uac2);
  publish_sensor(phases_[1].current, disp_data_.Iac2);
  publish_sensor(phases_[1].active_power, (float)inv_data_.Pac2);  // W (not kW)

  publish_sensor(phases_[2].voltage, disp_data_.Uac3);
  publish_sensor(phases_[2].current, disp_data_.Iac3);
  publish_sensor(phases_[2].active_power, (float)inv_data_.Pac3);  // W (not kW)

  // Total AC power: use TotalPac if available, otherwise sum per-phase
  {
    float total_w = (float)inv_data_.TotalPac;
    if (total_w == 0.0f) {
      total_w = (float)(inv_data_.Pac1 + inv_data_.Pac2 + inv_data_.Pac3);
    }
    publish_sensor(total_ac_power_, total_w);
  }

  // Only publish temperature if inverter supports it (non-zero)
  if (disp_data_.InvTemp > 0.0f) {
    publish_sensor(inverter_module_temp_, disp_data_.InvTemp);
  }
  publish_sensor(bt_signal_strength_, disp_data_.BTSigStrength);

  // Publish operation/feed-in time (0 is valid — inverter just started)
  publish_sensor(today_generation_time_, (float)inv_data_.OperationTime / 3600.0f);
  publish_sensor(total_generation_time_, (float)inv_data_.FeedInTime / 3600.0f);
  publish_sensor(wakeup_time_, (uint64_t)inv_data_.WakeupTime);

#ifdef USE_TEXT_SENSOR
  publish_sensor(status_text_sensor_,
                 lookup_status_code(inv_data_.DevStatus));
  publish_sensor(serial_number_, inv_data_.DeviceName);
  publish_sensor(software_version_, inv_data_.SWVersion);
  publish_sensor(device_type_,
                 lookup_status_code(inv_data_.DeviceType));
  publish_sensor(device_class_,
                 lookup_status_code(inv_data_.DeviceClass));
  publish_sensor(inverter_time_sensor_, inv_data_.InverterTimestamp);
  publish_sensor(mac_address_sensor_, mac_string_);

  // Raw data JSON for debugging
  if (raw_json_ != nullptr) {
    char buf[1024];
    snprintf(buf, sizeof(buf),
      "{\"Serial\":%lu,\"SUSyID\":%u,\"NetID\":%u,"
      "\"Pac\":%ld,\"Pac1\":%ld,\"Pac2\":%ld,\"Pac3\":%ld,"
      "\"Uac1\":%ld,\"Uac2\":%ld,\"Uac3\":%ld,"
      "\"Iac1\":%ld,\"Iac2\":%ld,\"Iac3\":%ld,"
      "\"Pdc1\":%ld,\"Pdc2\":%ld,\"Udc1\":%ld,\"Udc2\":%ld,\"Idc1\":%ld,\"Idc2\":%ld,"
      "\"Freq\":%ld,\"Temp\":%ld,\"Pmax\":%ld,\"Eta\":%ld,"
      "\"EToday\":%llu,\"ETotal\":%llu,"
      "\"OpTime\":%llu,\"FeedIn\":%llu,"
      "\"GridOut\":%lu,\"GridIn\":%lu,"
      "\"Status\":%lu,\"DevType\":%lu,\"DevClass\":%lu,\"Relay\":%lu,"
      "\"LastTime\":%ld,\"WakeupTime\":%ld}",
      (unsigned long)inv_data_.Serial, inv_data_.SUSyID, inv_data_.NetID,
      (long)inv_data_.TotalPac, (long)inv_data_.Pac1, (long)inv_data_.Pac2, (long)inv_data_.Pac3,
      (long)inv_data_.Uac1, (long)inv_data_.Uac2, (long)inv_data_.Uac3,
      (long)inv_data_.Iac1, (long)inv_data_.Iac2, (long)inv_data_.Iac3,
      (long)inv_data_.Pdc1, (long)inv_data_.Pdc2, (long)inv_data_.Udc1, (long)inv_data_.Udc2,
      (long)inv_data_.Idc1, (long)inv_data_.Idc2,
      (long)inv_data_.GridFreq, (long)inv_data_.InvTemp, (long)inv_data_.Pmax, (long)inv_data_.Eta,
      (unsigned long long)inv_data_.EToday, (unsigned long long)inv_data_.ETotal,
      (unsigned long long)inv_data_.OperationTime, (unsigned long long)inv_data_.FeedInTime,
      (unsigned long)inv_data_.MeteringGridMsTotWOut, (unsigned long)inv_data_.MeteringGridMsTotWIn,
      (unsigned long)inv_data_.DevStatus, (unsigned long)inv_data_.DeviceType,
      (unsigned long)inv_data_.DeviceClass, (unsigned long)inv_data_.GridRelay,
      (long)inv_data_.LastTime, (long)inv_data_.WakeupTime);
    raw_json_->publish_state(std::string(buf));
  }
#endif
#ifdef USE_BINARY_SENSOR
  // Only publish grid connection if relay status was actually returned
  // (code 51 = Closed/connected, 311 = Open/disconnected, 0 = query not supported)
  if (inv_data_.GridRelay != 0) {
    publish_sensor(grid_relay_, inv_data_.GridRelay == 51);
  }
#endif
  return sensors_created;
}

// ============================================================
//  Auto-sensor creation for autodiscovery mode
// ============================================================

void SmaInverterDevice::create_auto_sensors(const std::string &prefix) {
  if (auto_sensors_created_) return;
  auto_sensors_created_ = true;

  ESP_LOGI(TAG, "Creating auto-sensors for '%s'", prefix.c_str());

#ifdef USE_DEVICES
  // Create a sub-device for this inverter so it appears as a separate device in HA
  auto *dev = new Device();
  char *dev_name = strdup(prefix.c_str());
  dev->set_name(dev_name);
  dev->set_device_id(fnv1a_hash(mac_string_));
  App.register_device(dev);
  ha_device_ = dev;
  ESP_LOGI(TAG, "Registered sub-device '%s' (id=0x%08X)", prefix.c_str(), fnv1a_hash(mac_string_));
#endif

  // Pre-compute entity_fields bitfields for each sensor type
  uint32_t voltage_fields = sma_entity_fields(SMA_DC_IDX_VOLTAGE, SMA_UOM_IDX_V, SMA_ICON_IDX_FLASH);
  uint32_t current_fields = sma_entity_fields(SMA_DC_IDX_CURRENT, SMA_UOM_IDX_A, SMA_ICON_IDX_FLASH);
  uint32_t power_fields = sma_entity_fields(SMA_DC_IDX_POWER, SMA_UOM_IDX_W, SMA_ICON_IDX_FLASH);
  uint32_t freq_fields = sma_entity_fields(SMA_DC_IDX_FREQUENCY, SMA_UOM_IDX_HZ, SMA_ICON_IDX_SINE);
  uint32_t energy_fields = sma_entity_fields(SMA_DC_IDX_ENERGY, SMA_UOM_IDX_KWH, SMA_ICON_IDX_SOLAR);
  uint32_t temp_fields = sma_entity_fields(SMA_DC_IDX_TEMPERATURE, SMA_UOM_IDX_C, SMA_ICON_IDX_THERMO);
  uint32_t signal_fields = sma_entity_fields(SMA_DC_IDX_SIGNAL_STRENGTH, SMA_UOM_IDX_PERCENT, SMA_ICON_IDX_BT);
  uint32_t duration_fields = sma_entity_fields(SMA_DC_IDX_DURATION, SMA_UOM_IDX_H, SMA_ICON_IDX_TIMER);
  // entity_category = 2 (diagnostic) in bits 26-27
  uint32_t diag_fields = sma_entity_fields(0, 0, 0, 2);

  // Helper: create a sensor with metadata
  auto make_sensor = [&](sensor::Sensor **target, const std::string &name,
                         uint32_t fields, sensor::StateClass sc, int8_t decimals) {
    if (*target == nullptr) {
#ifdef USE_DEVICES
      auto *s = new DynamicSensor(name, fields, ha_device_);
#else
      auto *s = new DynamicSensor(name, fields);
#endif
      s->set_state_class(sc);
      s->set_accuracy_decimals(decimals);
      App.register_sensor(s);
      *target = s;
      ESP_LOGD(TAG, "  Sensor '%s' key=0x%08X", name.c_str(), s->get_object_id_hash());
    }
  };

  // AC phase sensors
  for (int i = 0; i < 3; i++) {
    make_sensor(&phases_[i].voltage, "Voltage L" + std::to_string(i + 1),
                voltage_fields, sensor::STATE_CLASS_MEASUREMENT, 1);
    make_sensor(&phases_[i].current, "Current L" + std::to_string(i + 1),
                current_fields, sensor::STATE_CLASS_MEASUREMENT, 2);
    make_sensor(&phases_[i].active_power, "Power L" + std::to_string(i + 1),
                power_fields, sensor::STATE_CLASS_MEASUREMENT, 0);
  }

  // DC PV sensors
  for (int i = 0; i < 2; i++) {
    std::string n = std::to_string(i + 1);
    make_sensor(&pvs_[i].voltage, "DC Voltage MPPT" + n,
                voltage_fields, sensor::STATE_CLASS_MEASUREMENT, 1);
    make_sensor(&pvs_[i].current, "DC Current MPPT" + n,
                current_fields, sensor::STATE_CLASS_MEASUREMENT, 2);
    make_sensor(&pvs_[i].active_power, "DC Power MPPT" + n,
                power_fields, sensor::STATE_CLASS_MEASUREMENT, 0);
  }

  // Scalar sensors
  make_sensor(&total_ac_power_, "Power",
              power_fields, sensor::STATE_CLASS_MEASUREMENT, 0);
  make_sensor(&grid_frequency_sensor_, "Grid Frequency",
              freq_fields, sensor::STATE_CLASS_MEASUREMENT, 2);
  make_sensor(&today_production_, "Energy Today",
              energy_fields, sensor::STATE_CLASS_TOTAL_INCREASING, 2);
  make_sensor(&total_energy_production_, "Total Energy",
              energy_fields, sensor::STATE_CLASS_TOTAL_INCREASING, 1);
  make_sensor(&inverter_module_temp_, "Temperature",
              temp_fields, sensor::STATE_CLASS_MEASUREMENT, 1);
  make_sensor(&bt_signal_strength_, "Signal Strength",
              signal_fields, sensor::STATE_CLASS_MEASUREMENT, 0);
  make_sensor(&today_generation_time_, "Operating Time",
              duration_fields, sensor::STATE_CLASS_TOTAL, 0);
  make_sensor(&total_generation_time_, "Feed-in Time",
              duration_fields, sensor::STATE_CLASS_TOTAL, 0);

  // Text sensors (diagnostic category)
#ifdef USE_TEXT_SENSOR
  {
    auto make_ts = [&](text_sensor::TextSensor **target, const std::string &name,
                       uint32_t fields = 0) {
      if (*target == nullptr) {
#ifdef USE_DEVICES
        auto *s = new DynamicTextSensor(name, fields, ha_device_);
#else
        auto *s = new DynamicTextSensor(name, fields);
#endif
        App.register_text_sensor(s);
        *target = s;
        ESP_LOGD(TAG, "  TextSensor '%s' key=0x%08X", name.c_str(), s->get_object_id_hash());
      }
    };
    make_ts(&status_text_sensor_, "Status");
    make_ts(&serial_number_, "Serial Number", diag_fields);
    make_ts(&software_version_, "Firmware Version", diag_fields);
    make_ts(&device_type_, "Device Type", diag_fields);
    make_ts(&device_class_, "Device Class", diag_fields);
    make_ts(&mac_address_sensor_, "MAC Address", diag_fields);
    make_ts(&raw_json_, "Raw Data", diag_fields);
  }
#endif

  // Binary sensor (grid relay)
#ifdef USE_BINARY_SENSOR
  if (grid_relay_ == nullptr) {
    uint32_t relay_fields = sma_entity_fields(SMA_DC_IDX_POWER, 0, SMA_ICON_IDX_RELAY);
#ifdef USE_DEVICES
    auto *s = new DynamicBinarySensor("Grid Connection", relay_fields, ha_device_);
#else
    auto *s = new DynamicBinarySensor("Grid Connection", relay_fields);
#endif
    App.register_binary_sensor(s);
    grid_relay_ = s;
    ESP_LOGD(TAG, "  BinarySensor 'Grid Connection' key=0x%08X", s->get_object_id_hash());
  }
#endif

  ESP_LOGI(TAG, "Auto-sensors DONE for '%s'. App sensors=%d",
           prefix.c_str(), (int)App.get_sensors().size());
}

}  // namespace sma_bluetooth
}  // namespace esphome
