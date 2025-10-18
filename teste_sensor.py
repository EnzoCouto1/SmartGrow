import requests
import json
import time

# URL da API
url = "http://localhost:8000/leituras"

# Dados de teste - diferentes cenários
cenarios_teste = [
    {"nome": "Cenário 1: Quente e Seco", "temperatura_celsius": 35.0, "umidade_solo": 25.0},
    {"nome": "Cenário 2: Frio e Úmido", "temperatura_celsius": 15.0, "umidade_solo": 80.0},
    {"nome": "Cenário 3: Ideal", "temperatura_celsius": 22.0, "umidade_solo": 50.0},
    {"nome": "Cenário 4: Muito Quente", "temperatura_celsius": 45.0, "umidade_solo": 30.0},
    {"nome": "Cenário 5: Muito Frio", "temperatura_celsius": 5.0, "umidade_solo": 60.0}
]

def enviar_leitura(temperatura, umidade):
    """Envia uma leitura de sensor para a API"""
    dados = {
        "temperatura_celsius": temperatura,
        "umidade_solo": umidade
    }
    
    try:
        response = requests.post(url, json=dados)
        if response.status_code == 200:
            resultado = response.json()
            print(f"✅ Sucesso!")
            print(f"   Irrigação: {resultado['estado_atual']['nivel_irrigacao']:.1f}%")
            print(f"   Ventilação: {resultado['estado_atual']['velocidade_ventilacao']:.1f}%")
            return resultado
        else:
            print(f"❌ Erro: {response.status_code}")
            print(f"   {response.text}")
    except Exception as e:
        print(f"❌ Erro de conexão: {e}")

def verificar_status():
    """Verifica o status atual do sistema"""
    try:
        response = requests.get("http://localhost:8000/status_sistema")
        if response.status_code == 200:
            status = response.json()
            print(f"📊 Status Atual:")
            print(f"   Irrigação: {status['nivel_irrigacao']:.1f}%")
            print(f"   Ventilação: {status['velocidade_ventilacao']:.1f}%")
        else:
            print(f"❌ Erro ao verificar status: {response.status_code}")
    except Exception as e:
        print(f"❌ Erro de conexão: {e}")

if __name__ == "__main__":
    print("🌱 Testador de Sensores - SmartGrow")
    print("=" * 50)
    
    while True:
        print("\nEscolha uma opção:")
        print("1. Testar cenários automáticos")
        print("2. Inserir dados manualmente")
        print("3. Verificar status do sistema")
        print("4. Ver histórico de leituras")
        print("0. Sair")
        
        opcao = input("\nDigite sua opção: ").strip()
        
        if opcao == "1":
            print("\n🧪 Testando cenários automáticos...")
            for cenario in cenarios_teste:
                print(f"\n{cenario['nome']}")
                print(f"   Temp: {cenario['temperatura_celsius']}°C, Umidade: {cenario['umidade_solo']}%")
                enviar_leitura(cenario['temperatura_celsius'], cenario['umidade_solo'])
                time.sleep(1)
                
        elif opcao == "2":
            try:
                temp = float(input("Digite a temperatura (°C): "))
                umidade = float(input("Digite a umidade do solo (%): "))
                print(f"\n📡 Enviando leitura: {temp}°C, {umidade}%")
                enviar_leitura(temp, umidade)
            except ValueError:
                print("❌ Por favor, digite números válidos!")
                
        elif opcao == "3":
            verificar_status()
            
        elif opcao == "4":
            try:
                response = requests.get("http://localhost:8000/leituras")
                if response.status_code == 200:
                    leituras = response.json()
                    print(f"\n📋 Histórico de Leituras ({len(leituras)} registros):")
                    for leitura in leituras[:5]:  # Mostra apenas as 5 mais recentes
                        print(f"   ID: {leitura['id']} | Temp: {leitura['temperatura']}°C | Umidade: {leitura['umidade']}% | Hora: {leitura['horario']}")
                else:
                    print(f"❌ Erro ao buscar histórico: {response.status_code}")
            except Exception as e:
                print(f"❌ Erro de conexão: {e}")
                
        elif opcao == "0":
            print("👋 Saindo...")
            break
            
        else:
            print("❌ Opção inválida!")
