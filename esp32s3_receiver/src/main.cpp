#include <Arduino.h>
#include <RadioLib.h>
#include <SD.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "TelemetryPacket.h"

constexpr uint8_t OLED_SDA = 1;
constexpr uint8_t OLED_SCL = 2;
constexpr uint8_t OLED_TEXT_X = 8;
constexpr uint8_t LORA_MISO = 16;
constexpr uint8_t LORA_MOSI = 15;
constexpr uint8_t LORA_SCK = 17;
constexpr uint8_t LORA_NSS = 21;
constexpr uint8_t LORA_RST = 12;
constexpr uint8_t LORA_BUSY = 13;
constexpr uint8_t LORA_DIO1 = 14;
constexpr uint8_t SD_MISO = 3;
constexpr uint8_t SD_MOSI = 5;
constexpr uint8_t SD_SCK = 7;
constexpr uint8_t SD_CS = 18;
constexpr uint8_t BUZZER_PIN = 9;

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
SPIClass sdSpi(HSPI);

volatile bool receivedFlag = false;
bool radioReady = false;
float lastRssi = NAN;
float lastSnr = NAN;
float lastFrequencyError = NAN;
uint32_t rxCount = 0;
uint32_t errorCount = 0;

#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void onPacketReceived() {
  receivedFlag = true;
}

void beepReceivedPacket() {
  for (uint8_t i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(50);
    digitalWrite(BUZZER_PIN, LOW);
    delay(50);
  }
}

void drawSignalStats(const char* status) {
  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tf);

  display.setCursor(OLED_TEXT_X, 12);
  display.print("RSSI ");
  if (isnan(lastRssi)) {
    display.print("--");
  } else {
    display.print(lastRssi, 1);
    display.print("dBm");
  }

  display.setCursor(OLED_TEXT_X, 27);
  display.print("SNR ");
  if (isnan(lastSnr)) {
    display.print("--");
  } else {
    display.print(lastSnr, 1);
    display.print("dB");
  }

  display.setCursor(OLED_TEXT_X, 42);
  display.print("FREQ ");
  if (isnan(lastFrequencyError)) {
    display.print("--");
  } else {
    display.print(lastFrequencyError, 0);
    display.print("Hz");
  }
  display.sendBuffer();
}

void printRadioLibState(const char* label, int state) {
  Serial.print(label);
  Serial.print(": ");
  Serial.println(state);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("SX1262 OK");
    return;
  }

  Serial.println("SX1262 ERROR - check wiring, power, antenna, and SPI pins");
}

void printSdCardType(uint8_t cardType) {
  Serial.print("microSD card type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC/SDXC");
  } else {
    Serial.println("UNKNOWN");
  }
}

bool initMicroSd() {
  Serial.print("microSD SPI pins: SCK=");
  Serial.print(SD_SCK);
  Serial.print(" MISO=");
  Serial.print(SD_MISO);
  Serial.print(" MOSI=");
  Serial.print(SD_MOSI);
  Serial.print(" CS=");
  Serial.println(SD_CS);

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  sdSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, sdSpi, 400000)) {
    Serial.println("microSD ERROR - card mount failed");
    return false;
  }

  const uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("microSD ERROR - no card detected");
    return false;
  }

  printSdCardType(cardType);
  Serial.print("microSD size: ");
  Serial.print(SD.cardSize() / (1024ULL * 1024ULL));
  Serial.println(" MB");
  Serial.println("microSD OK");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("ESP32-S3 SX1262 receiver test");
  Serial.println("SPI pins: SCK=17 MISO=16 MOSI=15 NSS=21 RST=12 BUSY=13 DIO1=14");
  Serial.println("OLED I2C pins: SDA=1 SCL=2");
  Serial.println("For Seeed Wio-SX1262 RX-only, RF_SW should be tied to 3V3.");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(100000);
  delay(100);
  display.begin();
  display.setPowerSave(0);
  display.clearDisplay();
  drawSignalStats("BOOT");

  initMicroSd();

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

  int state = radio.begin(868.0);
  printRadioLibState("radio.begin", state);
  if (state != RADIOLIB_ERR_NONE) {
    drawSignalStats("RADIO ERR");
    return;
  }

  state = radio.setDio2AsRfSwitch(true);
  printRadioLibState("radio.setDio2AsRfSwitch", state);

  radio.setPacketReceivedAction(onPacketReceived);

  state = radio.startReceive();
  printRadioLibState("radio.startReceive", state);
  if (state != RADIOLIB_ERR_NONE) {
    drawSignalStats("RX ERR");
    return;
  }

  radioReady = true;
  drawSignalStats("LISTEN");
  Serial.println("Listening for telemetry packets...");
}

void loop() {
  if (!radioReady || !receivedFlag) {
    delay(10);
    return;
  }

  receivedFlag = false;

  uint8_t payload[TelemetryPacket::kPayloadSize] = {};
  const size_t packetLength = radio.getPacketLength();
  const int state = radio.readData(payload, TelemetryPacket::kPayloadSize);

  if (state == RADIOLIB_ERR_NONE) {
    beepReceivedPacket();
    rxCount++;
    lastRssi = radio.getRSSI();
    lastSnr = radio.getSNR();
    lastFrequencyError = radio.getFrequencyError();

    TelemetryPacket::Values values = {};
    TelemetryPacket::Statuses statuses = {};
    TelemetryPacket::decode(payload, values, statuses);

    Serial.println();
    Serial.println("RX telemetry packet");
    Serial.print("Bytes: ");
    Serial.println(packetLength);
    Serial.print("RSSI: ");
    Serial.print(lastRssi);
    Serial.println(" dBm");
    Serial.print("SNR: ");
    Serial.print(lastSnr);
    Serial.println(" dB");
    Serial.print("Frequency error: ");
    Serial.print(lastFrequencyError);
    Serial.println(" Hz");
    Serial.print("DS18 temperature: ");
    Serial.println(values.ds18TemperatureC);
    Serial.print("BMP temperature: ");
    Serial.println(values.bmpTemperatureC);
    Serial.print("Pressure: ");
    Serial.println(values.pressureHPa);
    Serial.print("Voltage: ");
    Serial.println(values.voltageV);
    drawSignalStats("OK");
    radio.startReceive();
    return;
  }

  errorCount++;
  if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    Serial.println("CRC error");
    drawSignalStats("CRC ERR");
  } else {
    Serial.print("radio.readData error: ");
    Serial.println(state);
    drawSignalStats("RX ERR");
  }

  radio.startReceive();
}
