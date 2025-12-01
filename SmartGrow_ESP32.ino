/*
 * SmartGrow ESP32 - Código Final v4.0 (Baseado no PDF Pinout)
 * * HARDWARE CONFIGURADO (Conforme PDF):
 * - GPIO23: Sensor DHT22 (Temp/Umid Ar)
 * - GPIO34: Sensor Umidade Solo (1 unidade)
 * - GPIO05: Ultrassônico Trig
 * - GPIO04: Ultrassônico Echo
 * - GPIO26: Relé Bomba (Irrigação)
 * - GPIO27: Relé Iluminação
 * - GPIO14: Relé Ventilação
 * * HARDWARE DESATIVADO/COMENTADO (Sem suporte no Backend atual):
 * - GPIO12: Relé Aquecedor
 * - GPIO13: Relé Umidificador
 * - Dados do Ultrassônico (Lê mas não envia para API)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// --- CONFIGURAÇÕES DE REDE ---
const char* ssid = "NOME_DO_WIFI";
const char* password = "SENHA_DO_WIFI";
const char* api_url = "https://smartgrow-ajtn.onrender.com";

// =============================================================================
// PINAGEM (Baseada no PDF)
// =============================================================================

// Sensores
#define DHT_PIN 23
#define DHT_TYPE DHT22
#define SOIL_PIN 34             // Apenas 1 sensor (PDF)
#define ULTRASONIC_TRIG_PIN 5
#define ULTRASONIC_ECHO_PIN 4

// Atuadores (Relés)
#define PUMP_RELAY_PIN 26       // Irrigação
#define LIGHT_RELAY_PIN 27      // Iluminação
#define FAN_RELAY_PIN 14        // Ventilação

// --- ATUADORES EXTRAS (Presentes no PDF, mas sem lógica no Backend) ---
#define HEATER_RELAY_PIN 12     // Aquecedor
#define HUMIDIFIER_RELAY_PIN 13 // Umidificador

// =============================================================================
// CONTROLO (Ciclo 2 min + Zona Morta)
// =============================================================================
const unsigned long CONTROL_CYCLE_MS = 120000;
unsigned long cycleStartTime = 0;

// Estados para controle interno (evitar flood no Serial)
bool pumpState = false;
bool fanState = false;
bool lightState = false;

// =============================================================================
// VARIÁVEIS GLOBAIS
// =============================================================================
DHT dht(DHT_PIN, DHT_TYPE);
unsigned long lastSensorRead = 0;
unsigned long lastApiCall = 0;
const unsigned long SENSOR_INTERVAL = 5000; 
const unsigned long API_INTERVAL = 30000;   

// Variáveis de Leitura
float temperatura = 0.0;
float umidade_ar = 0.0;
float umidade_solo = 0.0;
float distancia_agua = 0.0; // Lido, mas não enviado

// Variáveis de Comando (Vindas da API)
float nivel_irrigacao = 0.0;
float velocidade_ventilacao = 0.0;
float nivel_iluminacao = 0.0;

// =============================================================================
// FUNÇÕES DE LEITURA
// =============================================================================

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
  int valor = analogRead(SOIL_PIN);
  // Calibração padrão: 4095 (Seco) -> 0 (Úmido)
  float umidade = map(valor, 0, 4095, 100, 0); 
  return constrain(umidade, 0, 100);
}

float lerDistanciaAgua() {
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  long duracao = pulseIn(ULTRASONIC_ECHO_PIN, HIGH);
  return (duracao * 0.0343) / 2;
}

// =============================================================================
// COMUNICAÇÃO
// =============================================================================

void enviarDadosParaAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  http.begin(String(api_url) + "/leituras");
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(1024);
  doc["temperatura_celsius"] = temperatura;
  doc["umidade_solo"] = umidade_solo;
  
  /* * COMENTADO: O Backend atual não tem campo para nível de água.
   * Se quiser enviar no futuro, adicione 'nivel_agua' no main.py.
   */
  // doc["nivel_reservatorio"] = distancia_agua; 
  
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
      
      Serial.println("Comandos Recebidos:");
      Serial.printf("Irr: %.1f%% | Ven: %.1f%% | Luz: %.1f%%\n", 
                    nivel_irrigacao, velocidade_ventilacao, nivel_iluminacao);
    }
  }
  http.end();
}

