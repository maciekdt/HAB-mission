#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <RadioLib.h>
#include <SparkFun_BMP581_Arduino_Library.h>
#include <Wire.h>

#include "TelemetryPacket.h"

// Lora
constexpr uint8_t LORA_NSS = 6;
constexpr uint8_t LORA_RST = 7;
constexpr uint8_t LORA_BUSY = 8;
constexpr uint8_t LORA_DIO1 = 9;

// Temp
constexpr uint8_t DS18_ONE_WIRE = 5;

// Voltage
constexpr uint8_t VOLTAGE_PIN = A0;
constexpr float ADC_REF = 3.3;
constexpr float ADC_MAX = 4095.0;
constexpr float DIVIDER_RATIO = 2.0;

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

OneWire oneWire(DS18_ONE_WIRE);
DallasTemperature ds18(&oneWire);

BMP581 bmp;

void setup() {
  SerialUSB.begin(115200);
  delay(1000);

  // SX1262
  SPI.begin();
  int state = radio.begin(868.0);
  int16_t power_state = radio.setOutputPower(22);
  (void)power_state;
  radio.setDio2AsRfSwitch(true);
  SerialUSB.println("SX1262: " + String(state));

  // DS18
  ds18.begin();
  SerialUSB.println("DS18 OK");

  // BMP581
  Wire.begin();
  if (bmp.beginI2C(BMP581_I2C_ADDRESS_DEFAULT) == BMP5_OK) {
    SerialUSB.println("BMP581 OK");
  } else {
    SerialUSB.println("BMP581 ERROR");
  }

  // Voltage
  analogReadResolution(12);
}

float readSupplyVoltage() {
  long sum = 0;
  int probes_n = 32;
  for (int i = 0; i < probes_n; i++) {
    sum += analogRead(VOLTAGE_PIN);
    delay(2);
  }
  float raw = sum / probes_n;
  float vadc = raw * ADC_REF / ADC_MAX;
  return vadc * DIVIDER_RATIO;
}

void loop() {
  SerialUSB.print("\n-------------------------\n\n");

  // DS18
  ds18.requestTemperatures();
  float tempC = ds18.getTempCByIndex(0);
  if (tempC == DEVICE_DISCONNECTED_C) {
    tempC = NAN;
  }
  SerialUSB.println("DS18 temp: " + String(tempC) + "C");

  // BMP581
  float bmpTemperatureC = NAN;
  float pressureHPa = NAN;
  if (bmp.setMode(BMP5_POWERMODE_FORCED) != BMP5_OK) {
    SerialUSB.println("BMP581 ERROR during setting BMP5_POWERMODE_FORCED mode");
  }

  bmp5_sensor_data bmp_data = {0};
  int8_t bmp_read_flag = bmp.getSensorData(&bmp_data);
  if (bmp_read_flag == BMP5_OK) {
    bmpTemperatureC = bmp_data.temperature;
    pressureHPa = bmp_data.pressure / 100.0F;
    SerialUSB.println("BMP581 temp: " + String(bmpTemperatureC) + "C");
    SerialUSB.println("BMP581 pressure: " + String(pressureHPa) + "hPa");
  } else {
    SerialUSB.println("BMP581 read error");
  }

  if (bmp.setMode(BMP5_POWERMODE_DEEP_STANDBY) != BMP5_OK) {
    SerialUSB.println("BMP581 ERROR during setting BMP5_POWERMODE_DEEP_STANDBY mode");
  }

  // Voltage
  float voltage = readSupplyVoltage();
  SerialUSB.println("Voltage: " + String(voltage) + "V");

  // SX1262
  const TelemetryPacket::Values telemetry = {
    tempC,
    bmpTemperatureC,
    pressureHPa,
    voltage,
  };
  uint8_t payload[TelemetryPacket::kPayloadSize];

  TelemetryPacket::encode(telemetry, payload);
  int state = radio.transmit(payload, TelemetryPacket::kPayloadSize);
  if (state == 0) {
    SerialUSB.println("SX1262 TX telemetry state: OK, bytes: " + String(TelemetryPacket::kPayloadSize));
  } else {
    SerialUSB.println("SX1262 TX state: " + String(state));
  }

  delay(5000);
}
