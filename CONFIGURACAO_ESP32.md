# Configuração do ESP32 para SmartGrow

## Pré-requisitos

### 1. **Software Necessário:**
- **Arduino IDE** (versão 2.0 ou superior)
- **ESP32 Board Package** instalado no Arduino IDE

### 2. **Bibliotecas Necessárias:**
Instale estas bibliotecas no Arduino IDE (Tools → Manage Libraries):

```
- WiFi (já incluída)
- HTTPClient (já incluída) 
- ArduinoJson (por Benoit Blanchon)
- DHT sensor library (por Adafruit)
- Adafruit Unified Sensor (por Adafruit)
```

### 3. **Hardware Necessário:**
- ESP32 DevKit
- Sensor DHT22 (temperatura e umidade do ar)
- Sensor de umidade do solo (analógico)
- Sensor ultrassônico HC-SR04
- 5x Módulos de relé
- Jumpers e protoboard
- Fonte de alimentação 5V/12V para os relés
- Sensor de luminosidade

## Configuração do Código

### 1. **Configurar WiFi:**
No arquivo `SmartGrow_ESP32.ino`, altere estas linhas:

```cpp
const char* ssid = "SEU_WIFI_SSID";           // Nome da sua rede WiFi
const char* password = "SUA_SENHA_WIFI";      // Senha da sua rede WiFi
const char* api_url = "http://SEU_IP:8000";   // IP do computador com a API
```

**Exemplo:**
```cpp
const char* ssid = "MinhaCasa_WiFi";
const char* password = "minhasenha123";
const char* api_url = "http://192.168.1.100:8000";
```

### 2. **Descobrir o IP do Computador:**
No Windows, abra o Prompt de Comando e digite:
```cmd
ipconfig
```
Procure por "Endereço IPv4" da sua conexão WiFi.

## Conexões dos Sensores

### **Sensor DHT22 (Temperatura e Umidade do Ar):**
```
DHT22 VCC  → ESP32 3.3V
DHT22 GND  → ESP32 GND
DHT22 DATA → ESP32 GPIO23
```

### **Sensor de Umidade do Solo:**
```
Sensor VCC → ESP32 3.3V
Sensor GND → ESP32 GND
Sensor AOUT → ESP32 GPIO34
```

### **Sensor Ultrassônico HC-SR04:**
```
HC-SR04 VCC  → ESP32 5V
HC-SR04 GND  → ESP32 GND
HC-SR04 TRIG → ESP32 GPIO5
HC-SR04 ECHO → ESP32 GPIO4
```

### **Sensor de Luminosidade (LDR):**
```
Sensor VCC → ESP32 3.3V
Sensor GND → ESP32 GND 
Sensor AOUT → ESP32 GPIO32
```

### **Módulos de Relé:**
```
Relé Bomba (GPIO26):
  - VCC → ESP32 3.3V
  - GND → ESP32 GND
  - IN  → ESP32 GPIO26

Relé Iluminação (GPIO27):
  - VCC → ESP32 3.3V
  - GND → ESP32 GND
  - IN  → ESP32 GPIO27

Relé Ventilação (GPIO14):
  - VCC → ESP32 3.3V
  - GND → ESP32 GND
  - IN  → ESP32 GPIO14

Relé Aquecedor (GPIO12):
  - VCC → ESP32 3.3V
  - GND → ESP32 GND
  - IN  → ESP32 GPIO12

Relé Umidificador (GPIO13):
  - VCC → ESP32 3.3V
  - GND → ESP32 GND
  - IN  → ESP32 GPIO13
```

## Como Executar

### 1. **Preparar o Arduino IDE:**
- Abra o Arduino IDE
- Vá em File → Preferences
- Adicione esta URL em "Additional Board Manager URLs":
  ```
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  ```

### 2. **Instalar o ESP32 Board Package:**
- Vá em Tools → Board → Boards Manager
- Procure por "ESP32"
- Instale "ESP32 by Espressif Systems"

### 3. **Configurar a Placa:**
- Tools → Board → ESP32 Arduino → ESP32 Dev Module
- Tools → Port → Selecione a porta COM do seu ESP32
- Tools → Upload Speed → 115200

### 4. **Carregar o Código:**
- Abra o arquivo `SmartGrow_ESP32.ino`
- Configure o WiFi e IP da API
- Clique em Upload (seta para a direita)

## Monitoramento

### **Serial Monitor:**
- Abra Tools → Serial Monitor
- Configure para 115200 baud
- Você verá as leituras dos sensores e status da comunicação

### **Exemplo de Saída:**
```
SmartGrow ESP32 - Iniciando...
Conectando ao WiFi.....
WiFi conectado!
IP: 192.168.1.150
API URL: http://192.168.1.100:8000

Testando sensores...
Leituras iniciais:
  Temperatura: 24.5°C
  Umidade do ar: 65.2%
  Umidade do solo: 45.8%
  Distância da água: 12.3 cm

Sistema iniciado com sucesso!

Leituras dos sensores:
  Temperatura: 24.5°C
  Umidade do ar: 65.2%
  Umidade do solo: 45.8%
  Distância da água: 12.3 cm

Enviando dados para API:
{"temperatura_celsius":24.5,"umidade_solo":45.8}
Resposta da API: {"status":"sucesso","mensagem":"Leitura processada com lógica fuzzy.","estado_atual":{"nivel_irrigacao":25.3,"velocidade_ventilacao":30.1}}
Comandos recebidos:
  Irrigação: 25.3%
  Ventilação: 30.1%
Bomba de água: DESLIGADA
Ventilação: DESLIGADA
```

## Solução de Problemas

### **ESP32 não conecta ao WiFi:**
- Verifique se o SSID e senha estão corretos
- Certifique-se de que a rede WiFi está funcionando
- Verifique se o ESP32 está próximo ao roteador

### **Erro de comunicação com API:**
- Verifique se o IP da API está correto
- Certifique-se de que a API está rodando no computador
- Teste se consegue acessar `http://IP:8000` no navegador

### **Sensores não funcionam:**
- Verifique as conexões dos fios
- Certifique-se de que os sensores estão alimentados
- Verifique se as bibliotecas estão instaladas

### **Relés não funcionam:**
- Verifique se os módulos de relé estão alimentados (5V ou 12V)
- Confirme as conexões dos pinos de controle
- Teste os relés manualmente

## Funcionamento do Sistema

1. **Leitura dos Sensores:** A cada 5 segundos
2. **Envio para API:** A cada 30 segundos
3. **Controle dos Atuadores:** Baseado na resposta da API
4. **Lógica Fuzzy:** Processada no servidor Python

O ESP32 funciona como um "cliente" que envia dados e recebe comandos, enquanto a lógica inteligente fica no servidor Python.
