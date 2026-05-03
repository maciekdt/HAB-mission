#include <math.h>
#include <string.h>

#include <unity.h>

#include "TelemetryPacket.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

extern "C" {
  void setUp() {
  }

  void tearDown() {
  }
}

namespace {
  using Status = TelemetryPacket::ValueStatus;

  void assertStatusOk(const TelemetryPacket::Statuses& statuses) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Status::Ok), static_cast<uint8_t>(statuses.ds18TemperatureC));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Status::Ok), static_cast<uint8_t>(statuses.bmpTemperatureC));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Status::Ok), static_cast<uint8_t>(statuses.pressureHPa));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Status::Ok), static_cast<uint8_t>(statuses.voltageV));
  }

  TelemetryPacket::Values decodePayload(const uint8_t (&payload)[TelemetryPacket::kPayloadSize], TelemetryPacket::Statuses& statuses) {
    TelemetryPacket::Values decoded = {};
    TEST_ASSERT_TRUE(TelemetryPacket::decode(payload, decoded, statuses));
    return decoded;
  }
}  // namespace

void test_encode_decode_roundtrip_ok_values() {
  const TelemetryPacket::Values input = {
    23.45F,
    -10.25F,
    1013.25F,
    3.87F,
  };
  uint8_t payload[TelemetryPacket::kPayloadSize] = {};

  TEST_ASSERT_TRUE(TelemetryPacket::encode(input, payload));

  TelemetryPacket::Statuses statuses = {};
  const TelemetryPacket::Values decoded = decodePayload(payload, statuses);

  assertStatusOk(statuses);
  TEST_ASSERT_FLOAT_WITHIN(0.005F, input.ds18TemperatureC, decoded.ds18TemperatureC);
  TEST_ASSERT_FLOAT_WITHIN(0.005F, input.bmpTemperatureC, decoded.bmpTemperatureC);
  TEST_ASSERT_FLOAT_WITHIN(0.005F, input.pressureHPa, decoded.pressureHPa);
  TEST_ASSERT_FLOAT_WITHIN(0.005F, input.voltageV, decoded.voltageV);
}

void test_encode_decode_exact_range_edges() {
  const TelemetryPacket::Values input = {
    -100.0F,
    100.0F,
    0.0F,
    5.0F,
  };
  uint8_t payload[TelemetryPacket::kPayloadSize] = {};

  TEST_ASSERT_TRUE(TelemetryPacket::encode(input, payload));

  TelemetryPacket::Statuses statuses = {};
  const TelemetryPacket::Values decoded = decodePayload(payload, statuses);

  assertStatusOk(statuses);
  TEST_ASSERT_FLOAT_WITHIN(0.005F, -100.0F, decoded.ds18TemperatureC);
  TEST_ASSERT_FLOAT_WITHIN(0.005F, 100.0F, decoded.bmpTemperatureC);
  TEST_ASSERT_FLOAT_WITHIN(0.005F, 0.0F, decoded.pressureHPa);
  TEST_ASSERT_FLOAT_WITHIN(0.005F, 5.0F, decoded.voltageV);
}

void test_missing_values_are_marked_and_decode_to_nan() {
  const TelemetryPacket::Values input = {
    NAN,
    INFINITY,
    1013.25F,
    3.7F,
  };
  uint8_t payload[TelemetryPacket::kPayloadSize] = {};

  TEST_ASSERT_TRUE(TelemetryPacket::encode(input, payload));

  TelemetryPacket::Statuses statuses = {};
  const TelemetryPacket::Values decoded = decodePayload(payload, statuses);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Status::Missing), static_cast<uint8_t>(statuses.ds18TemperatureC));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Status::Missing), static_cast<uint8_t>(statuses.bmpTemperatureC));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Status::Ok), static_cast<uint8_t>(statuses.pressureHPa));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Status::Ok), static_cast<uint8_t>(statuses.voltageV));
  TEST_ASSERT_TRUE(isnan(decoded.ds18TemperatureC));
  TEST_ASSERT_TRUE(isnan(decoded.bmpTemperatureC));
  TEST_ASSERT_FLOAT_WITHIN(0.005F, 1013.25F, decoded.pressureHPa);
  TEST_ASSERT_FLOAT_WITHIN(0.005F, 3.7F, decoded.voltageV);
}

void test_out_of_range_values_keep_direction_status() {
  const TelemetryPacket::Values input = {
    -100.01F,
    100.01F,
    -0.01F,
    5.01F,
  };
  uint8_t payload[TelemetryPacket::kPayloadSize] = {};

  TEST_ASSERT_TRUE(TelemetryPacket::encode(input, payload));

  TelemetryPacket::Statuses statuses = {};
  const TelemetryPacket::Values decoded = decodePayload(payload, statuses);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Status::BelowRange), static_cast<uint8_t>(statuses.ds18TemperatureC));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Status::AboveRange), static_cast<uint8_t>(statuses.bmpTemperatureC));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Status::BelowRange), static_cast<uint8_t>(statuses.pressureHPa));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Status::AboveRange), static_cast<uint8_t>(statuses.voltageV));

  TEST_ASSERT_FLOAT_WITHIN(0.005F, -100.0F, decoded.ds18TemperatureC);
  TEST_ASSERT_TRUE(decoded.bmpTemperatureC > 100.0F);
  TEST_ASSERT_FLOAT_WITHIN(0.005F, 0.0F, decoded.pressureHPa);
  TEST_ASSERT_TRUE(decoded.voltageV > 5.0F);
}

void test_first_byte_contains_four_two_bit_statuses() {
  const TelemetryPacket::Values input = {
    0.0F,
    NAN,
    -0.01F,
    5.01F,
  };
  uint8_t payload[TelemetryPacket::kPayloadSize] = {};

  TEST_ASSERT_TRUE(TelemetryPacket::encode(input, payload));

  // Status bits are written first: OK(11), Missing(00), BelowRange(01), AboveRange(10).
  TEST_ASSERT_EQUAL_HEX8(0xC6, payload[0]);
}

void test_encoding_is_deterministic_for_same_input() {
  const TelemetryPacket::Values input = {
    12.34F,
    56.78F,
    987.65F,
    4.32F,
  };
  uint8_t firstPayload[TelemetryPacket::kPayloadSize] = {};
  uint8_t secondPayload[TelemetryPacket::kPayloadSize] = {};

  TEST_ASSERT_TRUE(TelemetryPacket::encode(input, firstPayload));
  TEST_ASSERT_TRUE(TelemetryPacket::encode(input, secondPayload));

  TEST_ASSERT_EQUAL_UINT8_ARRAY(firstPayload, secondPayload, TelemetryPacket::kPayloadSize);
}

void runTelemetryPacketTests() {
  UNITY_BEGIN();
  RUN_TEST(test_encode_decode_roundtrip_ok_values);
  RUN_TEST(test_encode_decode_exact_range_edges);
  RUN_TEST(test_missing_values_are_marked_and_decode_to_nan);
  RUN_TEST(test_out_of_range_values_keep_direction_status);
  RUN_TEST(test_first_byte_contains_four_two_bit_statuses);
  RUN_TEST(test_encoding_is_deterministic_for_same_input);
  UNITY_END();
}

#ifdef ARDUINO
void setup() {
  delay(2000);
  runTelemetryPacketTests();
}

void loop() {
}
#else
int main() {
  runTelemetryPacketTests();
  return 0;
}
#endif
