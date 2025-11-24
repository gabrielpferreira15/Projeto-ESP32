/*
 * Disciplina: Algoritmos e Estruturas de Dados
 * Projeto: Sistema de Auditoria de Dispositivos BLE com ESP32
 * Autores: André Luis Freire de Andrade Sieczko (alfas@cesar.school)
 *          Arthur Leandro Costa (alc@cesar.school)
 *          Gabriel Peixoto e Silva Ferreira (gpsf@cesar.school)
 *          Pedro Henrique Guimarães Liberal (phgl@cesar.school)
 *          Rafael Holder de Oliveira (rho@cesar.school)  
 *          
 * Biblioteca necessária: ArduinoJson (Instalar via Library Manager)
 */

 // --- BIBLIOTECAS ---
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <vector>
#include <map>

// --- CONFIGURAÇÕES ---
const int scanTime = 5;
const char* DATA_FILENAME = "/scan_log.jsonl";
const int MAX_SCANS = 10;

// --- ESTRUTURA DE DADOS ---
struct DeviceData {
  String name;
  String address;
  int addrType;
  int rssi;
  int txPower;
  String manufacturerData;
  String payloadHex;
};

// --- Arrays globais (devices e MACs) ---
std::vector<DeviceData> currentDeviceList;
std::vector<String> sessionMacAddresses;

BLEScan* pBLEScan;

// --- PROTÓTIPO DAS FUNÇÕES AUXILIARES ---
void initFS();
bool isAddressAlreadySaved(String address);
String payloadToHex(uint8_t* payload, size_t length);
void saveScanToFile(BLEScanResults foundDevices);
void loadDataToArray();
void processarDadosDoArray();
void realizarAuditoriaEmDispositivo(const DeviceData& device, int index);
String analisarTipoMAC(String mac);
void explorarServicosAtivos(String address, int addrType);
String traduzirUUID(String uuid);
String obterPropriedades(BLERemoteCharacteristic* pChar);

// ---  SETUP E LOOP ---
void setup() {
  Serial.begin(115200);
  delay(4000);

  Serial.println("\n=== INICIANDO SISTEMA DE AUDITORIA BLE (DEEP SCAN) ===");
  initFS();

  sessionMacAddresses.clear();
  sessionMacAddresses.reserve(60);

  BLEDevice::init("ESP32_Auditor_DeepPro");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  Serial.println("Sistema pronto. Iniciando escaneamento.");
}

void loop() {
  for (int i = 0; i < MAX_SCANS; i++) {
    Serial.printf("\n--- Ciclo de Escaneamento %d/%d ---\n", i + 1, MAX_SCANS);
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    
    Serial.printf("Dispositivos detectados: %d\n", foundDevices.getCount());
    saveScanToFile(foundDevices);
    pBLEScan->clearResults(); 

    delay(5000);
  }

  Serial.println("\n>>> COLETA CONCLUÍDA. INICIANDO CARREGAMENTO <<<");
  loadDataToArray();

  Serial.println("\n================ RELATÓRIO DE AUDITORIA ================");
  processarDadosDoArray();
  Serial.println("========================================================");

  BLEDevice::deinit(true);
  
  Serial.println("\n--- Processo finalizado. Sistema em Standby. ---");;
  while (true) {
    delay(1000); 
  }
}

// --- IMPLEMENTAÇÃO DAS FUNÇÕES AUXILIARES ---
// --------------------------------------------
// Inicializa o sistema de arquivos LittleFS
void initFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("Erro Fatal: Falha ao montar LittleFS");
    return;
  }
  Serial.println("LittleFS OK.");
  
  if (LittleFS.exists(DATA_FILENAME)) {
    LittleFS.remove(DATA_FILENAME);
    Serial.println("Log anterior limpo com sucesso.");
  }
}

// Verifica se o endereço MAC já foi salvo na sessão atual
bool isAddressAlreadySaved(String address) {
  for (const String& savedAddr : sessionMacAddresses) {
    if (savedAddr.equals(address)) {
      return true;
    }
  }
  return false;
}

// Converte o payload bruto em uma string hexadecimal
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

