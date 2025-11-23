#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <set>
#include <vector>

int scanTime = 5;
NimBLEScan* pBLEScan;

// Estrutura para armazenar informações dos dispositivos
struct DeviceInfo {
  String mac;
  String name;
  int rssi;
};

// Set para rastrear MACs únicos
std::set<String> uniqueMACs;
// Vector para armazenar dispositivos únicos
std::vector<DeviceInfo> uniqueDevices;

/**
 * Função para scanear vulnerabilidades em um dispositivo específico
 * Adaptada do gepeto.txt
 */
void scanDeviceVulnerabilities(String macAddress, String deviceName) {
  Serial.println("\n===================================================");
  Serial.printf("Iniciando auditoria do dispositivo: %s (%s)\n", 
                deviceName.c_str(), macAddress.c_str());
  Serial.println("===================================================");
  
  // Cria o cliente NimBLE
  NimBLEClient* pClient = NimBLEDevice::createClient();
  
  // Cria objeto BLEAddress a partir do MAC
  NimBLEAddress targetAddress(macAddress.c_str());
  
  Serial.printf("Tentando conectar em: %s ...\n", macAddress.c_str());
  
  // Tenta conectar (com timeout padrão)
  if (pClient->connect(targetAddress)) {
    Serial.println("✓ Conectado! Iniciando mapeamento de serviços...");
    
    // Obtém todos os serviços
    std::vector<NimBLERemoteService*>* pServices = pClient->getServices(true);
    
    if (pServices->size() == 0) {
      Serial.println("Nenhum serviço encontrado.");
    }
    
    for (auto* pService : *pServices) {
      Serial.println("---------------------------------------------------");
      Serial.printf("Serviço Encontrado: %s\n", pService->getUUID().toString().c_str());
      
      // Tenta identificar serviços comuns
      if(pService->getUUID().equals(NimBLEUUID((uint16_t)0x180A))) {
        Serial.println("  [!] ALERTA: Device Information Service exposto (Info Leak)");
      }

      // Obtém todas as características deste serviço
      std::vector<NimBLERemoteCharacteristic*>* pChars = pService->getCharacteristics(true);
      
      for (auto* pChar : *pChars) {
        Serial.printf("  -> Característica: %s\n", pChar->getUUID().toString().c_str());
        
        // Analisa as Permissões (Properties)
        Serial.print("     Permissões: ");
        
        if (pChar->canRead()) {
          Serial.print("[READ] ");
          // Tenta ler o valor para ver se está aberto
          if(pChar->canRead()) {
            try {
              std::string value = pChar->readValue();
              Serial.printf("(Valor lido: %s) ", value.c_str());
            } catch (...) {
              Serial.print("(Erro ao ler) ");
            }
          }
        }
        
        if (pChar->canWrite()) Serial.print("[WRITE] ");
        if (pChar->canWriteNoResponse()) Serial.print("[WRITE NO RESP] ");
        if (pChar->canNotify()) Serial.print("[NOTIFY] ");
        if (pChar->canIndicate()) Serial.print("[INDICATE] ");
        
        Serial.println();

        // Análise de Vulnerabilidade Simples
        if (pChar->canWrite() || pChar->canWriteNoResponse()) {
          Serial.println("     [!] PERIGO: Escrita permitida. Verificar necessidade de autenticação.");
        }
      }
    }
    
    Serial.println("---------------------------------------------------");
    Serial.println("Auditoria finalizada. Desconectando...");
    pClient->disconnect();
    
  } else {
    Serial.println("✗ FALHA AO CONECTAR");
    Serial.printf("⚠ Não foi possível conectar ao dispositivo: %s (%s)\n", 
                  deviceName.c_str(), macAddress.c_str());
    Serial.println("  Possíveis causas: dispositivo fora de alcance, recusando conexões ou já conectado");
  }
  
  // Libera recursos do cliente
  NimBLEDevice::deleteClient(pClient);
  
  // Delay entre conexões para estabilidade do stack BLE
  delay(1000);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n========================================");
  Serial.println("BLE Vulnerability Scanner - ESP32");
  Serial.println("Projeto Acadêmico de Cibersegurança");
  Serial.println("========================================\n");

  Serial.println("Iniciando NimBLE...");
  NimBLEDevice::init("");
  
  // Configura o scanner
  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  Serial.printf("Iniciando escaneamento BLE (%d segundos)...\n", scanTime);
  Serial.println("Aguarde...\n");
  
  // Executa o scan
  NimBLEScanResults results = pBLEScan->start(scanTime, false);
  int deviceCount = results.getCount();
  
  Serial.println("========================================");
  Serial.printf("Escaneamento concluído! %d dispositivos encontrados.\n", deviceCount);
  Serial.println("========================================\n");
  
  // Processa os resultados e remove duplicatas
  Serial.println("Processando dispositivos e removendo duplicatas...\n");
  
  for (int i = 0; i < deviceCount; i++) {
    NimBLEAdvertisedDevice device = results.getDevice(i);
    String mac = device.getAddress().toString().c_str();
    
    // Verifica se o MAC já foi processado
    if (uniqueMACs.find(mac) == uniqueMACs.end()) {
      uniqueMACs.insert(mac);
      
      DeviceInfo devInfo;
      devInfo.mac = mac;
      devInfo.name = device.getName().c_str();
      devInfo.rssi = device.getRSSI();
      
      // Se nome vazio, coloca "Desconhecido"
      if (devInfo.name.length() == 0) {
        devInfo.name = "Desconhecido";
      }
      
      uniqueDevices.push_back(devInfo);
      
      Serial.printf("  [%d] %s - %s (RSSI: %d)\n", 
                    uniqueDevices.size(), 
                    devInfo.mac.c_str(), 
                    devInfo.name.c_str(), 
                    devInfo.rssi);
    }
  }
  
  Serial.printf("\n%d dispositivos únicos identificados.\n\n", uniqueDevices.size());
  
  // Cria JSON com os dispositivos únicos
  Serial.println("Gerando JSON de dispositivos...\n");
  
  DynamicJsonDocument doc(8192);
  JsonArray devicesArray = doc.createNestedArray("devices");
  
  for (const auto& dev : uniqueDevices) {
    JsonObject deviceObj = devicesArray.createNestedObject();
    deviceObj["mac"] = dev.mac;
    deviceObj["name"] = dev.name;
    deviceObj["rssi"] = dev.rssi;
  }
  
  // Serializa JSON para Serial
  Serial.println("JSON de dispositivos únicos:");
  Serial.println("----------------------------");
  serializeJsonPretty(doc, Serial);
  Serial.println("\n----------------------------\n");
  
  // Inicia auditoria de vulnerabilidades para cada dispositivo
  Serial.println("INICIANDO AUDITORIA DE VULNERABILIDADES");
  
  for (size_t i = 0; i < uniqueDevices.size(); i++) {
    Serial.printf("\n[%d/%d] Auditando dispositivo...\n", i + 1, uniqueDevices.size());
    scanDeviceVulnerabilities(uniqueDevices[i].mac, uniqueDevices[i].name);
  }
  
  Serial.println("AUDITORIA COMPLETA - FINALIZADO");
  
  pBLEScan->clearResults();
}

void loop() {
  // Como o código n roda em loop, toda a logica tá no setup
  delay(1000);
}
