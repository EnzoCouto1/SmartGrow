/*
 * Teste de BOMBA D'ÁGUA (Motor) no Pino 25
 * Lógica: 5s LIGADA / 5s DESLIGADA
 *
 * VERSÃO CORRIGIDA: Para relés "Active LOW" (Ativo Baixo)
 * A maioria dos módulos relé ativa (liga) com um sinal BAIXO (LOW / GND)
 * e desliga com um sinal ALTO (HIGH / 3.3V).
 *
 * CONEXÃO (Exemplo com MÓDULO RELÉ de 1 Canal):
 *
 * Lado do ESP32 (Controle):
 * 1. Pino 5V (ou VIN) do ESP32 -> VCC do Módulo Relé
 * 2. Pino GND do ESP32          -> GND do Módulo Relé
 * 3. Pino 25 (GPIO 25)         -> IN (Sinal) do Módulo Relé  <--- MUDANÇA AQUI

 */

// Define o pino que vai controlar o Relé
const int pinoRele = 27; // <--- MUDANÇA AQUI

void setup() {
  // Inicia a comunicação serial (para vermos mensagens no Monitor Serial)
  Serial.begin(115200); 
  
  // Configura o pino 25 como uma SAÍDA (OUTPUT)
  pinMode(pinoRele, OUTPUT);
  
  // Para relés Active LOW, precisamos começar com o pino em ALTO (HIGH)
  // para garantir que o relé comece DESLIGADO.
  digitalWrite(pinoRele, HIGH); 
  
  Serial.println("Iniciando teste de BOMBA D'ÁGUA (Lógica Invertida) no Pino 25");
  Serial.println("Ciclo: 5s LIGADA / 5s DESLIGADA");
}

void loop() {
  // Aciona o relé (que liga a bomba) enviando um sinal BAIXO (LOW)
  digitalWrite(pinoRele, LOW); 
  Serial.println("Pino 25 - BAIXO (LOW) -> Bomba LIGADA"); // <--- MUDANÇA AQUI
  delay(5000); // Espera 5 segundos (5000 milissegundos)
  
  // Desliga o relé (que desliga a bomba) enviando um sinal ALTO (HIGH)
  digitalWrite(pinoRele, HIGH); 
  Serial.println("Pino 25 - ALTO (HIGH) -> Bomba DESLIGADA"); // <--- MUDANÇA AQUI
  delay(5000); // Espera 5 segundos
}

void loop() {
  
}