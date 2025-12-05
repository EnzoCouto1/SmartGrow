#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// --- Configurações de Rede ---
const char* ssid = "WLL-Inatel";
const char* password = "inatelsemfio";
const char* api_url = "https://smartgrow-ajtn.onrender.com";

// --- Pinagem ---
#define DHT_PIN 23
#define DHT_TYPE DHT22
#define SOIL_PIN_1 34
#define SOIL_PIN_2 35
#define ULTRASONIC_TRIG_PIN 5
#define ULTRASONIC_ECHO_PIN 18

#define PUMP_RELAY_1 26
#define PUMP_RELAY_2 25
#define LIGHT_RELAY_PIN 13
#define FAN_RELAY_PIN 2

// --- Configurações de Tempo ---
const unsigned long CONTROL_CYCLE_MS = 60000; // Ciclo Total: 1 minuto
const unsigned long PUMP_WINDOW_MS = 10000;   // Janela de Atuação da Bomba: 10 segundos

unsigned long cycleStartTime = 0;
unsigned long lastLightToggle = 0; 

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
  int val1 = analogRead(SOIL_PIN_1);
  int val2 = analogRead(SOIL_PIN_2);
  long media = (val1 + val2) / 2;
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

// --- Controle de Atuadores (Lógica Atualizada) ---

void gerenciarAtuadores(unsigned long currentTime) {
    // 1. Reinicia o ciclo a cada 1 minuto (60 segundos)
    if (currentTime - cycleStartTime >= CONTROL_CYCLE_MS) {
        cycleStartTime = currentTime;
    }
    unsigned long elapsedTime = currentTime - cycleStartTime;

    // --- LÓGICA DA BOMBA (Janela de 10s) ---
    unsigned long pumpOnDuration = 0;

    if (nivel_irrigacao >= 99.0) {
        // MANUAL: Liga por 5 segundos fixos (como pedido anteriormente)
        pumpOnDuration = 5000; 
    } 
    else if (nivel_irrigacao > 30.0) {
        // FUZZY: Calcula a porcentagem DENTRO dos 10 segundos
        // Ex: 35% -> 0.35 * 10000 = 3500ms (3.5 segundos)
        pumpOnDuration = (PUMP_WINDOW_MS * (nivel_irrigacao / 100.0));
    }
    // Se for < 30% (Zona Morta), pumpOnDuration fica 0.

    // --- LÓGICA DA VENTOINHA (Ciclo Completo de 1 min) ---
    unsigned long fanOnDuration = 0;
    if (velocidade_ventilacao > 30.0) {
        // A ventoinha usa o ciclo cheio (60s) para ventilar melhor
        fanOnDuration = (CONTROL_CYCLE_MS * (velocidade_ventilacao / 100.0));
    }

    // --- LÓGICA DA ILUMINAÇÃO (Time Based) ---
    // Se o backend mandar ligar, liga pelo ciclo todo
    unsigned long lightOnDuration = (CONTROL_CYCLE_MS * (nivel_iluminacao / 100.0));


    // --- EXECUÇÃO FÍSICA ---

    // Bombas
    if (elapsedTime < pumpOnDuration) {
        digitalWrite(PUMP_RELAY_1, HIGH);
        digitalWrite(PUMP_RELAY_2, HIGH);
    } else {
        digitalWrite(PUMP_RELAY_1, LOW);
        digitalWrite(PUMP_RELAY_2, LOW);
    }

    // Ventoinha
    if (elapsedTime < fanOnDuration) {
        digitalWrite(FAN_RELAY_PIN, HIGH);
    } else {
        digitalWrite(FAN_RELAY_PIN, LOW);
    }

    // Iluminação (Controlada pelo Backend)
    if (elapsedTime < lightOnDuration) {
        digitalWrite(LIGHT_RELAY_PIN, HIGH);
    } else {
        digitalWrite(LIGHT_RELAY_PIN, LOW);
    }
}

// --- Setup ---

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(PUMP_RELAY_1, OUTPUT);
  pinMode(PUMP_RELAY_2, OUTPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  
  digitalWrite(PUMP_RELAY_1, LOW);
  digitalWrite(PUMP_RELAY_2, LOW);
  digitalWrite(FAN_RELAY_PIN, LOW);
  digitalWrite(LIGHT_RELAY_PIN, LOW);
  
  dht.begin();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  
  cycleStartTime = millis();
  lastLightToggle = millis();
}

// --- Loop ---

void loop() {
  unsigned long currentTime = millis();
  
  // 1. Sensores
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    temperatura = lerTemperatura();
    umidade_ar = lerUmidadeAr();
    umidade_solo = lerUmidadeSolo();
    lastSensorRead = currentTime;
  }
  
  // 2. API
  if (currentTime - lastApiCall >= API_INTERVAL) {
    enviarDadosParaAPI();
    lastApiCall = currentTime;
  }
  
  // 3. Atuadores (Lógica Fuzzy/Temporal)
  gerenciarAtuadores(currentTime);

  // 4. Iluminação (Piscar Manual para Teste - Opcional, pode remover se quiser só o backend)
  /* if (currentTime - lastLightToggle >= 4000) {
      lightState = !lightState;
      digitalWrite(LIGHT_RELAY_PIN, lightState ? HIGH : LOW);
      lastLightToggle = currentTime;
  }
  */
  
  delay(10);
}