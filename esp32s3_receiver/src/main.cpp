#include <Arduino.h>
#include <RadioLib.h>

#include "TelemetryPacket.h"

void setup() {
  Serial.begin(115200);
  delay(1000);

  const TelemetryPacket::Values sample = {
    21.5F,
    22.0F,
    1013.25F,
    4.1F,
  };

  uint8_t payload[TelemetryPacket::kPayloadSize] = {};
  TelemetryPacket::encode(sample, payload);

  TelemetryPacket::Values decoded = {};
  TelemetryPacket::Statuses statuses = {};
  TelemetryPacket::decode(payload, decoded, statuses);

  Serial.println("ESP32 receiver boot OK");
  Serial.print("Telemetry packet bytes: ");
  Serial.println(TelemetryPacket::kPayloadSize);
  Serial.print("Decoded pressure: ");
  Serial.println(decoded.pressureHPa);
}

void loop() {
  static uint32_t counter = 0;

  Serial.print("ESP32 receiver heartbeat: ");
  Serial.println(counter++);
  delay(1000);
}
