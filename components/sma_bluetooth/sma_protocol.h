#pragma once

/* MIT License — based on work by Lupo135, darrylb123, keerekeerweere */

#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>

namespace esphome {
namespace sma_bluetooth {

// ---------------------------------------------------------------------------
//  Unit conversion macros
// ---------------------------------------------------------------------------
#define tokWh(value64)     (double)(value64) / 1000.0
#define tokW(value32)      (float)(value32) / 1000.0
#define toW(value32)       (float)(value32) / 1.0
#define toHour(value64)    (double)(value64) / 3600.0
#define toAmp(value32)     (float)(value32) / 1000.0
#define toVolt(value32)    (float)(value32) / 100.0
#define toHz(value32)      (float)(value32) / 100.0
#define toPercent(value32) (float)(value32) / 100.0
#define toTemp(value32)    (float)(value32) / 100.0

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------
#define UG_USER      0x07
#define UG_INSTALLER 0x0A
#define USERGROUP    UG_USER

#define COMMBUFSIZE       2048
#define MAX_PCKT_BUF_SIZE COMMBUFSIZE

#define BTH_L2SIGNATURE 0x656003FF

// NaN sentinel values used by SMA protocol
#define NaN_S16 ((int16_t)0x8000)
#define NaN_U16 ((uint16_t)0xFFFF)
#define NaN_S32 ((int32_t)0x80000000)
#define NaN_U32 ((uint32_t)0xFFFFFFFF)
#define NaN_S64 ((int64_t)0x8000000000000000LL)
#define NaN_U64 ((uint64_t)0xFFFFFFFFFFFFFFFFULL)

inline bool is_NaN(int16_t v)  { return v == NaN_S16; }
inline bool is_NaN(uint16_t v) { return v == NaN_U16; }
inline bool is_NaN(int32_t v)  { return v == NaN_S32; }
inline bool is_NaN(uint32_t v) { return v == NaN_U32; }
inline bool is_NaN(int64_t v)  { return v == NaN_S64; }
inline bool is_NaN(uint64_t v) { return v == NaN_U64; }

// FreeRTOS event group bits for BT state signalling
#define BT_EVT_SPP_INIT     (1 << 0)
#define BT_EVT_DISC_DONE    (1 << 1)
#define BT_EVT_CONNECTED    (1 << 2)
#define BT_EVT_DISCONNECTED (1 << 3)

// Minimum valid time (2025-01-01 UTC) for sanity checks
static const time_t MIN_VALID_TIME = 1735689600;

// ---------------------------------------------------------------------------
//  SMA data types
// ---------------------------------------------------------------------------
enum SMA_DATATYPE {
  DT_ULONG  = 0,
  DT_STATUS = 8,
  DT_STRING = 16,
  DT_FLOAT  = 32,
  DT_SLONG  = 64,
};

// ---------------------------------------------------------------------------
//  Error codes
// ---------------------------------------------------------------------------
enum E_RC {
  E_OK         =  0,
  E_INIT       = -1,
  E_INVPASSW   = -2,
  E_RETRY      = -3,
  E_EOF        = -4,
  E_NODATA     = -5,
  E_OVERFLOW   = -6,
  E_BADARG     = -7,
  E_CHKSUM     = -8,
  E_INVRESP    = -9,
  E_ARCHNODATA = -10,
};

// ---------------------------------------------------------------------------
//  Data query types (bitmask)
// ---------------------------------------------------------------------------
enum getInverterDataType {
  EnergyProduction    = 1 << 0,
  SpotDCPower         = 1 << 1,
  SpotDCVoltage       = 1 << 2,
  SpotACPower         = 1 << 3,
  SpotACVoltage       = 1 << 4,
  SpotGridFrequency   = 1 << 5,
  SpotACTotalPower    = 1 << 8,
  TypeLabel           = 1 << 9,
  OperationTime       = 1 << 10,
  SoftwareVersion     = 1 << 11,
  DeviceStatus        = 1 << 12,
  GridRelayStatus     = 1 << 13,
  BatteryChargeStatus = 1 << 14,
  BatteryInfo         = 1 << 15,
  InverterTemp        = 1 << 16,
  MeteringGridMsTotW  = 1 << 17,
};

// ---------------------------------------------------------------------------
//  LRI (Logical Record Identifier) codes
// ---------------------------------------------------------------------------
enum LriDef {
  OperationHealth                = 0x2148,
  CoolsysTmpNom                 = 0x2377,
  DcMsWatt                      = 0x251E,
  MeteringTotWhOut               = 0x2601,
  MeteringDyWhOut                = 0x2622,
  GridMsTotW                     = 0x263F,
  BatChaStt                      = 0x295A,
  OperationHealthSttOk           = 0x411E,
  OperationHealthSttWrn          = 0x411F,
  OperationHealthSttAlm          = 0x4120,
  OperationGriSwStt              = 0x4164,
  OperationRmgTms                = 0x4166,
  DcMsVol                        = 0x451F,
  DcMsAmp                        = 0x4521,
  MeteringPvMsTotWhOut           = 0x4623,
  MeteringGridMsTotWhOut         = 0x4624,
  MeteringGridMsTotWhIn          = 0x4625,
  MeteringCsmpTotWhIn            = 0x4626,
  MeteringGridMsDyWhOut          = 0x4627,
  MeteringGridMsDyWhIn           = 0x4628,
  MeteringTotOpTms               = 0x462E,
  MeteringTotFeedTms             = 0x462F,
  MeteringGriFailTms             = 0x4631,
  MeteringWhIn                   = 0x463A,
  MeteringWhOut                  = 0x463B,
  MeteringPvMsTotWOut            = 0x4635,
  MeteringGridMsTotWOut_LRI      = 0x4636,
  MeteringGridMsTotWIn_LRI       = 0x4637,
  MeteringCsmpTotWIn             = 0x4639,
  GridMsWphsA                    = 0x4640,
  GridMsWphsB                    = 0x4641,
  GridMsWphsC                    = 0x4642,
  GridMsPhVphsA                  = 0x4648,
  GridMsPhVphsB                  = 0x4649,
  GridMsPhVphsC                  = 0x464A,
  GridMsAphsA_1                  = 0x4650,
  GridMsAphsB_1                  = 0x4651,
  GridMsAphsC_1                  = 0x4652,
  GridMsAphsA                    = 0x4653,
  GridMsAphsB                    = 0x4654,
  GridMsAphsC                    = 0x4655,
  GridMsHz                       = 0x4657,
  MeteringSelfCsmpSelfCsmpWh     = 0x46AA,
  MeteringSelfCsmpActlSelfCsmp   = 0x46AB,
  MeteringSelfCsmpSelfCsmpInc    = 0x46AC,
  MeteringSelfCsmpAbsSelfCsmpInc = 0x46AD,
  MeteringSelfCsmpDySelfCsmpInc  = 0x46AE,
  BatDiagCapacThrpCnt            = 0x491E,
  BatDiagTotAhIn                 = 0x4926,
  BatDiagTotAhOut                = 0x4927,
  BatTmpVal                      = 0x495B,
  BatVol                         = 0x495C,
  BatAmp                         = 0x495D,
  NameplateLocation              = 0x821E,
  NameplateMainModel             = 0x821F,
  NameplateModel                 = 0x8220,
  NameplateAvalGrpUsr            = 0x8221,
  NameplatePkgRev                = 0x8234,
  InverterWLim                   = 0x832A,
};

// ---------------------------------------------------------------------------
//  Inverter data — one instance per inverter device
// ---------------------------------------------------------------------------
struct InverterData {
  uint8_t  btAddress[6]{};
  uint8_t  SUSyID{0x7D};
  uint32_t Serial{0};
  uint8_t  NetID{0};
  int32_t  Pmax{0};
  int32_t  TotalPac{0};
  int32_t  Pac{0};
  int32_t  Pac1{0};
  int32_t  Pac2{0};
  int32_t  Pac3{0};
  int32_t  Uac1{0};
  int32_t  Uac2{0};
  int32_t  Uac3{0};
  int32_t  Iac1{0};
  int32_t  Iac2{0};
  int32_t  Iac3{0};
  int32_t  Pdc1{0};
  int32_t  Pdc2{0};
  int32_t  Udc1{0};
  int32_t  Udc2{0};
  int32_t  Idc1{0};
  int32_t  Idc2{0};
  int32_t  GridFreq{0};
  int32_t  Eta{0};
  int32_t  InvTemp{0};
  uint64_t EToday{0};
  uint64_t ETotal{0};
  time_t   LastTime{0};
  uint64_t OperationTime{0};
  uint64_t FeedInTime{0};
  int32_t  DevStatus{0};
  int32_t  GridRelay{0};
  E_RC     status{E_OK};
  uint32_t MeteringGridMsTotWOut{0};
  uint32_t MeteringGridMsTotWIn{0};
  time_t   WakeupTime{0};
  std::string DeviceName;
  std::string SWVersion;
  uint32_t DeviceType{0};
  uint32_t DeviceClass{0};
  std::string InverterTimestamp;
};

// ---------------------------------------------------------------------------
//  Display data — converted to human-readable units, one per inverter
// ---------------------------------------------------------------------------
struct DisplayData {
  float BTSigStrength{0};
  float Pmax{0};
  float TotalPac{0};
  float Pac{0};
  float Pac1{0};
  float Pac2{0};
  float Pac3{0};
  float Uac1{0};
  float Uac2{0};
  float Uac3{0};
  float Iac1{0};
  float Iac2{0};
  float Iac3{0};
  float InvTemp{0};
  float Pdc1{0};
  float Pdc2{0};
  float Udc1{0};
  float Udc2{0};
  float Idc1{0};
  float Idc2{0};
  float GridFreq{0};
  float EToday{0};
  float ETotal{0};
  bool  needsMissingValues{false};
};

// ---------------------------------------------------------------------------
//  L1 packet header (18 bytes, packed)
// ---------------------------------------------------------------------------
#pragma pack(push, 1)
struct L1Hdr {
  uint8_t  SOP;              // 0x7E
  uint16_t pkLength;         // little-endian
  uint8_t  pkChecksum;       // XOR of first 3 bytes
  uint8_t  SourceAddr[6];
  uint8_t  DestinationAddr[6];
  uint16_t command;           // little-endian
};
#pragma pack(pop)

// ---------------------------------------------------------------------------
//  FCS-16 lookup table (CRC used by SMA protocol)
// ---------------------------------------------------------------------------
static const uint16_t FCS_TABLE[256] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78,
};

// ---------------------------------------------------------------------------
//  Protocol helper functions (stateless)
// ---------------------------------------------------------------------------

inline uint16_t fcs16_update(uint16_t fcs, uint8_t byte) {
  return (fcs >> 8) ^ FCS_TABLE[(fcs ^ byte) & 0xFF];
}

inline uint16_t get_u16(const uint8_t *buf) {
  return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

inline uint32_t get_u32(const uint8_t *buf) {
  return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
         ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

inline uint64_t get_u64(const uint8_t *buf) {
  return (uint64_t)get_u32(buf) | ((uint64_t)get_u32(buf + 4) << 32);
}

inline uint32_t get_attribute(const uint8_t *recptr) {
  uint8_t data_type = recptr[0] >> 4;  // upper nibble
  if (data_type == DT_STATUS) {
    return get_u32(recptr + 16);
  }
  return get_u32(recptr + 8);
}

inline void get_version(uint32_t version, char *out) {
  snprintf(out, 24, "%d.%d.%d.%c",
           (version >> 24) & 0xFF,
           (version >> 16) & 0xFF,
           (version >> 8) & 0xFF,
           'A' + (version & 0xFF));
}

inline uint8_t print_unix_time(char *buf, time_t t) {
  struct tm tm_info;
  gmtime_r(&t, &tm_info);
  return strftime(buf, 24, "%Y-%m-%d %H:%M:%S", &tm_info);
}

// Status code table entry
struct StatusCode {
  uint16_t    code;
  const char *message;
};

// Status codes lookup table
static const StatusCode STATUS_CODES[] = {
    {50,   "Status"},
    {51,   "Closed"},
    {300,  "Nat"},
    {301,  "Grid failure"},
    {302,  "-------"},
    {303,  "Off"},
    {304,  "Island mode"},
    {305,  "Island mode"},
    {307,  "OK"},
    {308,  "On"},
    {309,  "Operation"},
    {310,  "General operating mode"},
    {311,  "Open"},
    {312,  "Phase assignment"},
    {358,  "SB 4000TL-20"},
    {359,  "SB 5000TL-20"},
    {558,  "SB 3000TL-20"},
    {6109, "SB 1600TL-10"},
    {9109, "SB 1600TL-10"},
    {8001, "Solar Inverters"},
};

inline const char *lookup_status_code(uint16_t code) {
  for (const auto &entry : STATUS_CODES) {
    if (entry.code == code) return entry.message;
  }
  return "Unknown";
}

// Query type tables — which data types to query in each cycle
static const getInverterDataType FAST_QUERY_TYPES[] = {
    SpotDCPower, SpotDCVoltage, SpotACPower,
    SpotACTotalPower, SpotACVoltage, EnergyProduction,
    SpotGridFrequency, OperationTime,
};
static const int NUM_FAST_TYPES = sizeof(FAST_QUERY_TYPES) / sizeof(FAST_QUERY_TYPES[0]);

static const getInverterDataType SLOW_QUERY_TYPES[] = {
    DeviceStatus, GridRelayStatus, InverterTemp,
};
static const int NUM_SLOW_TYPES = sizeof(SLOW_QUERY_TYPES) / sizeof(SLOW_QUERY_TYPES[0]);

static const getInverterDataType ONCE_QUERY_TYPES[] = {
    TypeLabel, SoftwareVersion,
};
static const int NUM_ONCE_TYPES = sizeof(ONCE_QUERY_TYPES) / sizeof(ONCE_QUERY_TYPES[0]);

// Data types where errors are non-fatal (inverter may not support them)
static const getInverterDataType IGNORABLE_ERROR_TYPES[] = {
    InverterTemp, MeteringGridMsTotW,
};
static const int NUM_IGNORABLE = sizeof(IGNORABLE_ERROR_TYPES) / sizeof(IGNORABLE_ERROR_TYPES[0]);

inline bool is_ignorable_error(getInverterDataType dt) {
  for (int i = 0; i < NUM_IGNORABLE; i++) {
    if (IGNORABLE_ERROR_TYPES[i] == dt) return true;
  }
  return false;
}

// Cycle intervals
static const int SLOW_EVERY = 6;   // slow queries every 6th cycle (~60s at 10s/cycle)
static const int DIAG_EVERY = 30;  // diagnostics every 30th cycle (~5 min)

// Constant byte arrays
static const uint8_t ZEROS_6[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t FFS_6[6]   = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

}  // namespace sma_bluetooth
}  // namespace esphome
