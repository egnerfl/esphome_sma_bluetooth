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

  // Bitmask tracking which data categories were actually read this cycle.
  // Prevents publishing zero-initialized values that were never received.
  uint32_t data_received{0};
  static constexpr uint32_t DATA_AC_POWER    = (1 << 0);
  static constexpr uint32_t DATA_AC_VOLTAGE  = (1 << 1);
  static constexpr uint32_t DATA_DC_POWER    = (1 << 2);
  static constexpr uint32_t DATA_DC_VOLTAGE  = (1 << 3);
  static constexpr uint32_t DATA_ENERGY      = (1 << 4);
  static constexpr uint32_t DATA_FREQUENCY   = (1 << 5);
  static constexpr uint32_t DATA_TEMPERATURE = (1 << 6);
  static constexpr uint32_t DATA_STATUS      = (1 << 7);
  static constexpr uint32_t DATA_OPTIME      = (1 << 8);
  static constexpr uint32_t DATA_AC_TOTAL    = (1 << 9);
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
  uint32_t    code;
  const char *message;
};

// Status/device type/device class lookup table (from SBFspot TagListEN-US.txt)
static const StatusCode STATUS_CODES[] = {
    // Low-range codes
    {0, "OK"},
    {35, "Fault"},
    {38, "Current"},
    {43, "Not Adjusted"},
    {44, "External"},
    {45, "External 2"},
    {46, "Battery"},
    {48, "Interior"},
    {49, "Function"},
    // Status codes (50–319)
    {50, "Status"},
    {51, "Closed"},
    {300, "Nat"},
    {301, "Grid failure"},
    {302, "---"},
    {303, "Off"},
    {304, "Island mode"},
    {305, "Island mode"},
    {306, "Island mode 60Hz"},
    {307, "OK"},
    {308, "On"},
    {309, "Operation"},
    {310, "General operating mode"},
    {311, "Open"},
    {312, "Phase assignment"},
    {313, "Island mode 50Hz"},
    {314, "Power limitation"},
    {315, "Max. active power"},
    {316, "Active power mode"},
    {317, "All phases"},
    {318, "Overload"},
    {319, "Overtemperature"},
    {350, "Remaining time"},
    {351, "Reactive power"},
    {352, "RD1663/661"},
    {353, "Reset event counter"},
    {354, "Reset max values"},
    {355, "Reset energy log"},
    {356, "Reset permanent stop"},
    {357, "Reset counter"},
    {358, "SB 4000TL-20"},
    {359, "SB 5000TL-20"},
    {558, "SB 3000TL-20"},
    {6109, "SB 1600TL-10"},
    // Device class codes (8000–8128)
    {8000, "All devices"},
    {8001, "Solar Inverters"},
    {8002, "Wind Turbine Inverter"},
    {8007, "Battery Inverter"},
    {8008, "Consumer"},
    {8009, "Sensor system"},
    {8033, "Electricity Meter"},
    {8064, "Communication product"},
    {8065, "PV Inverter (498)"},
    {8066, "Dev Class 66"},
    {8067, "Dev Class 67"},
    {8096, "Dev Class 96"},
    {8128, "Hybrid Inverter"},
    // Device type codes (9000–9498)
    {9000, "SWR 700"}, {9001, "SWR 850"}, {9002, "SWR 850E"},
    {9003, "SWR 1100"}, {9004, "SWR 1100E"}, {9005, "SWR 1100LV"},
    {9006, "SWR 1500"}, {9007, "SWR 1600"}, {9008, "SWR 1700E"},
    {9009, "SWR 1800U"}, {9010, "SWR 2000"}, {9011, "SWR 2400"},
    {9012, "SWR 2500"}, {9013, "SWR 2500U"}, {9014, "SWR 3000"},
    {9015, "SB 700"}, {9016, "SB 700U"}, {9017, "SB 1100"},
    {9018, "SB 1100U"}, {9019, "SB 1100LV"}, {9020, "SB 1700"},
    {9021, "SB 1900TLJ"}, {9022, "SB 2100TL"}, {9023, "SB 2500"},
    {9024, "SB 2800"}, {9025, "SB 2800i"}, {9026, "SB 3000"},
    {9027, "SB 3000US"}, {9028, "SB 3300"}, {9029, "SB 3300U"},
    {9030, "SB 3300TL"}, {9031, "SB 3300TL HC"}, {9032, "SB 3800"},
    {9033, "SB 3800U"}, {9034, "SB 4000US"}, {9035, "SB 4200TL"},
    {9036, "SB 4200TL HC"}, {9037, "SB 5000TL"}, {9038, "SB 5000TLW"},
    {9039, "SB 5000TL HC"}, {9040, "Convert 2700"},
    {9041, "SMC 4600A"}, {9042, "SMC 5000"}, {9043, "SMC 5000A"},
    {9044, "SB 5000US"}, {9045, "SMC 6000"}, {9046, "SMC 6000A"},
    {9047, "SB 6000US"}, {9048, "SMC 6000UL"}, {9049, "SMC 6000TL"},
    {9050, "SMC 6500A"}, {9051, "SMC 7000A"}, {9052, "SMC 7000HV"},
    {9053, "SB 7000US"}, {9054, "SMC 7000TL"}, {9055, "SMC 8000TL"},
    {9056, "SMC 9000TL"}, {9057, "SMC 10000TL"}, {9058, "SMC 11000TL"},
    {9059, "SB 3000 K"}, {9060, "Unknown inverter"},
    {9061, "Sensorbox"}, {9062, "SMC 11000TLRP"}, {9063, "SMC 10000TLRP"},
    {9064, "SMC 9000TLRP"}, {9065, "SMC 7000HVRP"}, {9066, "SB 1200"},
    {9067, "STP 10000TL-10"}, {9068, "STP 12000TL-10"},
    {9069, "STP 15000TL-10"}, {9070, "STP 17000TL-10"},
    {9071, "SB 2000HF-30"}, {9072, "SB 2500HF-30"}, {9073, "SB 3000HF-30"},
    {9074, "SB 3000TL-21"}, {9075, "SB 4000TL-21"}, {9076, "SB 5000TL-21"},
    {9077, "SB 2000HFUS-30"}, {9078, "SB 2500HFUS-30"},
    {9079, "SB 3000HFUS-30"}, {9080, "SB 8000TLUS"},
    {9081, "SB 9000TLUS"}, {9082, "SB 10000TLUS"}, {9083, "SB 8000US"},
    {9084, "WB 3600TL-20"}, {9085, "WB 5000TL-20"},
    {9086, "SB 3800US-10"}, {9087, "Sunny Beam BT11"},
    {9088, "SC 500CP"}, {9089, "SC 630CP"}, {9090, "SC 800CP"},
    {9091, "SC 250U"}, {9092, "SC 500U"}, {9093, "SC 500HEUS"},
    {9094, "SC 760CP"}, {9095, "SC 720CP"}, {9096, "SC 910CP"},
    {9097, "SMU"}, {9098, "STP 5000TL-20"}, {9099, "STP 6000TL-20"},
    {9100, "STP 7000TL-20"}, {9101, "STP 8000TL-10"},
    {9102, "STP 9000TL-20"}, {9103, "STP 8000TL-20"},
    {9104, "SB 3000TL-JP-21"}, {9105, "SB 3500TL-JP-21"},
    {9106, "SB 4000TL-JP-21"}, {9107, "SB 4500TL-JP-21"},
    {9108, "SCSMC"}, {9109, "SB 1600TL-10"}, {9110, "SSMUS"},
    {9111, "Remote SOC"}, {9112, "WB 2000HF-30"},
    {9113, "WB 2500HF-30"}, {9114, "WB 3000HF-30"},
    {9115, "WB 2000HFUS-30"}, {9116, "WB 2500HFUS-30"},
    {9117, "WB 3000HFUS-30"}, {9118, "VIEW-10"},
    {9119, "Sunny HomeManager"}, {9120, "SMID"},
    {9121, "SC 800HE-20"}, {9122, "SC 630HE-20"}, {9123, "SC 500HE-20"},
    {9124, "SC 720HE-20"}, {9125, "SC 760HE-20"},
    {9126, "SMC 6000A-11"}, {9127, "SMC 5000A-11"},
    {9128, "SMC 4600A-11"}, {9129, "SB 3800-11"}, {9130, "SB 3300-11"},
    {9131, "STP 20000TL-10"}, {9132, "CT Meter"},
    {9133, "SB 2000HFUS-32"}, {9134, "SB 2500HFUS-32"},
    {9135, "SB 3000HFUS-32"}, {9136, "WB 2000HFUS-32"},
    {9137, "WB 2500HFUS-32"}, {9138, "WB 3000HFUS-32"},
    {9139, "STP 20000TLHE-10"}, {9140, "STP 15000TLHE-10"},
    {9141, "SB 3000US-12"}, {9142, "SB 3800-US-12"},
    {9143, "SB 4000US-12"}, {9144, "SB 5000US-12"},
    {9145, "SB 6000US-12"}, {9146, "SB 7000US-12"},
    {9147, "SB 8000US-12"}, {9148, "SB 8000TLUS-12"},
    {9149, "SB 9000TLUS-12"}, {9150, "SB 10000TLUS-12"},
    {9151, "SB 11000TLUS-12"}, {9152, "SB 7000TLUS-12"},
    {9153, "SB 6000TLUS-12"}, {9154, "SB 1300TL-10"},
    {9155, "SBU 2200"}, {9156, "SBU 5000"},
    {9157, "SI 2012"}, {9158, "SI 2224"}, {9159, "SI 5048"},
    {9160, "SB 3600TL-20"}, {9161, "SB 3000TL-JP-22"},
    {9162, "SB 3500TL-JP-22"}, {9163, "SB 4000TL-JP-22"},
    {9164, "SB 4500TL-JP-22"}, {9165, "SB 3600TL-21"},
    {9166, "LUFFT"}, {9167, "Cluster Controller"},
    {9168, "SC 630HE-11"}, {9169, "SC 500HE-11"}, {9170, "SC 400HE-11"},
    {9171, "WB 3000TL-21"}, {9172, "WB 3600TL-21"},
    {9173, "WB 4000TL-21"}, {9174, "WB 5000TL-21"},
    {9175, "SC 250"}, {9176, "SMA Meteo Station"}, {9177, "SB 240-10"},
    {9178, "SB 240-US-10"}, {9179, "Multigate-10"},
    {9180, "Multigate-US-10"}, {9181, "STP 20000TLEE-10"},
    {9182, "STP 15000TLEE-10"}, {9183, "SB 2000TLST-21"},
    {9184, "SB 2500TLST-21"}, {9185, "SB 3000TLST-21"},
    {9186, "WB 2000TLST-21"}, {9187, "WB 2500TLST-21"},
    {9188, "WB 3000TLST-21"}, {9189, "WTP 5000TL-20"},
    {9190, "WTP 6000TL-20"}, {9191, "WTP 7000TL-20"},
    {9192, "WTP 8000TL-20"}, {9193, "WTP 9000TL-20"},
    {9194, "STP 12kTL-US-10"}, {9195, "STP 15kTL-US-10"},
    {9196, "STP 20kTL-US-10"}, {9197, "STP 24kTL-US-10"},
    {9198, "SB 3000TL-US-22"}, {9199, "SB 3800TL-US-22"},
    {9200, "SB 4000TL-US-22"}, {9201, "SB 5000TL-US-22"},
    {9202, "WB 3000TL-US-22"}, {9203, "WB 3800TL-US-22"},
    {9204, "WB 4000TL-US-22"}, {9205, "WB 5000TL-US-22"},
    {9206, "SC 500CP-JP"}, {9207, "SC 850CP"}, {9208, "SC 900CP"},
    {9209, "SC 850 CP-US"}, {9210, "SC 900 CP-US"}, {9211, "SC 619CP"},
    {9212, "SMA Meteo Station Pro"}, {9213, "SC 800 CP-US"},
    {9214, "SC 630 CP-US"}, {9215, "SC 500 CP-US"},
    {9216, "SC 720 CP-US"}, {9217, "SC 750 CP-US"},
    {9218, "SB 240 Dev"}, {9219, "SB 240-US BTF"},
    {9220, "Grid Gate-20"}, {9221, "SC 500 CP-US/600V"},
    {9222, "STP 10kTL-JP-10"}, {9223, "SI 6.0H"}, {9224, "SI 8.0H"},
    {9225, "SB 5000SE-10"}, {9226, "SB 3600SE-10"},
    {9227, "SC 800CP-JP"}, {9228, "SC 630CP-JP"},
    {9229, "WebBox-30"}, {9230, "Power Reducer Box"}, {9231, "S0 Box"},
    {9232, "SBC"}, {9233, "SBC+"}, {9234, "SBCL"},
    {9235, "SC 100 outdoor"}, {9236, "SC 1000MV"}, {9237, "SC 100LV"},
    {9238, "SC 1120MV"}, {9239, "SC 125LV"}, {9240, "SC 150"},
    {9241, "SC 200"}, {9242, "SC 200HE"}, {9243, "SC 250HE"},
    {9244, "SC 350"}, {9245, "SC 350HE"}, {9246, "SC 400HE-11"},
    {9247, "SC 400MV"}, {9248, "SC 500HE"}, {9249, "SC 500MV"},
    {9250, "SC 560HE"}, {9251, "SC 630HE-11"}, {9252, "SC 700MV"},
    {9253, "SCBFS"}, {9254, "SI 3324"}, {9255, "SI 4.0M"},
    {9256, "SI 4248"}, {9257, "SI 4248U"}, {9258, "SI 4500"},
    {9259, "SI 4548U"}, {9260, "SI 5.4M"}, {9261, "SI 5048U"},
    {9262, "SI 6048U"}, {9263, "SMC 7000HV-11"}, {9264, "Solar Tracker"},
    {9265, "Sunny Beam"}, {9266, "SWR 700/150"}, {9267, "SWR 700/200"},
    {9268, "SWR 700/250"}, {9269, "WebBox SC"}, {9270, "WebBox-10"},
    {9271, "STP 20kTL-JP-11"}, {9272, "STP 10kTL-JP-11"},
    {9273, "SB 6000TL-21"}, {9274, "SB 6000TL-US-22"},
    {9275, "SB 7000TL-US-22"}, {9276, "SB 7600TL-US-22"},
    {9277, "SB 8000TL-US-22"}, {9278, "SI 3.0M"}, {9279, "SI 4.4M"},
    {9280, "Non-SMA Consumer"}, {9281, "STP 10000TL-20"},
    {9282, "STP 11000TL-20"}, {9283, "STP 12000TL-20"},
    {9284, "STP 20000TL-30"}, {9285, "STP 25000TL-30"},
    {9286, "SCS-500"}, {9287, "SCS-630"}, {9288, "SCS-720"},
    {9289, "SCS-760"}, {9290, "SCS-800"}, {9291, "SCS-850"},
    {9292, "SCS-900"}, {9293, "SB 7700TL-US-22"},
    {9294, "SB 20.0-3SP-40"}, {9295, "SB 30.0-3SP-40"},
    {9296, "SC 1000 CP"}, {9297, "Zeversolar 1000"},
    {9298, "SC 2200-10"}, {9299, "SC 2200-US-10"},
    {9300, "SC 2475-EV-10"}, {9301, "SB 1.5-1VL-40"},
    {9302, "SB 2.5-1VL-40"}, {9303, "SB 2.0-1VL-40"},
    {9304, "SB 5.0-1SP-US-40"}, {9305, "SB 6.0-1SP-US-40"},
    {9306, "SB 7.7-1SP-US-40"}, {9307, "Energy Meter"},
    {9308, "Zone Monitoring"}, {9309, "STP 27kTL-US-10"},
    {9310, "STP 30kTL-US-10"}, {9311, "STP 25kTL-JP-30"},
    {9312, "SSM30"}, {9313, "SB 50.0-3SP-40"},
    {9314, "Plugwise Circle"}, {9315, "Plugwise Sting"},
    {9316, "SCS-1000"}, {9317, "SB 5400TL-JP-22"},
    {9318, "SCS-1000-KRBS"}, {9319, "SB 3.0-1AV-40"},
    {9320, "SB 3.6-1AV-40"}, {9321, "SB 4.0-1AV-40"},
    {9322, "SB 5.0-1AV-40"}, {9323, "Multibox-12-2"},
    {9324, "SBS 1.5-1VL-10"}, {9325, "SBS 2.0-1VL-10"},
    {9326, "SBS 2.5-1VL-10"}, {9327, "SMA Energy Meter"},
    {9328, "SB 3.0-1SP-US-40"}, {9329, "SB 3.8-1SP-US-40"},
    {9330, "SB 7.0-1SP-US-40"}, {9331, "SI 3.0M-12"},
    {9332, "SI 4.4M-12"}, {9333, "SI 6.0H-12"}, {9334, "SI 8.0H-12"},
    {9335, "SMA Com Gateway"}, {9336, "STP 15000TL-30"},
    {9337, "STP 17000TL-30"}, {9338, "STP 50-40"},
    {9339, "STP 50-US-40"}, {9340, "STP 50-JP-40"},
    {9341, "Edimax SP-2101W"}, {9342, "Edimax SP-2110W"},
    {9343, "Sunny HomeManager 2.0"}, {9344, "STP 4.0-3AV-40"},
    {9345, "STP 5.0-3AV-40"}, {9346, "STP 6.0-3AV-40"},
    {9347, "STP 8.0-3AV-40"}, {9348, "STP 10.0-3AV-40"},
    {9349, "STP 4.0-3SP-40"}, {9350, "STP 5.0-3SP-40"},
    {9351, "STP 6.0-3SP-40"}, {9352, "STP 8.0-3SP-40"},
    {9353, "STP 10.0-3SP-40"}, {9354, "STP 24.5kTL-JP-30"},
    {9355, "STP 20kTL-JP-30"}, {9356, "SBS 3.7-10"},
    {9358, "SBS 5.0-10"}, {9359, "SBS 6.0-10"},
    {9360, "SBS 3.8-US-10"}, {9361, "SBS 5.0-US-10"},
    {9362, "SBS 6.0-US-10"}, {9363, "SBS 4.0-JP-10"},
    {9364, "SBS 5.0-JP-10"}, {9365, "Edimax SP-2101W V2"},
    {9366, "STP 3.0-3AV-40"}, {9367, "STP 3.0-3SP-40"},
    {9368, "TIGO TS4-M"}, {9369, "TIGO TS4-S"},
    {9370, "TIGO TS4-O"}, {9371, "TIGO TS4-L"},
    {9372, "TIGO ES75"}, {9373, "TIGO 2ES"},
    {9374, "TIGO JBOX"}, {9375, "Wattnote Modbus RTU"},
    {9376, "SunSpec Modbus RTU"}, {9377, "SMA TGIB"},
    {9378, "SMA CelMdm"}, {9379, "SC 1850-US-10"},
    {9380, "SC 2500-US-EV-10"}, {9381, "SC 2940-10"},
    {9382, "SC 2940-US-10"}, {9383, "SCS 2200-10"},
    {9384, "SCS 2500-EV-10"}, {9385, "SCS 2500-EV-US-10"},
    {9386, "SCS 2200-US-10"}, {9387, "SC 2750-EV-10"},
    {9388, "SC 2750-EV-US-10"}, {9389, "SCS 2750-EV-10"},
    {9390, "SCS 2750-EV-US-10"}, {9391, "SCS 2475-10"},
    {9392, "SCS 2475-US-10"}, {9393, "SB/STP Demo"},
    {9394, "TIGO GW"}, {9395, "TIGO GW2"},
    {9396, "SMA DATA MANAGER M"}, {9397, "EDMM-10"},
    {9398, "EDMM-US-10"}, {9399, "EMETER-30"},
    {9400, "EDMS-US-10"}, {9401, "SB 3.0-1AV-41"},
    {9402, "SB 3.6-1AV-41"}, {9403, "SB 4.0-1AV-41"},
    {9404, "SB 5.0-1AV-41"}, {9405, "SB 6.0-1AV-41"},
    {9406, "SHP 100k-20"}, {9407, "SHP 150k-20"},
    {9408, "SHP 125k-US-20"}, {9409, "SHP 150k-US-20"},
    {9410, "SHP 100k-JP-20"}, {9411, "SHP 150k-JP-20"},
    {9412, "SC 2475-10"}, {9413, "SC 3000-EV-10"},
    {9414, "SCS 3000-EV-10"}, {9415, "SC 1760-US-10"},
    {9416, "SC 2000-US-10"}, {9417, "SC 2500-EV-10"},
    {9418, "SCS 1900-10"}, {9419, "SCS 1900-US-10"},
    {9420, "SCS 2900-10"}, {9421, "SCS 2900-US-10"},
    {9422, "SB 3.0-1AV-US-42"}, {9423, "SB 3.8-1AV-US-42"},
    {9424, "SB 4.8-1AV-US-42"}, {9425, "SB 5.8-1AV-US-42"},
    {9426, "SB 4.4-1AV-JP-42"}, {9427, "SB 5.5-1AV-JP-42"},
    {9428, "STP 62-US-41"}, {9429, "STP 50-US-41"},
    {9430, "STP 33-US-41"}, {9431, "STP 50-41"},
    {9432, "STP 50-JP-41"}, {9433, "SC 2000-EV-US-10"},
    {9434, "SMA DATA MANAGER L"}, {9435, "EDML-10"},
    {9436, "EDML-US-10"}, {9437, "SMA Revenue Grade Meter"},
    {9438, "SMASpot"}, {9439, "SB 3.0-1SP-US-41"},
    {9440, "SB 3.8-1SP-US-41"}, {9441, "SB 5.0-1SP-US-41"},
    {9442, "SB 6.0-1SP-US-41"}, {9443, "SB 7.0-1SP-US-41"},
    {9444, "SB 7.7-1SP-US-41"}, {9445, "SB 3.0-1TP-US-41"},
    {9446, "SB 3.8-1TP-US-41"}, {9447, "SB 5.0-1TP-US-41"},
    {9448, "SB 6.0-1TP-US-41"}, {9449, "SB 7.0-1TP-US-41"},
    {9450, "SB 7.7-1TP-US-41"}, {9451, "SLQ 15.0-3AV-41"},
    {9452, "SLQ 18.0-3AV-41"}, {9453, "SLQ 20.0-3AV-41"},
    {9454, "SLQ 23.0-3AV-41"}, {9455, "SB 5.5-LV-JP-41"},
    {9456, "STPS 60-10"}, {9457, "SC 4000-UP"},
    {9458, "SC 4000-UP-US"}, {9459, "SCS 3450-UP-XT"},
    {9460, "SCS 3450-UP-XT-US"}, {9461, "SC 4200-UP"},
    {9462, "SC 4200-UP-US"}, {9463, "SCS 3600-UP-XT"},
    {9464, "SCS 3600-UP-XT-US"}, {9465, "SC 4400-UP"},
    {9466, "SC 4400-UP-US"}, {9467, "SCS 3800-UP-XT"},
    {9468, "SCS 3800-UP-XT-US"}, {9469, "SC 4600-UP"},
    {9470, "SC 4600-UP-US"}, {9471, "SCS 3950-UP-XT"},
    {9472, "SCS 3950-UP-XT-US"}, {9473, "SMA Energy Meter 3"},
    {9474, "SI 4.4M-13"}, {9475, "SI 6.0H-13"}, {9476, "SI 8.0H-13"},
    {9477, "SHP 143k-JP-20"}, {9478, "FRITZ DECT 200"},
    {9479, "FRITZ DECT 210"}, {9480, "STP 60-10"},
    {9481, "STP 60-JP-10"}, {9482, "SHP 75-10"},
    {9483, "EVC 7.4-1AC-10"}, {9484, "EVC 22-3AC-10"},
    {9485, "Hybrid Controller"}, {9486, "SI 5.0H-13"},
    {9487, "SBS 4.0-JP"}, {9488, "STP 25-50"},
    {9489, "STP 20-50"}, {9490, "STP 17-50"}, {9491, "STP 15-50"},
    {9492, "STP 12-50"}, {9493, "STP 30-US-50"},
    {9494, "STP 25-US-50"}, {9495, "STP 24-US-50"},
    {9496, "STP 20-US-50"}, {9497, "STP 15-US-50"},
    {9498, "STP 12-US-50"},
};
static const size_t STATUS_CODES_SIZE = sizeof(STATUS_CODES) / sizeof(STATUS_CODES[0]);

// Lookup by code; returns human-readable string or "Code XXXX" if unknown
inline std::string lookup_status_code(uint32_t code) {
  for (size_t i = 0; i < STATUS_CODES_SIZE; i++) {
    if (STATUS_CODES[i].code == code) return std::string(STATUS_CODES[i].message);
  }
  return "Code " + std::to_string(code);
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