// Salva os novos dispositivos detectados em um arquivo JSONL (no ESP32)
void saveScanToFile(BLEScanResults foundDevices) {
  File file = LittleFS.open(DATA_FILENAME, "a");
  if (!file) {
    Serial.println("Erro: Não foi possível escrever no arquivo.");
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
    doc["addrType"] = (int)device.getAddressType();
    doc["rssi"] = device.haveRSSI() ? device.getRSSI() : 0;
    doc["txPower"] = device.haveTXPower() ? device.getTXPower() : -127;

    if (device.haveManufacturerData()) {
        std::string md = device.getManufacturerData();
        String hexMD = "";
        for (int j = 0; j < md.length(); j++) {
            char buf[3];
            sprintf(buf, "%02X", (unsigned char)md[j]);
            hexMD += buf;
        }
        doc["manufacturerData"] = hexMD;
    } else {
        doc["manufacturerData"] = "N/A";
    }

    if (device.getPayloadLength() > 0) {
       doc["payload"] = payloadToHex(device.getPayload(), device.getPayloadLength());
    } else {
       doc["payload"] = "";
    }

    serializeJson(doc, file);
    file.println();
  }
  
  file.close();
  if (newDevicesCount > 0) {
    Serial.printf("Novos dispositivos encontrados: %d\n", newDevicesCount);
  } else {
    Serial.println("Nenhum novo dispositivo encontrado neste ciclo.");
  }
}

// Carrega os dados do arquivo para o array na RAM
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
        data.addrType = doc.containsKey("type") ? doc["addrType"].as<int>() : 0;
        data.rssi = doc["rssi"];
        data.txPower = doc["txPower"];
        data.manufacturerData = doc["manufacturerData"].as<String>();
        data.payloadHex = doc["payload"].as<String>();

        currentDeviceList.push_back(data);
      }
    }
  }
  file.close();
  
  Serial.printf("Registros carregados na RAM para auditoria: %d\n", currentDeviceList.size());
}
// Processa os dados carregados e realiza a auditoria
String analisarTipoMAC(String mac) {
    // Se o bit menos significativo do primeiro byte for 1, é Multicast (não comum em device address).
    // Se o segundo bit menos significativo for 1, é "Locally Administered" (Random).
    // Ex: mac = "5a:..." -> 0x5A -> 0101 1010. O segundo bit (bit 1) é 1.
    long firstByte = strtol(mac.substring(0, 2).c_str(), NULL, 16);
    
    // Verifica bit "Locally Administered" (bitmask 0x02)
    if (firstByte & 0x02) {
        // Geralmente dispositivos móveis (iOS/Android) usam Random Resolvable Private Addresses para privacidade
        return "RANDOM/PRIVATE (Privacidade Ativa)";
    } else {
        return "PUBLIC (Fixo de Fábrica)";
    }
}

// Traduz o UUID padrão para uma descrição conhecida
String traduzirUUID(String uuid) {
    uuid.toLowerCase();
    if (uuid.indexOf("1800") >= 0) return "Generic Access";
    if (uuid.indexOf("1801") >= 0) return "Generic Attribute";
    if (uuid.indexOf("180a") >= 0) return "Device Information";
    if (uuid.indexOf("180f") >= 0) return "Battery Service";
    if (uuid.indexOf("180d") >= 0) return "Heart Rate";
    if (uuid.indexOf("1812") >= 0) return "Human Interface Device (HID)";
    if (uuid.indexOf("ffe0") >= 0) return "HC-05/06 Serial (Proprietário)";
    if (uuid.indexOf("fe9f") >= 0) return "Google/Android Fast Pair";
    if (uuid.indexOf("fd6f") >= 0) return "Contact Tracing (COVID/Exposure)";
    return "Desconhecido / Proprietário";
}

// Obtém as propriedades de uma característica remota
String obterPropriedades(BLERemoteCharacteristic* pChar) {
    String props = "";
    if (pChar->canRead()) props += "READ ";
    if (pChar->canWrite()) props += "WRITE ";
    if (pChar->canWriteNoResponse()) props += "WRITE_NO_RESP ";
    if (pChar->canNotify()) props += "NOTIFY ";
    if (pChar->canIndicate()) props += "INDICATE ";
    if (pChar->canBroadcast()) props += "BROADCAST ";
    
    if (props == "") props = "NONE";
    return "[" + props + "]";
}

