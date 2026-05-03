#pragma once

#include <stddef.h>
#include <stdint.h>

class TelemetryPacket {
public:
  enum class ValueStatus : uint8_t {
    Missing = 0,
    BelowRange = 1,
    AboveRange = 2,
    Ok = 3,
  };

  struct Values {
    float ds18TemperatureC;
    float bmpTemperatureC;
    float pressureHPa;
    float voltageV;
  };

  struct Statuses {
    ValueStatus ds18TemperatureC;
    ValueStatus bmpTemperatureC;
    ValueStatus pressureHPa;
    ValueStatus voltageV;
  };

  static constexpr size_t kPayloadSize = 9;

  static bool encode(const Values& values, uint8_t (&payload)[kPayloadSize]);
  static bool decode(const uint8_t (&payload)[kPayloadSize], Values& values, Statuses& statuses);

private:
  static constexpr uint8_t kTemperatureBits = 15;
  static constexpr uint8_t kPressureBits = 18;
  static constexpr uint8_t kVoltageBits = 9;
  static constexpr float kScale = 100.0F;

  static ValueStatus quantize(float value, float minValue, float maxValue, uint8_t bitCount, uint32_t& quantized);
  static float dequantize(uint32_t quantized, float minValue);
  static void writeBits(uint8_t* payload, uint8_t bitOffset, uint8_t bitCount, uint32_t value);
  static uint32_t readBits(const uint8_t* payload, uint8_t bitOffset, uint8_t bitCount);
};
