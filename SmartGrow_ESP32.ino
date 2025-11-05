/*
 * SmartGrow ESP32 - Código para Estufa Inteligente v1.1
 * Envia dados dos sensores para API FastAPI com Lógica Fuzzy
 * * --- ATUALIZADO ---
 * - Adicionado Sensor de Luminosidade (LDR)
 * - Envia 3 valores (temp, umidade, lum) para a API
 * - Recebe e atua em 3 comandos (irrigação, ventilação, iluminação)
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
#define LDR_PIN 32                    // --- NOVO --- GPIO32 (Pino ADC1)

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
const unsigned long SENSOR_INTERVAL = 5000; // Lê sensores a cada 5 segundos
const unsigned long API_INTERVAL = 30000;   // Envia para API a cada 30 segundos

// Variáveis para armazenar leituras
float temperatura = 0.0;
float umidade_ar = 0.0;
float umidade_solo = 0.0;
float distancia_agua = 0.0;
float luminosidade = 0.0;           // --- NOVO ---

// Estado dos atuadores (recebido da API)
float nivel_irrigacao = 0.0;
float velocidade_ventilacao = 0.0;
float nivel_iluminacao = 0.0;       // --- NOVO ---

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
  int valor_analogico = analogRead(SOIL_MOISTURE_PIN);
  float umidade = map(valor_analogico, 0, 4095, 100, 0); // Invertido: 0 (seco) -> 100%
  umidade = constrain(umidade, 0, 100);
  return umidade;
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

// --- NOVA FUNÇÃO ---
float lerLuminosidade() {
  // Lê o LDR (0-4095). 
  // Assumimos que 4095 é escuro total e 0 é luz máxima (depende do seu circuito LDR)
  int valor_analogico_ldr = analogRead(LDR_PIN);
  // Converte para porcentagem (0% escuro, 100% claro)
  float lum_percent = map(valor_analogico_ldr, 4095, 0, 0, 100);
  lum_percent = constrain(lum_percent, 0, 100);
  return lum_percent;
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

  // --- ATUALIZADO --- Cria o JSON com os 3 dados
  DynamicJsonDocument doc(1024);
  doc["temperatura_celsius"] = temperatura;
  doc["umidade_solo"] = umidade_solo;
  doc["luminosidade"] = luminosidade; // <-- DADO ADICIONADO
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("Enviando dados para API:");
  Serial.println(jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Resposta da API: " + response);

    // --- ATUALIZADO --- Parse da resposta para obter os 3 comandos
    DynamicJsonDocument responseDoc(1024);
    deserializeJson(responseDoc, response);
    
    if (responseDoc.containsKey("estado_atual")) {
      nivel_irrigacao = responseDoc["estado_atual"]["nivel_irrigacao"];
      velocidade_ventilacao = responseDoc["estado_atual"]["velocidade_ventilacao"];
      nivel_iluminacao = responseDoc["estado_atual"]["nivel_iluminacao"]; // <-- COMANDO ADICIONADO
      
      Serial.println("Comandos recebidos:");
      Serial.println("  Irrigação: " + String(nivel_irrigacao) + "%");
      Serial.println("  Ventilação: " + String(velocidade_ventilacao) + "%");
      Serial.println("  Iluminação: " + String(nivel_iluminacao) + "%"); // <-- LOG ADICIONADO

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
    
    // --- ATUALIZADO --- Obtém os 3 comandos
    nivel_irrigacao = doc["nivel_irrigacao"];
    velocidade_ventilacao = doc["velocidade_ventilacao"];
    nivel_iluminacao = doc["nivel_iluminacao"]; // <-- COMANDO ADICIONADO
    
    controlarAtuadores();
  }
  
  http.end();
}

// =============================================================================
// FUNÇÕES DE CONTROLE DOS ATUADORES
// =============================================================================

void controlarAtuadores() {
  
  // Controla bomba de água (irrigação)
  // Lógica de exemplo: liga o relé se a API disser mais de 50%
  if (nivel_irrigacao > 50.0) {
    digitalWrite(PUMP_RELAY_PIN, HIGH); // Liga bomba
    Serial.println("Bomba de água: LIGADA");
  } else {
    digitalWrite(PUMP_RELAY_PIN, LOW); // Desliga bomba
    Serial.println("Bomba de água: DESLIGADA");
  }
  
  // Controla ventilação/exaustor
  if (velocidade_ventilacao > 50.0) {
    digitalWrite(FAN_RELAY_PIN, HIGH); // Liga ventilação
    Serial.println("Ventilação: LIGADA");
  } else {
    digitalWrite(FAN_RELAY_PIN, LOW); // Desliga ventilação
    Serial.println("Ventilação: DESLIGADA");
  }
  
  // --- NOVA LÓGICA DE ILUMINAÇÃO ---
  if (nivel_iluminacao > 50.0) {
    digitalWrite(LIGHT_RELAY_PIN, HIGH); // Liga luz
    Serial.println("Iluminação: LIGADA");
  } else {
    digitalWrite(LIGHT_RELAY_PIN, LOW); // Desliga luz
    Serial.println("Iluminação: DESLIGADA");
  }

  /* * NOTA: A lógica original de controle de Aquecedor e Umidificador  foi removida
   * pois ela era local do ESP32 e não usava a API Fuzzy. 
   * O backend atualmente não controla "aquecedor" ou "umidificador".
   * Se precisar deles, teríamos que adicionar ao backend.
   */
}

// =============================================================================
// CONFIGURAÇÃO INICIAL
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("🌱 SmartGrow ESP32 v1.1 - Iniciando...");
  
  // Configura pinos dos relés como saída
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);
  pinMode(HEATER_RELAY_PIN, OUTPUT);
  pinMode(HUMIDIFIER_RELAY_PIN, OUTPUT);
  
  // Configura pinos do sensor ultrassônico
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);

  // Configura pino do LDR como entrada (embora analogRead defina automaticamente)
  pinMode(LDR_PIN, INPUT); 
  
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
  luminosidade = lerLuminosidade(); // <-- NOVO
  
  Serial.println("Leituras iniciais:");
  Serial.println("  Temperatura: " + String(temperatura) + "°C");
  Serial.println("  Umidade do ar: " + String(umidade_ar) + "%");
  Serial.println("  Umidade do solo: " + String(umidade_solo) + "%");
  Serial.println("  Luminosidade: " + String(luminosidade) + "%"); // <-- NOVO
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
    luminosidade = lerLuminosidade(); // <-- NOVO
    
    Serial.println("\n📊 Leituras dos sensores:");
    Serial.println("  Temperatura: " + String(temperatura) + "°C");
    Serial.println("  Umidade do ar: " + String(umidade_ar) + "%");
    Serial.println("  Umidade do solo: " + String(umidade_solo) + "%");
    Serial.println("  Luminosidade: " + String(luminosidade) + "%"); // <-- NOVO
    Serial.println("  Distância da água: " + String(distancia_agua) + " cm");
    
    lastSensorRead = currentTime;
  }
  
  // Envia dados para API a cada intervalo definido
  if (currentTime - lastApiCall >= API_INTERVAL) {
    enviarDadosParaAPI();
    lastApiCall = currentTime;
  }
  
  /* * A função obterStatusSistema() foi removida do loop 
   * para evitar chamadas duplicadas, já que a função 
   * enviarDadosParaAPI() já obtém o status mais recente 
   * como resposta ao POST.
   */
   
  delay(100); // Pequena pausa para estabilidade
}