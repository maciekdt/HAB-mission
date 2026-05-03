#include "TelemetryPacket.h"

#include <math.h>

namespace {
constexpr float kMinTemperatureC = -100.0F;
constexpr float kMaxTemperatureC = 100.0F;
constexpr float kMinPressureHPa = 0.0F;
constexpr float kMaxPressureHPa = 1500.0F;
constexpr float kMinVoltageV = 0.0F;
constexpr float kMaxVoltageV = 5.0F;
}  // namespace

bool TelemetryPacket::encode(const Values& values, uint8_t (&payload)[kPayloadSize]) {
  uint32_t ds18Temperature = 0;
  uint32_t bmpTemperature = 0;
  uint32_t pressure = 0;
  uint32_t voltage = 0;

  const Statuses statuses = {
    quantize(values.ds18TemperatureC, kMinTemperatureC, kMaxTemperatureC, kTemperatureBits, ds18Temperature),
    quantize(values.bmpTemperatureC, kMinTemperatureC, kMaxTemperatureC, kTemperatureBits, bmpTemperature),
    quantize(values.pressureHPa, kMinPressureHPa, kMaxPressureHPa, kPressureBits, pressure),
    quantize(values.voltageV, kMinVoltageV, kMaxVoltageV, kVoltageBits, voltage),
  };

  for (size_t i = 0; i < kPayloadSize; i++) {
    payload[i] = 0;
  }

  uint8_t bitOffset = 0;
  writeBits(payload, bitOffset, 2, static_cast<uint8_t>(statuses.ds18TemperatureC));
  bitOffset += 2;
  writeBits(payload, bitOffset, 2, static_cast<uint8_t>(statuses.bmpTemperatureC));
  bitOffset += 2;
  writeBits(payload, bitOffset, 2, static_cast<uint8_t>(statuses.pressureHPa));
  bitOffset += 2;
  writeBits(payload, bitOffset, 2, static_cast<uint8_t>(statuses.voltageV));
  bitOffset += 2;

  writeBits(payload, bitOffset, kTemperatureBits, ds18Temperature);
  bitOffset += kTemperatureBits;
  writeBits(payload, bitOffset, kTemperatureBits, bmpTemperature);
  bitOffset += kTemperatureBits;
  writeBits(payload, bitOffset, kPressureBits, pressure);
  bitOffset += kPressureBits;
  writeBits(payload, bitOffset, kVoltageBits, voltage);

  return true;
}

bool TelemetryPacket::decode(const uint8_t (&payload)[kPayloadSize], Values& values, Statuses& statuses) {
  uint8_t bitOffset = 0;
  statuses.ds18TemperatureC = static_cast<ValueStatus>(readBits(payload, bitOffset, 2));
  bitOffset += 2;
  statuses.bmpTemperatureC = static_cast<ValueStatus>(readBits(payload, bitOffset, 2));
  bitOffset += 2;
  statuses.pressureHPa = static_cast<ValueStatus>(readBits(payload, bitOffset, 2));
  bitOffset += 2;
  statuses.voltageV = static_cast<ValueStatus>(readBits(payload, bitOffset, 2));
  bitOffset += 2;

  const uint32_t ds18Temperature = readBits(payload, bitOffset, kTemperatureBits);
  bitOffset += kTemperatureBits;
  const uint32_t bmpTemperature = readBits(payload, bitOffset, kTemperatureBits);
  bitOffset += kTemperatureBits;
  const uint32_t pressure = readBits(payload, bitOffset, kPressureBits);
  bitOffset += kPressureBits;
  const uint32_t voltage = readBits(payload, bitOffset, kVoltageBits);

  values.ds18TemperatureC =
      statuses.ds18TemperatureC == ValueStatus::Missing ? NAN : dequantize(ds18Temperature, kMinTemperatureC);
  values.bmpTemperatureC =
      statuses.bmpTemperatureC == ValueStatus::Missing ? NAN : dequantize(bmpTemperature, kMinTemperatureC);
  values.pressureHPa = statuses.pressureHPa == ValueStatus::Missing ? NAN : dequantize(pressure, kMinPressureHPa);
  values.voltageV = statuses.voltageV == ValueStatus::Missing ? NAN : dequantize(voltage, kMinVoltageV);

  return true;
}

TelemetryPacket::ValueStatus TelemetryPacket::quantize(
    float value,
    float minValue,
    float maxValue,
    uint8_t bitCount,
    uint32_t& quantized) {
  if (isnan(value) || isinf(value)) {
    quantized = 0;
    return ValueStatus::Missing;
  }

  if (value < minValue) {
    quantized = 0;
    return ValueStatus::BelowRange;
  }

  if (value > maxValue) {
    quantized = (1UL << bitCount) - 1;
    return ValueStatus::AboveRange;
  }

  const float shifted = (value - minValue) * kScale;
  quantized = static_cast<uint32_t>(lroundf(shifted));

  return ValueStatus::Ok;
}

float TelemetryPacket::dequantize(uint32_t quantized, float minValue) {
  return (static_cast<float>(quantized) / kScale) + minValue;
}

void TelemetryPacket::writeBits(uint8_t* payload, uint8_t bitOffset, uint8_t bitCount, uint32_t value) {
  for (uint8_t i = 0; i < bitCount; i++) {
    const uint8_t sourceBit = bitCount - 1 - i;
    const uint8_t bit = (value >> sourceBit) & 0x01;
    const uint8_t targetBitIndex = bitOffset + i;
    const uint8_t targetByte = targetBitIndex / 8;
    const uint8_t targetBit = 7 - (targetBitIndex % 8);

    if (bit != 0) {
      payload[targetByte] |= (1U << targetBit);
    }
  }
}

uint32_t TelemetryPacket::readBits(const uint8_t* payload, uint8_t bitOffset, uint8_t bitCount) {
  uint32_t value = 0;
  for (uint8_t i = 0; i < bitCount; i++) {
    const uint8_t sourceBitIndex = bitOffset + i;
    const uint8_t sourceByte = sourceBitIndex / 8;
    const uint8_t sourceBit = 7 - (sourceBitIndex % 8);
    const uint8_t bit = (payload[sourceByte] >> sourceBit) & 0x01;

    value = (value << 1) | bit;
  }

  return value;
}
