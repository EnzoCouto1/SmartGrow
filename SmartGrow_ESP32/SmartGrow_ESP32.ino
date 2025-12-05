/*
 * SmartGrow ESP32 - Código Final v10.0 (Ventilação Manual Full ON)
 * * --- LÓGICA ATUALIZADA ---
 * 1. LUZ:
 * - Auto: ACESA DIRETO (Regra do cliente).
 * - Manual: Obedece controle manual.
 * * 2. IRRIGAÇÃO:
 * - Auto: Lógica Fuzzy (Tempo variável).
 * - Manual: Pulso de segurança (5 segundos) ao ativar.
 * * 3. VENTILAÇÃO (NOVO):
 * - Auto: Lógica Fuzzy (Tempo proporcional à temperatura).
 * - Manual: Se ativado, fica LIGADA O TEMPO TODO (100% on).
 */

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

// --- Controles de Tempo ---
const unsigned long CONTROL_CYCLE_MS = 60000; // Ciclo Total: 1 minuto
const unsigned long PUMP_WINDOW_MS = 10000;   // Janela de Atuação da Bomba (Auto): 10s

unsigned long cycleStartTime = 0;

// --- Globais ---
DHT dht(DHT_PIN, DHT_TYPE);
unsigned long lastSensorRead = 0;
unsigned long lastApiCall = 0;

const unsigned long SENSOR_INTERVAL = 5000; 
const unsigned long API_INTERVAL = 2000;   

float temperatura = 0.0;
float umidade_ar = 0.0;
float umidade_solo = 0.0;

// Variáveis de Controle vindas da API
float nivel_irrigacao = 0.0;
float velocidade_ventilacao = 0.0;
float nivel_iluminacao = 0.0;

// Variáveis de Modo Automático
bool modo_auto_luz = false; 
bool modo_auto_irrigacao = false; 
bool modo_auto_ventilacao = false; // NOVA VARIÁVEL PARA O VENTILADOR

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
    
    // 1. Ler os níveis (Atuadores)
    if (responseDoc.containsKey("estado_atual")) {
      nivel_irrigacao = responseDoc["estado_atual"]["nivel_irrigacao"];
      velocidade_ventilacao = responseDoc["estado_atual"]["velocidade_ventilacao"];
      nivel_iluminacao = responseDoc["estado_atual"]["nivel_iluminacao"];
    }

    // 2. Ler os MODOS AUTOMÁTICOS
    if (responseDoc.containsKey("modo_automatico")) {
        modo_auto_luz = responseDoc["modo_automatico"]["iluminacao"];
        modo_auto_irrigacao = responseDoc["modo_automatico"]["irrigacao"];
        
        // Verifica se existe a chave de ventilação, senão assume padrão (false ou true conforme preferir)
        if (responseDoc["modo_automatico"].containsKey("ventilacao")) {
            modo_auto_ventilacao = responseDoc["modo_automatico"]["ventilacao"];
        }
    }
  }
  http.end();
}

// --- Controle de Atuadores ---

void gerenciarAtuadores(unsigned long currentTime) {
    if (currentTime - cycleStartTime >= CONTROL_CYCLE_MS) {
        cycleStartTime = currentTime;
    }
    unsigned long elapsedTime = currentTime - cycleStartTime;

    // ---------------------------------------------------------
    // 1. LÓGICA DA BOMBA (IRRIGAÇÃO)
    // ---------------------------------------------------------
    unsigned long pumpOnDuration = 0;

    if (modo_auto_irrigacao == true) {
        // AUTO: Usa janela Fuzzy
        pumpOnDuration = (PUMP_WINDOW_MS * (nivel_irrigacao / 100.0));
    } 
    else {
        // MANUAL: Pulso Fixo de Segurança (5s)
        if (nivel_irrigacao >= 99.0) {
            pumpOnDuration = 5000; 
        } else {
            pumpOnDuration = 0;
        }
    }

    // Aplicação Física Bomba
    if (elapsedTime < pumpOnDuration) {
        digitalWrite(PUMP_RELAY_1, HIGH);
        digitalWrite(PUMP_RELAY_2, HIGH);
    } else {
        digitalWrite(PUMP_RELAY_1, LOW);
        digitalWrite(PUMP_RELAY_2, LOW);
    }

    // ---------------------------------------------------------
    // 2. LÓGICA DA VENTILAÇÃO (NOVA IMPLEMENTAÇÃO)
    // ---------------------------------------------------------
    unsigned long fanOnDuration = 0;

    if (modo_auto_ventilacao == true) {
        // AUTO: Proporcional à temperatura (Lógica Fuzzy padrão)
        // Ex: Se ventilação é 50%, liga por 30s dentro de 1 minuto
        fanOnDuration = (CONTROL_CYCLE_MS * (velocidade_ventilacao / 100.0));
    }
    else {
        // MANUAL: "Ligada o tempo todo" se o botão estiver ON
        if (velocidade_ventilacao >= 99.0) {
            fanOnDuration = CONTROL_CYCLE_MS + 1000; // Define um tempo maior que o ciclo para garantir ON constante
        } else {
            fanOnDuration = 0;
        }
    }

    // Aplicação Física Ventilador
    if (elapsedTime < fanOnDuration) {
        digitalWrite(FAN_RELAY_PIN, HIGH);
    } else {
        digitalWrite(FAN_RELAY_PIN, LOW);
    }

    // ---------------------------------------------------------
    // 3. LÓGICA DA LUZ
    // ---------------------------------------------------------
    unsigned long lightOnDuration = (CONTROL_CYCLE_MS * (nivel_iluminacao / 100.0));

    if (modo_auto_luz == true) {
        // Auto: LUZ SEMPRE ACESA
        digitalWrite(LIGHT_RELAY_PIN, HIGH);
    } 
    else {
        // Manual: Obedece controle
        if (elapsedTime < lightOnDuration) {
            digitalWrite(LIGHT_RELAY_PIN, HIGH);
        } else {
            digitalWrite(LIGHT_RELAY_PIN, LOW);
        }
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
  
  // Estado Inicial
  digitalWrite(PUMP_RELAY_1, LOW);
  digitalWrite(PUMP_RELAY_2, LOW);
  digitalWrite(FAN_RELAY_PIN, LOW);
  
  // Luz inicia ligada
  digitalWrite(LIGHT_RELAY_PIN, HIGH); 
  
  dht.begin();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  
  cycleStartTime = millis();
}

// --- Loop ---

void loop() {
  unsigned long currentTime = millis();
  
  // Sensores (5s)
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    temperatura = lerTemperatura();
    umidade_ar = lerUmidadeAr();
    umidade_solo = lerUmidadeSolo();
    lastSensorRead = currentTime;
  }
  
  // API (2s)
  if (currentTime - lastApiCall >= API_INTERVAL) {
    enviarDadosParaAPI();
    lastApiCall = currentTime;
  }
  
  // Atuadores
  gerenciarAtuadores(currentTime);
  
  delay(10);
}