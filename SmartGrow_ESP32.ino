/*
 * SmartGrow ESP32 - Código Final v14.1 (Ultrassônico Ativado)
 * -------------------------------------------------------------------
 * ATUALIZAÇÃO v14.1:
 * - Adicionada leitura do Sensor Ultrassônico (HC-SR04).
 * - Pinos: Trig (5) e Echo (18).
 * - Exibe a distância em cm no Monitor Serial a cada 5 segundos.
 * -------------------------------------------------------------------
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

// --- MUDANÇA IMPORTANTE AQUI ---
// Pinos 25/26 conflitam com WiFi. Usando 33/32 (ADC1/Digital Seguro)
#define PUMP_RELAY_1 33  // Mude o fio da Bomba 1 para o pino 33
#define PUMP_RELAY_2 32  // Mude o fio da Bomba 2 para o pino 32

#define LIGHT_RELAY_PIN 13
#define FAN_RELAY_PIN 2

// --- Controles de Tempo ---
const unsigned long CONTROL_CYCLE_MS = 60000; // Ciclo Global
const unsigned long PUMP_WINDOW_MS = 10000;   // Janela Auto Bomba

unsigned long cycleStartTime = 0;
unsigned long manualIrrigationStart = 0; // Timer exclusivo para manual

// --- Globais ---
DHT dht(DHT_PIN, DHT_TYPE);
unsigned long lastSensorRead = 0;
unsigned long lastApiCall = 0;

const unsigned long SENSOR_INTERVAL = 5000; 
const unsigned long API_INTERVAL = 2000;   

float temperatura = 0.0;
float umidade_ar = 0.0;
float umidade_solo = 0.0;
float distancia_ultrassonico = 0.0; // Variável para o ultrassônico

// Variáveis de Controle
float nivel_irrigacao = 0.0;
float velocidade_ventilacao = 0.0;
float nivel_iluminacao = 0.0;

// Modos
bool modo_auto_luz = false; 
bool modo_auto_irrigacao = false; 
bool modo_auto_ventilacao = false;

bool lastPumpStatePrint = false; 

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

// NOVA FUNÇÃO: Ler Ultrassônico
float lerNivelReservatorio() {
    // Garante que o trigger está baixo
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    delayMicroseconds(2);
    
    // Envia pulso de 10us
    digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    
    // Lê o tempo de resposta do Echo
    long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH);
    
    // Calcula distância em cm (Velocidade do som: 0.034 cm/us)
    float distance = duration * 0.034 / 2;
    
    return distance;
}

// --- Comunicação ---

void enviarDadosParaAPI() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Erro: WiFi desconectado!");
    return;
  }
  
  HTTPClient http;
  http.begin(String(api_url) + "/leituras");
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(1024);
  doc["temperatura_celsius"] = temperatura;
  doc["umidade_solo"] = umidade_solo;
  // Se quiser enviar o nível para a API no futuro, adicione aqui:
  // doc["nivel_reservatorio"] = distancia_ultrassonico;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpCode = http.POST(jsonString);
  
  if (httpCode > 0) {
    String response = http.getString();
    
    DynamicJsonDocument responseDoc(1024);
    DeserializationError error = deserializeJson(responseDoc, response);

    if (error) {
      Serial.print("Erro JSON: ");
      Serial.println(error.c_str());
      return;
    }
    
    // Ler Modos
    if (responseDoc.containsKey("modo_automatico")) {
        modo_auto_luz = responseDoc["modo_automatico"]["iluminacao"];
        modo_auto_irrigacao = responseDoc["modo_automatico"]["irrigacao"];
        if (responseDoc["modo_automatico"].containsKey("ventilacao")) {
            modo_auto_ventilacao = responseDoc["modo_automatico"]["ventilacao"];
        }
    }

    // Ler Estados e Detectar Borda de Subida do Manual
    if (responseDoc.containsKey("estado_atual")) {
      float novo_nivel_irrigacao = responseDoc["estado_atual"]["nivel_irrigacao"];
      
      // Se detectarmos que ligou o botão manual AGORA -> Marcamos o tempo no timer isolado
      if (!modo_auto_irrigacao && novo_nivel_irrigacao >= 99.0 && nivel_irrigacao < 99.0) {
          manualIrrigationStart = millis(); 
          Serial.println(">>> START MANUAL (Timer Isolado) <<<");
      }

      nivel_irrigacao = novo_nivel_irrigacao;
      velocidade_ventilacao = responseDoc["estado_atual"]["velocidade_ventilacao"];
      nivel_iluminacao = responseDoc["estado_atual"]["nivel_iluminacao"];
    }

    Serial.printf("API -> Irrig: %.0f%% | Vent: %.0f%% | Luz: %.0f%%\n", 
                  nivel_irrigacao, velocidade_ventilacao, nivel_iluminacao);
  }
  http.end();
}

// --- Controle de Atuadores ---

void gerenciarAtuadores(unsigned long currentTime) {
    if (currentTime - cycleStartTime >= CONTROL_CYCLE_MS) {
        cycleStartTime = currentTime;
    }
    unsigned long elapsedTimeGlobal = currentTime - cycleStartTime;

    // ---------------------------------------------------------
    // 1. BOMBA (Lógica Híbrida)
    // ---------------------------------------------------------
    bool pumpState = false; 

    if (modo_auto_irrigacao == true) {
        // --- MODO AUTO ---
        unsigned long pumpOnDuration = 0;
        if (nivel_irrigacao > 30.0) { 
            pumpOnDuration = (PUMP_WINDOW_MS * (nivel_irrigacao / 100.0));
        }
        if (elapsedTimeGlobal < pumpOnDuration) pumpState = true;
    } 
    else {
        // --- MODO MANUAL (Timer Isolado) ---
        if (nivel_irrigacao >= 99.0) {
            unsigned long timeSinceManual = currentTime - manualIrrigationStart;
            if ((timeSinceManual % 60000) < 5000) {
                pumpState = true;
            }
        }
    }

    // Debug Serial
    if (pumpState != lastPumpStatePrint) {
        if (pumpState) Serial.println("!!! ATIVANDO BOMBAS (Pinos 33 e 32) !!!");
        else Serial.println("... Desativando Bombas ...");
        lastPumpStatePrint = pumpState;
    }

    // Aplicação Física
    if (pumpState) {
        digitalWrite(PUMP_RELAY_1, HIGH);
        digitalWrite(PUMP_RELAY_2, HIGH);
    } else {
        digitalWrite(PUMP_RELAY_1, LOW);
        digitalWrite(PUMP_RELAY_2, LOW);
    }

    // ---------------------------------------------------------
    // 2. VENTILAÇÃO
    // ---------------------------------------------------------
    unsigned long fanOnDuration = 0;
    
    if (modo_auto_ventilacao == true) {
        if (velocidade_ventilacao > 30.0) {
            fanOnDuration = (CONTROL_CYCLE_MS * (velocidade_ventilacao / 100.0));
        }
    } else {
        if (velocidade_ventilacao >= 99.0) fanOnDuration = CONTROL_CYCLE_MS + 1000; 
    }

    if (elapsedTimeGlobal < fanOnDuration) {
        digitalWrite(FAN_RELAY_PIN, HIGH);
    } else {
        digitalWrite(FAN_RELAY_PIN, LOW);
    }

    // ---------------------------------------------------------
    // 3. LUZ
    // ---------------------------------------------------------
    unsigned long lightOnDuration = (CONTROL_CYCLE_MS * (nivel_iluminacao / 100.0));
    
    if (modo_auto_luz == true) {
        digitalWrite(LIGHT_RELAY_PIN, HIGH);
    } else {
        if (elapsedTimeGlobal < lightOnDuration) digitalWrite(LIGHT_RELAY_PIN, HIGH);
        else digitalWrite(LIGHT_RELAY_PIN, LOW);
    }
}

// --- Setup ---

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n--- SMARTGROW V14.1 (Pinos 33/32 + Ultrassom) ---");

  // Configura os NOVOS pinos
  pinMode(PUMP_RELAY_1, OUTPUT);
  pinMode(PUMP_RELAY_2, OUTPUT);
  
  pinMode(FAN_RELAY_PIN, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  
  digitalWrite(PUMP_RELAY_1, LOW);
  digitalWrite(PUMP_RELAY_2, LOW);
  digitalWrite(FAN_RELAY_PIN, LOW);
  digitalWrite(LIGHT_RELAY_PIN, HIGH); 
  
  dht.begin();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado!");
  
  cycleStartTime = millis();
  manualIrrigationStart = millis(); 
}

// --- Loop ---

void loop() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    temperatura = lerTemperatura();
    umidade_ar = lerUmidadeAr();
    umidade_solo = lerUmidadeSolo();
    
    // Lê e printa o Ultrassônico
    distancia_ultrassonico = lerNivelReservatorio();
    
    Serial.printf("[Sensor] U.Solo: %.0f%% | Reservatorio: %.1f cm\n", umidade_solo, distancia_ultrassonico);
    lastSensorRead = currentTime;
  }
  
  if (currentTime - lastApiCall >= API_INTERVAL) {
    enviarDadosParaAPI();
    lastApiCall = currentTime;
  }
  
  gerenciarAtuadores(currentTime);
  
  delay(10);
}