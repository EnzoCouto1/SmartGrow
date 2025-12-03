#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// --- Configurações de Rede ---
const char* ssid = "WLL-Inatel";
const char* password = "inatelsemfio";
const char* api_url = "https://smartgrow-ajtn.onrender.com";

// --- Pinagem (Configuração V5.0) ---
#define DHT_PIN 23
#define DHT_TYPE DHT22

// Sensores de Solo (2 unidades conforme seu último pedido)
#define SOIL_PIN_1 34
#define SOIL_PIN_2 35

#define ULTRASONIC_TRIG_PIN 5
#define ULTRASONIC_ECHO_PIN 18

// Atuadores
#define PUMP_RELAY_1 26   // Bomba 1
#define PUMP_RELAY_2 25   // Bomba 2
#define LIGHT_RELAY_PIN 13 // Iluminação (Pisca 4s)
#define FAN_RELAY_PIN 2    // Ventilação

// --- Controles de Tempo ---
const unsigned long CONTROL_CYCLE_MS = 60000; // Ciclo 1 minuto
unsigned long cycleStartTime = 0;
unsigned long lastLightToggle = 0; // Timer da luz

// Estados Internos
bool pumpState = false;
bool fanState = false;
bool lightState = false;

// --- Globais ---
DHT dht(DHT_PIN, DHT_TYPE);
unsigned long lastSensorRead = 0;
unsigned long lastApiCall = 0;
const unsigned long SENSOR_INTERVAL = 5000; 
const unsigned long API_INTERVAL = 30000;   

float temperatura = 0.0;
float umidade_ar = 0.0;
float umidade_solo = 0.0;

float nivel_irrigacao = 0.0;
float velocidade_ventilacao = 0.0;
float nivel_iluminacao = 0.0;

// --- Funções de Leitura ---

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
  // Média dos 2 sensores
  int val1 = analogRead(SOIL_PIN_1);
  int val2 = analogRead(SOIL_PIN_2);
  long media = (val1 + val2) / 2;
  
  // Calibração: 4095(Seco) -> 0(Úmido)
  float umidade = map(media, 0, 4095, 100, 0); 
  return constrain(umidade, 0, 100);
}

// --- Comunicação ---

void enviarDadosParaAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  
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
    deserializeJson(responseDoc, response);
    
    if (responseDoc.containsKey("estado_atual")) {
      nivel_irrigacao = responseDoc["estado_atual"]["nivel_irrigacao"];
      velocidade_ventilacao = responseDoc["estado_atual"]["velocidade_ventilacao"];
      nivel_iluminacao = responseDoc["estado_atual"]["nivel_iluminacao"];
    }
  }
  http.end();
}

// --- Controle de Atuadores ---

void gerenciarAtuadores(unsigned long currentTime) {
    // Reinicia ciclo de 1 minuto
    if (currentTime - cycleStartTime >= CONTROL_CYCLE_MS) {
        cycleStartTime = currentTime;
    }
    unsigned long elapsedTime = currentTime - cycleStartTime;

    // 1. Lógica da Bomba (Pulso de 0.5s se > 30%)
    unsigned long pumpOnDuration = 0;
    if (nivel_irrigacao > 30.0) {
        pumpOnDuration = 5000; // 0.5 segundos fixo
    }

    // 2. Lógica da Ventoinha (Proporcional ao tempo)
    unsigned long fanOnDuration = 0;
    if (velocidade_ventilacao > 30.0) {
        fanOnDuration = (CONTROL_CYCLE_MS * (velocidade_ventilacao / 100.0));
    }

    // Execução Física - Bombas
    if (elapsedTime < pumpOnDuration) {
        digitalWrite(PUMP_RELAY_1, HIGH);
        digitalWrite(PUMP_RELAY_2, HIGH);
    } else {
        digitalWrite(PUMP_RELAY_1, LOW);
        digitalWrite(PUMP_RELAY_2, LOW);
    }

    // Execução Física - Ventoinha
    if (elapsedTime < fanOnDuration) {
        digitalWrite(FAN_RELAY_PIN, HIGH);
    } else {
        digitalWrite(FAN_RELAY_PIN, LOW);
    }
}

// --- Setup ---

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Iniciando SmartGrow v5.0...");
  
  // Configura Pinos
  pinMode(PUMP_RELAY_1, OUTPUT);
  pinMode(PUMP_RELAY_2, OUTPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  
  // Desliga tudo
  digitalWrite(PUMP_RELAY_1, LOW);
  digitalWrite(PUMP_RELAY_2, LOW);
  digitalWrite(FAN_RELAY_PIN, LOW);
  digitalWrite(LIGHT_RELAY_PIN, LOW);
  
  dht.begin();
  
  // Conecta WiFi
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
  lastLightToggle = millis();
}

// --- Loop ---

void loop() {
  unsigned long currentTime = millis();
  
  // 1. Sensores (a cada 5s)
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    temperatura = lerTemperatura();
    umidade_ar = lerUmidadeAr();
    umidade_solo = lerUmidadeSolo();
    
    Serial.printf("T:%.1f H:%.1f S:%.1f\n", temperatura, umidade_ar, umidade_solo);
    lastSensorRead = currentTime;
  }
  
  // 2. API (a cada 30s)
  if (currentTime - lastApiCall >= API_INTERVAL) {
    enviarDadosParaAPI();
    lastApiCall = currentTime;
  }
  
  // 3. Atuadores (Bombas e Fans)
  gerenciarAtuadores(currentTime);

  // 4. Iluminação (Piscar a cada 4 segundos)
  if (currentTime - lastLightToggle >= 40000) {
      lightState = !lightState; // Inverte estado
      digitalWrite(LIGHT_RELAY_PIN, lightState ? HIGH : LOW);
      lastLightToggle = currentTime;
      
      // Debug opcional
      // Serial.print("Luz: "); Serial.println(lightState ? "ON" : "OFF");
  }
  
  delay(10);
}
