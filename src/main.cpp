#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>

int scanTime = 5;
BLEScan* pBLEScan;

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando escaneamento...");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

void loop() {
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);

  Serial.println("====================================");
  Serial.println("Dispositivos BLE encontrados:");
  Serial.println("====================================");
  
  int devCount = foundDevices.getCount();
  for (int i = 0; i < devCount; ++i) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);

    Serial.printf("  >> Nome: %s\n", device.haveName() ? device.getName().c_str() : "(sem nome)");
    Serial.printf("  >> Endereço: %s\n", device.getAddress().toString().c_str());
    Serial.printf("  >> Tipo de Endereço: %d\n", device.getAddressType());
    Serial.printf("  >> RSSI: %d dBm\n", device.haveRSSI() ? device.getRSSI() : 0);
    Serial.printf("  >> TX Power: %d dBm\n", device.haveTXPower() ? device.getTXPower() : 0);
    Serial.printf("  >> Appearance: %d\n", device.haveAppearance() ? device.getAppearance() : 0);
    Serial.printf("  >> Manufacturer Data: %d\n", device.haveManufacturerData() ? device.getManufacturerData().c_str() : "(sem dados)");
    Serial.printf("  >> Service Data: %d\n", device.haveServiceData() ? device.getServiceData().c_str() : "(sem dados)");
    Serial.printf("  >> Service UUID: %d\n", device.haveServiceUUID() ? device.getServiceUUID().toString().c_str() : "(sem UUID)");
    Serial.printf("  >> Payload Length: %d\n", device.getPayloadLength());

    Serial.printf("  >> Payload: ");
    for (size_t k = 0; k < device.getPayloadLength(); ++k) {
      Serial.printf("%02X ", device.getPayload()[k]);
    }

    Serial.println();
    Serial.println("......");
  }

  Serial.printf("Dispositivos encontrados: %d\n", foundDevices.getCount());
  Serial.println("Escaneamento completo!");
  pBLEScan->clearResults();
  delay(2000);
}
