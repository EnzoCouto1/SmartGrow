/*
 * SmartGrow ESP32 - Código para Estufa Inteligente
 * Envia dados dos sensores para API FastAPI
 * 
 * Pinagem conforme diagrama:
 * - GPIO34: Sensor de umidade do solo (AOUT)
 * - GPIO23: Sensor DHT22 (DATA) - temperatura e umidade do ar
 * - GPIO5: Sensor ultrassônico (TRIG)
 * - GPIO4: Sensor ultrassônico (ECHO)
 * - GPIO26: Relé bomba de água
 * - GPIO27: Relé iluminação
 * - GPIO14: Relé exaustor/ventilação
 * - GPIO12: Relé aquecedor
 * - GPIO13: Relé umidificador
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// =============================================================================
// CONFIGURAÇÕES DE REDE
// =============================================================================
const char* ssid = "SEU_WIFI_SSID";           // Nome da sua rede WiFi
const char* password = "SUA_SENHA_WIFI";      // Senha da sua rede WiFi
const char* api_url = "http://SEU_IP:8000";   // IP do computador com a API

// =============================================================================
// CONFIGURAÇÕES DOS SENSORES
// =============================================================================
#define DHT_PIN 23                    // GPIO23 - DHT22
#define DHT_TYPE DHT22                // Tipo do sensor
#define SOIL_MOISTURE_PIN 34          // GPIO34 - Sensor umidade solo
#define ULTRASONIC_TRIG_PIN 5         // GPIO5 - Sensor ultrassônico TRIG
#define ULTRASONIC_ECHO_PIN 4         // GPIO4 - Sensor ultrassônico ECHO

// =============================================================================
// CONFIGURAÇÕES DOS ATUADORES (RELÉS)
// =============================================================================
#define PUMP_RELAY_PIN 26             // GPIO26 - Bomba de água
#define LIGHT_RELAY_PIN 27            // GPIO27 - Iluminação
#define FAN_RELAY_PIN 14              // GPIO14 - Exaustor/ventilação
#define HEATER_RELAY_PIN 12           // GPIO12 - Aquecedor
#define HUMIDIFIER_RELAY_PIN 13       // GPIO13 - Umidificador

// =============================================================================
// VARIÁVEIS GLOBAIS
// =============================================================================
DHT dht(DHT_PIN, DHT_TYPE);
unsigned long lastSensorRead = 0;
unsigned long lastApiCall = 0;
const unsigned long SENSOR_INTERVAL = 5000;    // Lê sensores a cada 5 segundos
const unsigned long API_INTERVAL = 30000;      // Envia para API a cada 30 segundos

// Variáveis para armazenar leituras
float temperatura = 0.0;
float umidade_ar = 0.0;
float umidade_solo = 0.0;
float distancia_agua = 0.0;

// Estado dos atuadores (recebido da API)
float nivel_irrigacao = 0.0;
float velocidade_ventilacao = 0.0;

// =============================================================================
// FUNÇÕES DE LEITURA DOS SENSORES
// =============================================================================

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
  // Lê o valor analógico (0-4095 para ESP32)
  int valor_analogico = analogRead(SOIL_MOISTURE_PIN);
  
  // Converte para porcentagem (0-100%)
  // Valores típicos: 0-3000 = seco, 3000-4095 = úmido
  float umidade = map(valor_analogico, 0, 4095, 100, 0);
  
  // Limita entre 0 e 100
  umidade = constrain(umidade, 0, 100);
  
  return umidade;
}

float lerDistanciaAgua() {
  // Limpa o pino TRIG
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  // Envia pulso de 10us no TRIG
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  
  // Lê o tempo de resposta do ECHO
  long duracao = pulseIn(ULTRASONIC_ECHO_PIN, HIGH);
  
  // Calcula a distância (velocidade do som = 343 m/s)
  float distancia = (duracao * 0.0343) / 2;
  
  return distancia;
}

// =============================================================================
// FUNÇÕES DE COMUNICAÇÃO COM API
// =============================================================================

void enviarDadosParaAPI() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado!");
    return;
  }
  
  HTTPClient http;
  http.begin(String(api_url) + "/leituras");
  http.addHeader("Content-Type", "application/json");
  
  // Cria o JSON com os dados dos sensores
  DynamicJsonDocument doc(1024);
  doc["temperatura_celsius"] = temperatura;
  doc["umidade_solo"] = umidade_solo;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("Enviando dados para API:");
  Serial.println(jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Resposta da API: " + response);
    
    // Parse da resposta para obter comandos dos atuadores
    DynamicJsonDocument responseDoc(1024);
    deserializeJson(responseDoc, response);
    
    if (responseDoc.containsKey("estado_atual")) {
      nivel_irrigacao = responseDoc["estado_atual"]["nivel_irrigacao"];
      velocidade_ventilacao = responseDoc["estado_atual"]["velocidade_ventilacao"];
      
      Serial.println("Comandos recebidos:");
      Serial.println("  Irrigação: " + String(nivel_irrigacao) + "%");
      Serial.println("  Ventilação: " + String(velocidade_ventilacao) + "%");
      
      // Atualiza os atuadores
      controlarAtuadores();
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
    
    controlarAtuadores();
  }
  
  http.end();
}

// =============================================================================
// FUNÇÕES DE CONTROLE DOS ATUADORES
// =============================================================================

void controlarAtuadores() {
  // Controla bomba de água (irrigação)
  if (nivel_irrigacao > 50) {
    digitalWrite(PUMP_RELAY_PIN, HIGH);  // Liga bomba
    Serial.println("Bomba de água: LIGADA");
  } else {
    digitalWrite(PUMP_RELAY_PIN, LOW);   // Desliga bomba
    Serial.println("Bomba de água: DESLIGADA");
  }
  
  // Controla ventilação/exaustor
  if (velocidade_ventilacao > 50) {
    digitalWrite(FAN_RELAY_PIN, HIGH);   // Liga ventilação
    Serial.println("Ventilação: LIGADA");
  } else {
    digitalWrite(FAN_RELAY_PIN, LOW);    // Desliga ventilação
    Serial.println("Ventilação: DESLIGADA");
  }
  
  // Controle adicional baseado em temperatura
  if (temperatura > 30) {
    digitalWrite(HEATER_RELAY_PIN, LOW);     // Desliga aquecedor
    digitalWrite(FAN_RELAY_PIN, HIGH);       // Liga ventilação
  } else if (temperatura < 15) {
    digitalWrite(HEATER_RELAY_PIN, HIGH);    // Liga aquecedor
    digitalWrite(FAN_RELAY_PIN, LOW);        // Desliga ventilação
  }
  
  // Controle de umidade do ar
  if (umidade_ar < 40) {
    digitalWrite(HUMIDIFIER_RELAY_PIN, HIGH); // Liga umidificador
  } else if (umidade_ar > 70) {
    digitalWrite(HUMIDIFIER_RELAY_PIN, LOW);  // Desliga umidificador
  }
}

// =============================================================================
// CONFIGURAÇÃO INICIAL
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("🌱 SmartGrow ESP32 - Iniciando...");
  
  // Configura pinos dos relés como saída
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);
  pinMode(HEATER_RELAY_PIN, OUTPUT);
  pinMode(HUMIDIFIER_RELAY_PIN, OUTPUT);
  
  // Configura pinos do sensor ultrassônico
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  
  // Inicializa todos os relés desligados
  digitalWrite(PUMP_RELAY_PIN, LOW);
  digitalWrite(LIGHT_RELAY_PIN, LOW);
  digitalWrite(FAN_RELAY_PIN, LOW);
  digitalWrite(HEATER_RELAY_PIN, LOW);
  digitalWrite(HUMIDIFIER_RELAY_PIN, LOW);
  
  // Inicializa sensor DHT22
  dht.begin();
  
  // Conecta ao WiFi
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
  
  // Teste inicial dos sensores
  Serial.println("\n🔍 Testando sensores...");
  temperatura = lerTemperatura();
  umidade_ar = lerUmidadeAr();
  umidade_solo = lerUmidadeSolo();
  distancia_agua = lerDistanciaAgua();
  
  Serial.println("Leituras iniciais:");
  Serial.println("  Temperatura: " + String(temperatura) + "°C");
  Serial.println("  Umidade do ar: " + String(umidade_ar) + "%");
  Serial.println("  Umidade do solo: " + String(umidade_solo) + "%");
  Serial.println("  Distância da água: " + String(distancia_agua) + " cm");
  
  Serial.println("\n✅ Sistema iniciado com sucesso!");
}

// =============================================================================
// LOOP PRINCIPAL
// =============================================================================

void loop() {
  unsigned long currentTime = millis();
  
  // Lê sensores a cada intervalo definido
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    temperatura = lerTemperatura();
    umidade_ar = lerUmidadeAr();
    umidade_solo = lerUmidadeSolo();
    distancia_agua = lerDistanciaAgua();
    
    Serial.println("\n📊 Leituras dos sensores:");
    Serial.println("  Temperatura: " + String(temperatura) + "°C");
    Serial.println("  Umidade do ar: " + String(umidade_ar) + "%");
    Serial.println("  Umidade do solo: " + String(umidade_solo) + "%");
    Serial.println("  Distância da água: " + String(distancia_agua) + " cm");
    
    lastSensorRead = currentTime;
  }
  
  // Envia dados para API a cada intervalo definido
  if (currentTime - lastApiCall >= API_INTERVAL) {
    enviarDadosParaAPI();
    lastApiCall = currentTime;
  }
  
  // Verifica status do sistema periodicamente
  if (currentTime % 60000 == 0) { // A cada 1 minuto
    obterStatusSistema();
  }
  
  delay(100); // Pequena pausa para estabilidade
}