// =============================================================================
// ATUADORES (LÓGICA PROPORCIONAL AO TEMPO)
// =============================================================================

void gerenciarAtuadores(unsigned long currentTime) {
    if (currentTime - cycleStartTime >= CONTROL_CYCLE_MS) {
        cycleStartTime = currentTime;
    }
    unsigned long elapsedTime = currentTime - cycleStartTime;

    // --- 1. Bomba de Água ---
    unsigned long pumpOnDuration = 0;
    if (nivel_irrigacao > 30.0) { // Zona Morta 30%
        pumpOnDuration = (CONTROL_CYCLE_MS * (nivel_irrigacao / 100.0));
    }

    // --- 2. Ventilação ---
    unsigned long fanOnDuration = 0;
    if (velocidade_ventilacao > 30.0) { // Zona Morta 30%
        fanOnDuration = (CONTROL_CYCLE_MS * (velocidade_ventilacao / 100.0));
    }

    // --- 3. Iluminação ---
    unsigned long lightOnDuration = (CONTROL_CYCLE_MS * (nivel_iluminacao / 100.0));

    // --- EXECUÇÃO FÍSICA ---

    // Bomba
    if (elapsedTime < pumpOnDuration) {
        digitalWrite(PUMP_RELAY_PIN, HIGH);
    } else {
        digitalWrite(PUMP_RELAY_PIN, LOW);
    }

    // Ventoinha
    if (elapsedTime < fanOnDuration) {
        digitalWrite(FAN_RELAY_PIN, HIGH);
    } else {
        digitalWrite(FAN_RELAY_PIN, LOW);
    }

    // Iluminação
    if (elapsedTime < lightOnDuration) {
        digitalWrite(LIGHT_RELAY_PIN, HIGH);
    } else {
        digitalWrite(LIGHT_RELAY_PIN, LOW);
    }

    /* * ATUADORES EXTRAS (COMENTADOS)
     * O Backend Fuzzy não envia comandos para Aquecedor ou Umidificador.
     * Eles ficam desligados por segurança.
     */
    digitalWrite(HEATER_RELAY_PIN, LOW);
    digitalWrite(HUMIDIFIER_RELAY_PIN, LOW);
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Configura Relés Ativos
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  
  // Configura Relés Extras (Mantidos desligados)
  pinMode(HEATER_RELAY_PIN, OUTPUT);
  pinMode(HUMIDIFIER_RELAY_PIN, OUTPUT);
  
  // Estado Inicial: Tudo Desligado
  digitalWrite(PUMP_RELAY_PIN, LOW);
  digitalWrite(FAN_RELAY_PIN, LOW);
  digitalWrite(LIGHT_RELAY_PIN, LOW);
  digitalWrite(HEATER_RELAY_PIN, LOW);
  digitalWrite(HUMIDIFIER_RELAY_PIN, LOW);
  
  // Sensores
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  // SOIL_PIN é input analógico automático
  
  dht.begin();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  
  cycleStartTime = millis();
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    temperatura = lerTemperatura();
    umidade_ar = lerUmidadeAr();
    umidade_solo = lerUmidadeSolo(); 
    distancia_agua = lerDistanciaAgua(); // Lê mas não usa
    
    Serial.printf("T:%.1f | UmidAr:%.1f | Solo:%.1f | NivelAgua:%.1f\n", 
                  temperatura, umidade_ar, umidade_solo, distancia_agua);
    lastSensorRead = currentTime;
  }
  
  if (currentTime - lastApiCall >= API_INTERVAL) {
    enviarDadosParaAPI();
    lastApiCall = currentTime;
  }
  
  gerenciarAtuadores(currentTime);
  delay(10);
}