// Explora os serviços ativos de um dispositivo BLE
void explorarServicosAtivos(String address, int addrType) {
    Serial.println("    >>> INICIANDO PROBING (DEEP SCAN) <<<");
    BLEClient* pClient = BLEDevice::createClient();

    esp_ble_addr_type_t type = (esp_ble_addr_type_t)addrType;

    if (pClient->connect(BLEAddress(address.c_str()), type)) {
        Serial.println("    [CONECTADO] Conectado ao alvo. A ler tabela GATT...");
        
        std::map<std::string, BLERemoteService*>* pServices = pClient->getServices();        
        
        if (pServices != nullptr) {
            Serial.printf("    [INFO] Serviços encontrados: %d\n", pServices->size());

            for (auto const& [uuid_str, remoteService] : *pServices) {
                String svcUUID = String(remoteService->getUUID().toString().c_str());
                Serial.printf("       + Serviço: %s (%s)\n", svcUUID.c_str(), traduzirUUID(svcUUID).c_str());
                
                std::map<std::string, BLERemoteCharacteristic*>* pChars = remoteService->getCharacteristics();
                
                if (pChars != nullptr) {
                    for (auto const& [char_uuid, remoteChar] : *pChars) {
                        String charUUID = String(remoteChar->getUUID().toString().c_str());
                        String flags = obterPropriedades(remoteChar);
                        
                        Serial.printf("          -> Char: %s  %s\n", charUUID.c_str(), flags.c_str());
                    }
                }
            }
        } else {
            Serial.println("    [ERRO] Falha ao enumerar serviços.");
        }
        
        pClient->disconnect();
        Serial.println("    [FIM] Desconectado.");
    } else {
        Serial.println("    [FALHA] O dispositivo recusou a conexão ou saiu do alcance.");
    }
    
    delete pClient;
    delay(500);
}

// Realiza a auditoria detalhada de um dispositivo específico
void realizarAuditoriaEmDispositivo(const DeviceData& device, int index) {
    Serial.printf("\n[ALVO #%02d] %s\n", index + 1, device.address.c_str());
    Serial.println("----------------------------------------------------------------");
    
    Serial.print("Nome:          ");
    if (device.name == "N/A") {
        Serial.println("NÃO IDENTIFICADO (Suspeito ou Beacon simples)");
    } else {
        Serial.println(device.name);
    }
    
    Serial.printf("Tipo MAC:      %s\n", analisarTipoMAC(device.address).c_str());

    Serial.printf("Sinal (RSSI):  %d dBm", device.rssi);
    if (device.rssi > -60) {
      Serial.println(" [CRÍTICO: Alvo próximo - Executando Deep Scan]");
      explorarServicosAtivos(device.address, device.addrType);
    } else if (device.rssi > -70) {
      Serial.println("ALERTA: Próximo (Sala)");
    } else if (device.rssi > -90) {
      Serial.println("INFO: Distante");
    } else {
      Serial.println("INFO: Sinal fraco/Instável");
    }

    if (device.manufacturerData != "N/A") {
        Serial.printf("Dados Fabr.:   %s (Tam: %d bytes)\n", device.manufacturerData.c_str(), device.manufacturerData.length() / 2);
        
        if (device.manufacturerData.startsWith("4C00")) {
            Serial.println("               -> DETECÇÃO: Provável dispositivo Apple/iBeacon");
        }
        else if (device.manufacturerData.startsWith("0600")) {
             Serial.println("               -> DETECÇÃO: Provável dispositivo Microsoft");
        }
    } else {
        Serial.println("Dados Fabr.:   Não foi possível obter dados de fabricante.");
    }

    if (device.payloadHex.length() > 0) {
        Serial.printf("Payload:       DETECTADO (%d bytes)\n", device.payloadHex.length() / 2);
    }
}

// Processa os dados carregados do array
void processarDadosDoArray() {
  if (currentDeviceList.empty()) {
    Serial.println("Nenhum dispositivo para auditar.");
    return;
  }

  int idx = 0;
  for (const auto& dev : currentDeviceList) {
    realizarAuditoriaEmDispositivo(dev, idx);
    idx++;
  }
  
  Serial.println("\n--- Resumo Estatístico ---");
  Serial.printf("Total Auditado: %d dispositivos\n", currentDeviceList.size());
}
