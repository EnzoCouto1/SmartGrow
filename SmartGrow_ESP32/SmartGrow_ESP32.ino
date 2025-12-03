#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// =========================================================
// 1. CONFIGURAÇÃO DE REDE E PINAGEM (DEFINIÇÕES)
// =========================================================

const char* ssid = "WLL-Inatel";
const char* password = "inatelsemfio";
const char* api_url = "https://smartgrow-ajtn.onrender.com";

// Pinos dos Sensores
#define DHT_PIN 23
#define DHT_TYPE DHT22
#define SOIL_PIN_1 34
#define SOIL_PIN_2 35
#define ULTRASONIC_TRIG_PIN 5
#define ULTRASONIC_ECHO_PIN 18

// Pinos dos Atuadores
#define PUMP_RELAY_1 26 // Bomba 1
#define PUMP_RELAY_2 25 // Bomba 2
#define LIGHT_RELAY_PIN 13 // Iluminação
#define FAN_RELAY_PIN 2 // Ventilação

// =========================================================
// 2. VARIÁVEIS GLOBAIS E TEMPORIZADORES
// =========================================================

// Configuração de Ciclo e Duração
const unsigned long CONTROL_CYCLE_MS = 60000;
unsigned long cycleStartTime = 0;

// DURAÇÃO: BOMBA E FAN ATIVOS POR 5 SEGUNDOS
const unsigned long PUMP_DURATION_MS = 5000;
const unsigned long FAN_DURATION_MS = 5000;

// Temporizadores de Leitura e Comunicação
DHT dht(DHT_PIN, DHT_TYPE);
unsigned long lastSensorRead = 0;
unsigned long lastApiCall = 0;
const unsigned long SENSOR_INTERVAL = 5000; 
const unsigned long API_INTERVAL = 30000; 

// Dados dos Sensores
float temperatura = 0.0;
float umidade_ar = 0.0;
float umidade_solo = 0.0;

// Variáveis de Controle (API)
float nivel_irrigacao = 0.0;
float velocidade_ventilacao = 0.0;
float nivel_iluminacao = 0.0;

// =========================================================
// 3. FUNÇÕES DE LEITURA (SENSORES)
// =========================================================

float lerTemperatura() {
  float temp = dht.readTemperature();
  if (isnan(temp)) return -999;
  return temp;
}

float lerUmidadeAr() {
  float umid = dht.readHumidity();
  if (isnan(umid)) return -999;
  return umid;
}

float lerUmidadeSolo() {
  int val1 = analogRead(SOIL_PIN_1);
  int val2 = analogRead(SOIL_PIN_2);
  long media = (val1 + val2) / 2;
  
  float umidade = map(media, 4095, 0, 0, 100); 
  return constrain(umidade, 0, 100);
}

// =========================================================
// 4. FUNÇÃO DE COMUNICAÇÃO (API)
// =========================================================

void enviarDadosParaAPI() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("API: WiFi desconectado. Pulando chamada."); // LINHA CORRIGIDA
    return;
  }
  
  HTTPClient http;
  http.begin(String(api_url) + "/leituras");
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(1024);
  doc["temperatura_celsius"] = temperatura;
  doc["umidade_solo"] = umidade_solo;
   
 String jsonString;
  serializeJson(doc, jsonString);
  
  int httpCode = http.POST(jsonString);
  
  if (httpCode > 0) {
    String response = http.getString();
    DynamicJsonDocument responseDoc(1024);
    if (deserializeJson(responseDoc, response) == DeserializationError::Ok) {
      if (responseDoc.containsKey("estado_atual")) {
        nivel_irrigacao = responseDoc["estado_atual"]["nivel_irrigacao"];
        velocidade_ventilacao = responseDoc["estado_atual"]["velocidade_ventilacao"];
        nivel_iluminacao = responseDoc["estado_atual"]["nivel_iluminacao"];
        Serial.printf("API OK! Irrigacao: %.0f, Fan: %.0f\n", nivel_irrigacao, velocidade_ventilacao);
      }
    } else {
        Serial.println("API: Falha ao desserializar resposta.");
    }
  } else {
    Serial.printf("API: POST falhou, codigo %d\n", httpCode);
  }
  http.end();
}

// =========================================================
// 5. FUNÇÃO DE CONTROLE DE ATUADORES
// =========================================================

void gerenciarAtuadores(unsigned long currentTime) {
    // Reinicia ciclo de 1 minuto
    if (currentTime - cycleStartTime >= CONTROL_CYCLE_MS) {
        cycleStartTime = currentTime;
        Serial.println("--- NOVO CICLO DE CONTROLE (1 MIN) ---");
    }
    unsigned long elapsedTime = currentTime - cycleStartTime;

    // Lógica da Bomba (Pulso de 5.0s)
    if (elapsedTime < PUMP_DURATION_MS && nivel_irrigacao > 30.0) {
        digitalWrite(PUMP_RELAY_1, HIGH);
        digitalWrite(PUMP_RELAY_2, HIGH);
    } else {
        digitalWrite(PUMP_RELAY_1, LOW);
        digitalWrite(PUMP_RELAY_2, LOW);
    }
    
    // Lógica da Ventoinha (Pulso de 5.0s)
    if (elapsedTime < FAN_DURATION_MS && velocidade_ventilacao > 30.0) {
        digitalWrite(FAN_RELAY_PIN, HIGH);
    } else {
        digitalWrite(FAN_RELAY_PIN, LOW);
    }
}

// =========================================================
// 6. SETUP E LOOP PRINCIPAL
// =========================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Iniciando SmartGrow v5.1...");
  
  // 6.1 Configuração dos Pinos
  pinMode(PUMP_RELAY_1, OUTPUT);
  pinMode(PUMP_RELAY_2, OUTPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  
  // 6.2 Inicialização (Desliga Bombas/Fan, Liga Luz)
  digitalWrite(PUMP_RELAY_1, LOW);
  digitalWrite(PUMP_RELAY_2, LOW);
  digitalWrite(FAN_RELAY_PIN, LOW);
  
  // LUZ SEMPRE LIGADA (HIGH = LIGA)
  digitalWrite(LIGHT_RELAY_PIN, HIGH);
  Serial.println("Luz: ON (Permanente)");

  dht.begin();
  
  // 6.3 Conexão WiFi
  Serial.print("Conectando a ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado!");
  } else {
    Serial.println("\nFalha no WiFi - Rodando offline.");
  }
  
  cycleStartTime = millis();
}

void loop() {
  unsigned long currentTime = millis();
  
  // 7.1 Sensores (a cada 5s)
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    temperatura = lerTemperatura();
    umidade_ar = lerUmidadeAr();
    umidade_solo = lerUmidadeSolo();
   
    Serial.printf("T:%.1f H:%.1f S:%.1f\n", temperatura, umidade_ar, umidade_solo);
    lastSensorRead = currentTime;
  }
  
  // 7.2 API (a cada 30s)
  if (currentTime - lastApiCall >= API_INTERVAL) {
    enviarDadosParaAPI();
    lastApiCall = currentTime;
  }
  
  // 7.3 Atuadores (Controle de tempo fixo)
  gerenciarAtuadores(currentTime);
  
  delay(10);
}