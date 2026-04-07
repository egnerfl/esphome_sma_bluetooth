/*
 * Unit tests for sma_protocol.h — pure protocol helpers.
 * Run with: pio test -e native
 */

#include <unity.h>
#include <cstring>

// Provide stubs for types used in the header but not needed for protocol tests
namespace esphome {
namespace sensor { class Sensor {}; }
namespace text_sensor { class TextSensor {}; }
namespace binary_sensor { class BinarySensor {}; }
}  // namespace esphome

#include "sma_protocol.h"

using namespace esphome::sma_bluetooth;

// ============================================================
//  FCS-16 checksum tests
// ============================================================

void test_fcs16_update_zero() {
    uint16_t fcs = 0xFFFF;
    fcs = fcs16_update(fcs, 0x00);
    // After one byte of 0x00: (0xFFFF >> 8) ^ FCS_TABLE[(0xFFFF ^ 0x00) & 0xFF]
    //   = 0x00FF ^ FCS_TABLE[0xFF] = 0x00FF ^ 0x0F78
    TEST_ASSERT_EQUAL_HEX16(0x00FF ^ 0x0F78, fcs);
}

void test_fcs16_known_sequence() {
    // Compute FCS over a known byte sequence
    uint16_t fcs = 0xFFFF;
    const uint8_t data[] = {0x7E, 0x00, 0x01, 0x02};
    for (int i = 0; i < 4; i++) {
        fcs = fcs16_update(fcs, data[i]);
    }
    fcs ^= 0xFFFF;
    // Just verify it produces a consistent result (not zero)
    TEST_ASSERT_NOT_EQUAL(0, fcs);
}

// ============================================================
//  get_u16 / get_u32 / get_u64 little-endian extraction
// ============================================================

void test_get_u16() {
    uint8_t buf[] = {0x34, 0x12};
    TEST_ASSERT_EQUAL_HEX16(0x1234, get_u16(buf));
}

void test_get_u32() {
    uint8_t buf[] = {0x78, 0x56, 0x34, 0x12};
    TEST_ASSERT_EQUAL_HEX32(0x12345678, get_u32(buf));
}

void test_get_u64() {
    uint8_t buf[] = {0xEF, 0xCD, 0xAB, 0x90, 0x78, 0x56, 0x34, 0x12};
    TEST_ASSERT_EQUAL_UINT64(0x1234567890ABCDEFULL, get_u64(buf));
}

void test_get_u16_zeros() {
    uint8_t buf[] = {0x00, 0x00};
    TEST_ASSERT_EQUAL_HEX16(0x0000, get_u16(buf));
}

void test_get_u32_max() {
    uint8_t buf[] = {0xFF, 0xFF, 0xFF, 0xFF};
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, get_u32(buf));
}

// ============================================================
//  NaN detection
// ============================================================

void test_nan_s32() {
    TEST_ASSERT_TRUE(is_NaN((int32_t)0x80000000));
    TEST_ASSERT_FALSE(is_NaN((int32_t)0));
    TEST_ASSERT_FALSE(is_NaN((int32_t)42));
}

void test_nan_u32() {
    TEST_ASSERT_TRUE(is_NaN((uint32_t)0xFFFFFFFF));
    TEST_ASSERT_FALSE(is_NaN((uint32_t)0));
}

void test_nan_u64() {
    TEST_ASSERT_TRUE(is_NaN((uint64_t)0xFFFFFFFFFFFFFFFFULL));
    TEST_ASSERT_FALSE(is_NaN((uint64_t)0));
}

void test_nan_s16() {
    TEST_ASSERT_TRUE(is_NaN((int16_t)0x8000));
    TEST_ASSERT_FALSE(is_NaN((int16_t)0));
}

// ============================================================
//  Unit conversion macros
// ============================================================

void test_to_volt() {
    // 23050 raw = 230.50 V
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 230.50f, toVolt(23050));
}

void test_to_amp() {
    // 1500 raw = 1.500 A
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, toAmp(1500));
}

void test_to_hz() {
    // 5000 raw = 50.00 Hz
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, toHz(5000));
}

void test_to_kwh() {
    // 1234567 Wh = 1234.567 kWh
    float result = (float)tokWh(1234567ULL);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1234.567f, result);
}

void test_to_temp() {
    // 3520 raw = 35.20 C
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 35.20f, toTemp(3520));
}

void test_to_kw() {
    // 1500 W = 1.500 kW
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, tokW(1500));
}

// ============================================================
//  Status code lookup
// ============================================================

