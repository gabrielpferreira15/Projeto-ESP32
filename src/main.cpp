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
const int MAX_SCANS = 5;

/* ESTRUTURA DE DADOS */
struct DeviceData {
  String name;
  String address;
  int rssi;
  int txPower;
  String manufacturerData;
  String payloadHex;
};

// Arrays globais (devices e MACs)
std::vector<DeviceData> currentDeviceList;
std::vector<String> sessionMacAddresses;

BLEScan* pBLEScan;

/* FUNÇÕES AUXILIARES */
void initFS();
bool isAddressAlreadySaved(String address);
String payloadToHex(uint8_t* payload, size_t length);
void saveScanToFile(BLEScanResults foundDevices);
void loadDataToArray();
void processarDadosDoArray();

/* SETUP E LOOP */
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("Iniciando sistema...");
  initFS();

  sessionMacAddresses.clear();
  sessionMacAddresses.reserve(60);

  BLEDevice::init("ESP32_Scanner");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  Serial.println("Sistema pronto. Iniciando escaneamento.");
}

void loop() {
  for (int i = 0; i < MAX_SCANS; i++) {
    Serial.printf("--- Iniciando ciclo %d/%d ---\n", i + 1, MAX_SCANS);
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    
    Serial.printf("Dispositivos encontrados neste ciclo: %d\n", foundDevices.getCount());
    saveScanToFile(foundDevices);
    pBLEScan->clearResults(); 

    delay(5000); 
  }

  Serial.println(">>> LIMITE DE ESCANEAMENTOS ATINGIDO. <<<");

  loadDataToArray();
  Serial.println("--- Iniciando a anãlise dos escaneamentos. ---");
  processarDadosDoArray();

  BLEDevice::deinit(true);
  
  Serial.println("--- Fim do processo. Entrando em loop infinito (Idle) ---");
  while (true) {
    delay(1000); 
  }
}

/* IMPLEMENTAÇÃO DAS FUNÇÕES AUXILIARES */
void initFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("Falha fatal no LittleFS");
    return;
  }
  Serial.println("LittleFS OK.");
  
  if (LittleFS.exists(DATA_FILENAME)) {
    LittleFS.remove(DATA_FILENAME);
    Serial.println("Log antigo removido.");
  }
}

bool isAddressAlreadySaved(String address) {
  for (const String& savedAddr : sessionMacAddresses) {
    if (savedAddr.equals(address)) {
      return true;
    }
  }
  return false;
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
  int newDevicesCount = 0;
  
  for (int i = 0; i < count; i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);
    
    String currentAddress = device.getAddress().toString().c_str();
    if (isAddressAlreadySaved(currentAddress)) {
      continue;
    }
    sessionMacAddresses.push_back(currentAddress);
    newDevicesCount++;

    JsonDocument doc;    
    doc["name"] = device.haveName() ? device.getName() : "N/A";
    doc["addr"] = device.getAddress().toString();
    doc["rssi"] = device.haveRSSI() ? device.getRSSI() : 0;
    doc["txPower"] = device.haveTXPower() ? device.getTXPower() : 0;
    doc["manufacturerData"] = device.haveManufacturerData() ? device.getManufacturerData() : "N/A";

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

  std::vector<String>().swap(sessionMacAddresses);

  File file = LittleFS.open(DATA_FILENAME, "r");
  if (!file) {
    Serial.println("Arquivo não existe ou não pode ser aberto.");
    return;
  }

  Serial.println("--- Lendo dados ÚNICOS do arquivo para a RAM ---");

  while (file.available()) {
    String jsonLine = file.readStringUntil('\n');
    
    if (jsonLine.length() > 0) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, jsonLine);

      if (!error) {
        DeviceData data;
        data.name = doc["name"].as<String>();
        data.address = doc["addr"].as<String>();
        data.rssi = doc["rssi"];
        data.txPower = doc["txPower"];
        data.manufacturerData = doc["manufacturerData"].as<String>();
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