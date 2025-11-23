/*#include <Arduino.h>
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
*/

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <vector>

/* CONFIGURAÇÕES */
const int scanTime = 5;
const char* DATA_FILENAME = "/scan_log.jsonl";
const int MAX_SCANS = 3;

/* ESTRUTURA DE DADOS */
struct DeviceData {
  String name;
  String address;
  int addressType;
  int rssi;
  int txPower;
  int appearance;
  String manufacturerData;
  String serviceData;
  String serviceUUID;
  int payloadLength;
  String payloadHex;
};

// Array dinâmico para uso das outras funções
std::vector<DeviceData> currentDeviceList;

BLEScan* pBLEScan;

/* FUNÇÕES AUXILIARES */
// Função para inicializar o LittleFS
void initFS();

// Função para converter Payload (bytes) em String Hexadecimal
String payloadToHex(uint8_t* payload, size_t length);

// Função para SALVAR os dados no arquivo (Append)
void saveScanToFile(BLEScanResults foundDevices);

// Função para LER do arquivo e carregar no Array (Vector)
void loadDataToArray();

// Função de exemplo para USAR os dados que estão no Array
void processarDadosDoArray();


/* SETUP E LOOP */
void setup() {
  Serial.begin(115200);
  initFS();

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  Serial.println("Sistema pronto. Iniciando loop de pesquisa.");
}

void loop() {
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  Serial.printf("Escaneamento concluído. Encontrados: %d\n", foundDevices.getCount());

  for (int i = 0; i < MAX_SCANS; i++) {
    Serial.printf("Processando ciclo de salvamento %d/%d\n", i + 1, MAX_SCANS);

    saveScanToFile(foundDevices);
    //loadDataToArray();
    //processarDadosDoArray();

    pBLEScan->clearResults();
    
    Serial.println("------------------------------------------------");
    delay(5000);
  }

  Serial.println(">>> LIMITE DE ESCANEAMENTOS ATINGIDO. ENCERRANDO. <<<");

  loadDataToArray();
  processarDadosDoArray();
  
  while (true) {
    delay(1000); 
  }
}


/* IMPLEMENTAÇÃO DAS FUNÇÕES AUXILIARES */
void initFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("Falha ao montar o sistema de arquivos LittleFS");
    return;
  }
  Serial.println("LittleFS montado com sucesso.");
}

String payloadToHex(uint8_t* payload, size_t length) {
  String hexString = "";
  if (length > 0 && payload != nullptr) {
    for (size_t i = 0; i < length; i++) {
      if (payload[i] < 16) hexString += "0";
      hexString += String(payload[i], HEX);
    }
  }
  return hexString;
}

void saveScanToFile(BLEScanResults foundDevices) {
  File file = LittleFS.open(DATA_FILENAME, "a");
  
  if (!file) {
    Serial.println("Erro ao abrir arquivo para gravação.");
    return;
  }

  int count = foundDevices.getCount();
  for (int i = 0; i < count; i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);
    JsonDocument doc;
    
    doc["name"] = device.haveName() ? device.getName() : "N/A";
    doc["addr"] = device.getAddress().toString();
    doc["addrType"] = device.getAddressType();
    doc["rssi"] = device.haveRSSI() ? device.getRSSI() : 0;
    doc["txPower"] = device.haveTXPower() ? device.getTXPower() : 0;
    doc["appearance"] = device.haveAppearance() ? device.getAppearance() : 0;
    doc["manufacturerData"] = device.haveManufacturerData() ? device.getManufacturerData() : "N/A";
    doc["serviceData"] = device.haveServiceData() ? device.getServiceData() : "N/A";
    doc["serviceUUID"] = device.haveServiceUUID() ? device.getServiceUUID().toString() : "N/A";
    doc["payloadLength"] = device.getPayloadLength();

    if (device.getPayloadLength() > 0) {
       doc["payload"] = payloadToHex(device.getPayload(), device.getPayloadLength());
    } else {
       doc["payload"] = "";
    }

    serializeJson(doc, file);
    file.println();
  }
  
  file.close();
  Serial.println("Dados salvos no LittleFS.");
}

void loadDataToArray() {
  currentDeviceList.clear();

  File file = LittleFS.open(DATA_FILENAME, "r");
  if (!file) {
    Serial.println("Arquivo não existe ou não pode ser aberto.");
    return;
  }

  Serial.println("--- Lendo dados do arquivo para a RAM ---");

  while (file.available()) {
    String jsonLine = file.readStringUntil('\n');
    
    if (jsonLine.length() > 0) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, jsonLine);

      if (!error) {
        DeviceData data;
        data.name = doc["name"].as<String>();
        data.address = doc["addr"].as<String>();
        data.addressType = doc["addrType"];
        data.rssi = doc["rssi"];
        data.txPower = doc["txPower"];
        data.appearance = doc["appearance"];
        data.manufacturerData = doc["manufacturerData"].as<String>();
        data.serviceData = doc["serviceData"].as<String>();
        data.serviceUUID = doc["serviceUUID"].as<String>();
        data.payloadLength = doc["payloadLength"];
        data.payloadHex = doc["payload"].as<String>();

        currentDeviceList.push_back(data);
      }
    }
  }
  file.close();
  
  Serial.printf("Total de registros carregados na RAM: %d\n", currentDeviceList.size());
}

void processarDadosDoArray() {
  Serial.println(">>> Processando dados armazenados no Array Global <<<");
  // Exemplo: Calcular média de RSSI ou buscar um MAC específico
  if (currentDeviceList.empty()) return;

  long totalRSSI = 0;
  for (const auto& dev : currentDeviceList) {
    totalRSSI += dev.rssi;
    // Exemplo: Imprimir apenas dispositivos com sinal forte
    if (dev.rssi > -50) {
        Serial.printf("Dispositivo Forte: %s (%s)\n", dev.name.c_str(), dev.address.c_str());
    }
  }
  Serial.printf("Média de RSSI dos dados históricos: %ld\n", totalRSSI / currentDeviceList.size());
}