void test_lookup_known_code() {
    TEST_ASSERT_EQUAL_STRING("OK", lookup_status_code(307).c_str());
}

void test_lookup_closed() {
    TEST_ASSERT_EQUAL_STRING("Closed", lookup_status_code(51).c_str());
}

void test_lookup_unknown_code() {
    TEST_ASSERT_EQUAL_STRING("Code 9999", lookup_status_code(9999).c_str());
}

void test_lookup_device_type() {
    TEST_ASSERT_EQUAL_STRING("SB 1600TL-10", lookup_status_code(9109).c_str());
}

void test_lookup_device_class() {
    TEST_ASSERT_EQUAL_STRING("Solar Inverters", lookup_status_code(8001).c_str());
}

// ============================================================
//  get_version
// ============================================================

void test_get_version() {
    char buf[24];
    // Version 2.5.10.R  -> 0x02050A11 (R = 'A' + 17 = 0x11)
    // Actually: version = (major<<24)|(minor<<16)|(patch<<8)|release
    // release char = 'A' + release_byte
    uint32_t ver = (2 << 24) | (5 << 16) | (10 << 8) | 3;  // 'D' = 'A'+3
    get_version(ver, buf);
    TEST_ASSERT_EQUAL_STRING("2.5.10.D", buf);
}

// ============================================================
//  print_unix_time
// ============================================================

void test_print_unix_time() {
    char buf[24];
    // 2025-01-01 00:00:00 UTC = 1735689600
    print_unix_time(buf, 1735689600);
    TEST_ASSERT_EQUAL_STRING("2025-01-01 00:00:00", buf);
}

// ============================================================
//  Ignorable error types
// ============================================================

void test_ignorable_inverter_temp() {
    TEST_ASSERT_TRUE(is_ignorable_error(InverterTemp));
}

void test_ignorable_metering() {
    TEST_ASSERT_TRUE(is_ignorable_error(MeteringGridMsTotW));
}

void test_not_ignorable_dc_power() {
    TEST_ASSERT_FALSE(is_ignorable_error(SpotDCPower));
}

// ============================================================
//  FCS table sanity
// ============================================================

void test_fcs_table_size() {
    TEST_ASSERT_EQUAL(256, sizeof(FCS_TABLE) / sizeof(FCS_TABLE[0]));
}

void test_fcs_table_first() {
    TEST_ASSERT_EQUAL_HEX16(0x0000, FCS_TABLE[0]);
}

void test_fcs_table_last() {
    TEST_ASSERT_EQUAL_HEX16(0x0F78, FCS_TABLE[255]);
}

// ============================================================
//  L1Hdr struct layout
// ============================================================

void test_l1hdr_size() {
    TEST_ASSERT_EQUAL(18, sizeof(L1Hdr));
}

// ============================================================
//  Main
// ============================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // FCS
    RUN_TEST(test_fcs16_update_zero);
    RUN_TEST(test_fcs16_known_sequence);
    RUN_TEST(test_fcs_table_size);
    RUN_TEST(test_fcs_table_first);
    RUN_TEST(test_fcs_table_last);

    // Byte extraction
    RUN_TEST(test_get_u16);
    RUN_TEST(test_get_u32);
    RUN_TEST(test_get_u64);
    RUN_TEST(test_get_u16_zeros);
    RUN_TEST(test_get_u32_max);

    // NaN
    RUN_TEST(test_nan_s32);
    RUN_TEST(test_nan_u32);
    RUN_TEST(test_nan_u64);
    RUN_TEST(test_nan_s16);

    // Conversions
    RUN_TEST(test_to_volt);
    RUN_TEST(test_to_amp);
    RUN_TEST(test_to_hz);
    RUN_TEST(test_to_kwh);
    RUN_TEST(test_to_temp);
    RUN_TEST(test_to_kw);

    // Status codes
    RUN_TEST(test_lookup_known_code);
    RUN_TEST(test_lookup_closed);
    RUN_TEST(test_lookup_unknown_code);
    RUN_TEST(test_lookup_device_type);
    RUN_TEST(test_lookup_device_class);

    // Version
    RUN_TEST(test_get_version);
    RUN_TEST(test_print_unix_time);

    // Ignorable errors
    RUN_TEST(test_ignorable_inverter_temp);
    RUN_TEST(test_ignorable_metering);
    RUN_TEST(test_not_ignorable_dc_power);

    // Struct layout
    RUN_TEST(test_l1hdr_size);

    return UNITY_END();
}
