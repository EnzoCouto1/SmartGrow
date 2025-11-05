import requests
import json
import time

# URL da API
url = "http://localhost:8000/leituras"

# Cenários de teste atualizados com LUMINOSIDADE
cenarios_teste = [
    {"nome": "Cenário 1: Quente, Seco e Escuro", "temp": 35.0, "umidade": 20.0, "lum": 10.0},
    {"nome": "Cenário 2: Frio, Úmido e Claro", "temp": 15.0, "umidade": 80.0, "lum": 90.0},
    {"nome": "Cenário 3: Ideal (Dia)", "temp": 22.0, "umidade": 50.0, "lum": 80.0},
    {"nome": "Cenário 4: Tarde Nublada", "temp": 25.0, "umidade": 55.0, "lum": 35.0}
]

def enviar_leitura(temperatura, umidade, luminosidade):
    """Envia uma leitura de sensor para a API"""
    dados = {
        "temperatura_celsius": temperatura,
        "umidade_solo": umidade,
        "luminosidade": luminosidade  
    }
    
    try:
        response = requests.post(url, json=dados)
        if response.status_code == 200:
            resultado = response.json()
            print(f"✅ Sucesso!")
            print(f"   Irrigação: {resultado['estado_atual']['nivel_irrigacao']:.1f}%")
            print(f"   Ventilação: {resultado['estado_atual']['velocidade_ventilacao']:.1f}%")
            print(f"   Iluminação: {resultado['estado_atual']['nivel_iluminacao']:.1f}%") 
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
            print(f"   Iluminação: {status['nivel_iluminacao']:.1f}%") 
        else:
            print(f"❌ Erro ao verificar status: {response.status_code}")
    except Exception as e:
        print(f"❌ Erro de conexão: {e}")

if __name__ == "__main__":
    print("🌱 Testador de Sensores - SmartGrow v1.1 (com Luminosidade)")
    print("=" * 60)
    
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
                print(f"   Temp: {cenario['temp']}°C, Umidade: {cenario['umidade']}%, Lum: {cenario['lum']}%")
                enviar_leitura(cenario['temp'], cenario['umidade'], cenario['lum'])
                time.sleep(1)
                
        elif opcao == "2":
            try:
                temp = float(input("Digite a temperatura (°C): "))
                umidade = float(input("Digite a umidade do solo (%): "))
                lum = float(input("Digite a luminosidade (%): "))
                print(f"\n📡 Enviando leitura: {temp}°C, {umidade}%, {lum}%")
                enviar_leitura(temp, umidade, lum)
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
                    # Mostra apenas as 5 mais recentes
                    for leitura in leituras[:5]:
                        print(f"   ID: {leitura['id']} | T: {leitura['temperatura']} | U: {leitura['umidade']} | L: {leitura.get('luminosidade', 'N/A')} | Hora: {leitura['horario']}")
                else:
                    print(f"❌ Erro ao buscar histórico: {response.status_code}")
            except Exception as e:
                print(f"❌ Erro de conexão: {e}")
                
        elif opcao == "0":
            print("👋 Saindo...")
            break
            
        else:
            print("❌ Opção inválida!")