import serial
import time
import json
import datetime
import os

# --- CONFIGURAÇÕES ---
PORTA = '/dev/ttyUSB0'
BAUD = 115200
ARQUIVO_JSON = "auditoria_ble.json"
ARQUIVO_LOG = "auditoria_raw.log"
# ---------------------

def salvar_log_bruto(texto):
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    with open(ARQUIVO_LOG, "a") as f:
        f.write(f"[{timestamp}] {texto}\n")

def salvar_json_estruturado(dados_dict):
    dados_dict["timestamp_host"] = datetime.datetime.now().isoformat()
    
    lista_registros = []
    if os.path.exists(ARQUIVO_JSON):
        try:
            with open(ARQUIVO_JSON, "r") as f:
                conteudo = f.read()
                if conteudo:
                    lista_registros = json.loads(conteudo)
                    if not isinstance(lista_registros, list):
                        lista_registros = [lista_registros]
        except json.JSONDecodeError:
            pass

    lista_registros.append(dados_dict)

    with open(ARQUIVO_JSON, "w") as f:
        json.dump(lista_registros, f, indent=4)
    print(f" >> [SUCESSO] JSON salvo com {dados_dict.get('device_count', 0)} devices.")

print(f"--- INICIANDO CAPTURA EM {PORTA} ---")
print("1. Se der erro de permissão, feche o 'screen' ou outros terminais.")
print("2. Se o script rodar mas não aparecer nada, aperte o botão EN (Reset) no ESP32.")

# CONFIGURAÇÃO DA PORTA (MÉTODO ANTI-RESET)
try:
    ser = serial.Serial()
    ser.port = PORTA
    ser.baudrate = BAUD
    ser.timeout = 1
    
    # O TRUQUE: Desativar sinais de controle antes de abrir a porta
    ser.dtr = False
    ser.rts = False
    
    ser.open()
    
    # Reforça o estado dos pinos após abrir
    ser.dtr = False
    ser.rts = False

except Exception as e:
    print(f"\nERRO FATAL AO ABRIR PORTA: {e}")
    print("Verifique se o 'screen' ainda está rodando (Ctrl+A, k, y para fechar).")
    exit()

print("\nConectado! Aguardando dados... (Ctrl+C para parar)")

buffer = ""

while True:
    try:
        if ser.in_waiting > 0:
            try:
                chunk = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                buffer += chunk
                
                while '\n' in buffer:
                    linha, buffer = buffer.split('\n', 1)
                    linha = linha.strip()
                    
                    if not linha: continue

                    salvar_log_bruto(linha)
                    print(f"[ESP32]: {linha}")

                    if "JSON_START:" in linha:
                        try:
                            json_part = linha.split("JSON_START:")[1]
                            dados = json.loads(json_part)
                            salvar_json_estruturado(dados)
                        except Exception as e:
                            print(f" >> [ERRO JSON] Falha ao decodificar: {e}")

            except Exception as e:
                print(f"Erro de leitura: {e}")

    except KeyboardInterrupt:
        print("\nCaptura finalizada pelo usuário.")
        ser.close()
        break