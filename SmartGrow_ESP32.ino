/*
 * SmartGrow ESP32 - Código para Estufa Inteligente v1.5
 * Lógica de Controlo Proporcional ao Tempo (Ciclo de 2 minutos)
 * Aplica "Zona Morta" de 30% em todos os atuadores.
 * 
 * Pinagem conforme Pinout.pdf:
 * ATUADORES:
 * - GPIO26: Relé Bomba 1 (Irrigação Setorizada)
 * - GPIO14: Relé Bomba 2 (Irrigação Setorizada)
 * - GPIO13: Relé Bomba 3 (Irrigação Setorizada)
 * - GPIO27: Relé Iluminação (Controle Fotoperíodo)
 * - GPIO12: Relé Ventilação (Controle das Fans)
 * 
 * SENSORES:
 * - GPIO34: Sensor Solo 1 (Umidade do Solo)
 * - GPIO35: Sensor Solo 2 (Umidade do Solo)
 * - GPIO33: Sensor Solo 3 (Umidade do Solo)
 * - GPIO23: Sensor DHT22 (Temp. e Umidade Ar)
 * - GPIO05: Ultrassônico TRIG (Nível de Água - Saída)
 * - GPIO19: Ultrassônico ECHO (Nível de Água - Entrada) - CRÍTICO: Usar Divisor de Tensão!
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// Configurações de Rede
const char* ssid = "SEU_WIFI_SSID";
const char* password = "SUA_SENHA_WIFI";
const char* api_url = "http://SEU_IP:8000";

// Configurações dos Sensores
#define DHT_PIN 23
#define DHT_TYPE DHT22
#define SOIL_MOISTURE_PIN_1 34  // Sensor Solo 1
#define SOIL_MOISTURE_PIN_2 35  // Sensor Solo 2
#define SOIL_MOISTURE_PIN_3 33  // Sensor Solo 3
#define ULTRASONIC_TRIG_PIN 5
#define ULTRASONIC_ECHO_PIN 19   // CORRIGIDO: Era GPIO4, agora GPIO19 (requer divisor de tensão!)

// Configurações dos Atuadores (Relés) - Conforme Pinout.pdf
#define PUMP_RELAY_PIN_1 26     // Bomba 1 (Irrigação Setorizada)
#define PUMP_RELAY_PIN_2 14     // Bomba 2 (Irrigação Setorizada)
#define PUMP_RELAY_PIN_3 13     // Bomba 3 (Irrigação Setorizada)
#define LIGHT_RELAY_PIN 27      // Iluminação (Controle Fotoperíodo)
#define FAN_RELAY_PIN 12        // CORRIGIDO: Era GPIO14, agora GPIO12 (Ventilação)

// Configurações do Ciclo de Controlo
const unsigned long CONTROL_CYCLE_MS = 120000; // 2 minutos (120,000 ms)
unsigned long cycleStartTime = 0;
bool pumpState = false;
bool fanState = false;
bool lightState = false;

// Variáveis Globais
DHT dht(DHT_PIN, DHT_TYPE);
unsigned long lastSensorRead = 0;
unsigned long lastApiCall = 0;
const unsigned long SENSOR_INTERVAL = 5000; // 5 segundos
const unsigned long API_INTERVAL = 30000;   // 30 segundos

float temperatura = 0.0;
float umidade_ar = 0.0;
float umidade_solo = 0.0;
float distancia_agua = 0.0;
float luminosidade = 0.0;

float nivel_irrigacao = 0.0;
float velocidade_ventilacao = 0.0;
float nivel_iluminacao = 0.0;

// Funções de Leitura dos Sensores
float lerTemperatura() {
  float temp = dht.readTemperature();
  if (isnan(temp)) {
    Serial.println("Erro ao ler temperatura!");
    return -999;
  }
  return temp;
}

float lerUmidadeAr() {
  float umid = dht.readHumidity();
  if (isnan(umid)) {
    Serial.println("Erro ao ler umidade do ar!");
    return -999;
  }
  return umid;
}

float lerUmidadeSolo() {
  // Lê os 3 sensores de solo e calcula a média
  int valor1 = analogRead(SOIL_MOISTURE_PIN_1);
  int valor2 = analogRead(SOIL_MOISTURE_PIN_2);
  int valor3 = analogRead(SOIL_MOISTURE_PIN_3);
  
  float umidade1 = map(valor1, 0, 4095, 100, 0); // Invertido: 0 (seco) -> 100%
  float umidade2 = map(valor2, 0, 4095, 100, 0);
  float umidade3 = map(valor3, 0, 4095, 100, 0);
  
  float umidade_media = (umidade1 + umidade2 + umidade3) / 3.0;
  umidade_media = constrain(umidade_media, 0, 100);
  return umidade_media;
}

float lerDistanciaAgua() {
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  long duracao = pulseIn(ULTRASONIC_ECHO_PIN, HIGH);
  float distancia = (duracao * 0.0343) / 2;
  return distancia;
}

// Nota: Sensor de luminosidade (LDR) não está no Pinout.pdf
// Se necessário, adicione um sensor de luminosidade em um pino disponível
float lerLuminosidade() {
  // Por enquanto retorna um valor padrão ou 0
  // TODO: Adicionar sensor de luminosidade se necessário
  return 50.0; // Valor padrão temporário
}

// Funções de Comunicação com API
void enviarDadosParaAPI() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado!");
    return;
  }
  
  HTTPClient http;
  http.begin(String(api_url) + "/leituras");
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(1024);
  doc["temperatura_celsius"] = temperatura;
  doc["umidade_solo"] = umidade_solo;
  doc["luminosidade"] = luminosidade;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("Enviando dados para API:");
  Serial.println(jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Resposta da API: " + response);

    DynamicJsonDocument responseDoc(1024);
    deserializeJson(responseDoc, response);
    
    if (responseDoc.containsKey("estado_atual")) {
      nivel_irrigacao = responseDoc["estado_atual"]["nivel_irrigacao"];
      velocidade_ventilacao = responseDoc["estado_atual"]["velocidade_ventilacao"];
      nivel_iluminacao = responseDoc["estado_atual"]["nivel_iluminacao"];
      
      Serial.println("Comandos recebidos e atualizados:");
      Serial.println("  Irrigação: " + String(nivel_irrigacao) + "%");
      Serial.println("  Ventilação: " + String(velocidade_ventilacao) + "%");
      Serial.println("  Iluminação: " + String(nivel_iluminacao) + "%");
    }
  } else {
    Serial.println("Erro na comunicação: " + String(httpResponseCode));
  }
  
  http.end();
}

void obterStatusSistema() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  HTTPClient http;
  http.begin(String(api_url) + "/status_sistema");
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Status do sistema: " + response);
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    
    nivel_irrigacao = doc["nivel_irrigacao"];
    velocidade_ventilacao = doc["velocidade_ventilacao"];
    nivel_iluminacao = doc["nivel_iluminacao"];
  }
  http.end();
}

// Função de Controlo Proporcional ao Tempo
void gerenciarAtuadores(unsigned long currentTime) {
    if (currentTime - cycleStartTime >= CONTROL_CYCLE_MS) {
        cycleStartTime = currentTime; // Reinicia o ciclo
    }

    unsigned long elapsedTimeInCycle = currentTime - cycleStartTime;

    // --- Lógica da Bomba (Com Zona Morta de 30%) ---
    unsigned long pumpOnDuration = 0; // Começa em 0 (desligada)
    if (nivel_irrigacao > 30.0) {
        // Só calcula o tempo de ligação se passar da zona morta
        pumpOnDuration = (CONTROL_CYCLE_MS * (nivel_irrigacao / 100.0));
    }

    // --- Lógica da Ventoinha (Com Zona Morta de 30%) ---
    unsigned long fanOnDuration = 0; // Começa em 0 (desligada)
    if (velocidade_ventilacao > 30.0) {
        // Só calcula o tempo de ligação se passar da zona morta
        fanOnDuration = (CONTROL_CYCLE_MS * (velocidade_ventilacao / 100.0));
    }

    // --- Lógica da Iluminação (Com Zona Morta de 30%) ---
    unsigned long lightOnDuration = 0; // Começa em 0 (desligada)
    if (nivel_iluminacao > 30.0) {
         // Só calcula o tempo de ligação se passar da zona morta
        lightOnDuration = (CONTROL_CYCLE_MS * (nivel_iluminacao / 100.0));
    }

    // --- Controlo da Bomba (3 bombas setorizadas) ---
    // Todas as 3 bombas são controladas pelo mesmo nível de irrigação
    if (elapsedTimeInCycle < pumpOnDuration) {
        if (!pumpState) {
            Serial.println("Decisão de Controlo: LIGANDO Bombas de Irrigação");
            pumpState = true;
        }
        digitalWrite(PUMP_RELAY_PIN_1, HIGH);
        digitalWrite(PUMP_RELAY_PIN_2, HIGH);
        digitalWrite(PUMP_RELAY_PIN_3, HIGH);
    } else {
        if (pumpState) {
            Serial.println("Decisão de Controlo: DESLIGANDO Bombas de Irrigação");
            pumpState = false;
        }
        digitalWrite(PUMP_RELAY_PIN_1, LOW);
        digitalWrite(PUMP_RELAY_PIN_2, LOW);
        digitalWrite(PUMP_RELAY_PIN_3, LOW);
    }

    // --- Controlo da Ventoinha ---
    if (elapsedTimeInCycle < fanOnDuration) {
        if (!fanState) {
            Serial.println("Decisão de Controlo: LIGANDO Ventilação");
            fanState = true;
        }
        digitalWrite(FAN_RELAY_PIN, HIGH);
    } else {
        if (fanState) {
            Serial.println("Decisão de Controlo: DESLIGANDO Ventilação");
            fanState = false;
        }
        digitalWrite(FAN_RELAY_PIN, LOW);
    }

    // --- Controlo da Iluminação ---
    if (elapsedTimeInCycle < lightOnDuration) {
        if (!lightState) {
            Serial.println("Decisão de Controlo: LIGANDO Iluminação");
            lightState = true;
        }
        digitalWrite(LIGHT_RELAY_PIN, HIGH);
    } else {
        if (lightState) {
            Serial.println("Decisão de Controlo: DESLIGANDO Iluminação");
            lightState = false;
        }
        digitalWrite(LIGHT_RELAY_PIN, LOW);
    }
}

// Configuração Inicial (Setup)
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("🌱 SmartGrow ESP32 v1.4 (Ciclo 2 min) - Iniciando...");
  
  // Configuração dos Relés (Atuadores)
  pinMode(PUMP_RELAY_PIN_1, OUTPUT);
  pinMode(PUMP_RELAY_PIN_2, OUTPUT);
  pinMode(PUMP_RELAY_PIN_3, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);
  
  // Configuração dos Sensores
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  // Sensores de solo são analógicos, não precisam de pinMode 
  
  // Inicializa todos os relés em LOW (desligados)
  digitalWrite(PUMP_RELAY_PIN_1, LOW);
  digitalWrite(PUMP_RELAY_PIN_2, LOW);
  digitalWrite(PUMP_RELAY_PIN_3, LOW);
  digitalWrite(LIGHT_RELAY_PIN, LOW);
  digitalWrite(FAN_RELAY_PIN, LOW);
  
  dht.begin();
  
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi conectado!");
  Serial.println("IP: " + WiFi.localIP().toString());
  Serial.println("API URL: " + String(api_url));

  Serial.println("\n🔍 Testando sensores...");
  temperatura = lerTemperatura();
  umidade_ar = lerUmidadeAr();
  umidade_solo = lerUmidadeSolo();
  distancia_agua = lerDistanciaAgua();
  luminosidade = lerLuminosidade();
  
  Serial.println("Leitura inicial:");
  Serial.println("  Temperatura: " + String(temperatura) + "°C");
  Serial.println("  Umidade do ar: " + String(umidade_ar) + "%");
  Serial.println("  Umidade do solo: " + String(umidade_solo) + "%");
  Serial.println("  Luminosidade: " + String(luminosidade) + "%");
  Serial.println("  Distância da água: " + String(distancia_agua) + " cm");
  
  cycleStartTime = millis();
  
  Serial.println("\n✅ Sistema iniciado com sucesso!");
}

// Loop Principal
void loop() {
  unsigned long currentTime = millis();
  
  // Tarefa 1: Ler Sensores
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    temperatura = lerTemperatura();
    umidade_ar = lerUmidadeAr();
    umidade_solo = lerUmidadeSolo();
    distancia_agua = lerDistanciaAgua();
    luminosidade = lerLuminosidade();
    
    Serial.println("\n📊 Leituras dos sensores:");
    Serial.println("  Temperatura: " + String(temperatura) + "°C");
    Serial.println("  Umidade do ar: " + String(umidade_ar) + "%");
    Serial.println("  Umidade do solo: " + String(umidade_solo) + "%");
    Serial.println("  Luminosidade: " + String(luminosidade) + "%");
    Serial.println("  Distância da água: " + String(distancia_agua) + " cm");
    
    lastSensorRead = currentTime;
  }
  
  // Tarefa 2: Falar com a API
  if (currentTime - lastApiCall >= API_INTERVAL) {
    enviarDadosParaAPI(); // Envia dados e recebe novos comandos
    lastApiCall = currentTime;
  }
  
  // Tarefa 3: Controlar os Atuadores
  gerenciarAtuadores(currentTime);
   
  delay(10);
}