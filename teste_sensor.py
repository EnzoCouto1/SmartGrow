import requests
import datetime

url = "https://smartgrow-ajtn.onrender.com/leituras" 

def testar_cenario(nome, temp, umid_media):
    print(f"\n🧪 TESTE: {nome}")
    print(f"   Simulando média dos 3 sensores: {umid_media}%")
    
    dados = {
        "temperatura_celsius": temp,
        "umidade_solo": umid_media 
    }
    
    try:
        # Envia para a nuvem
        response = requests.post(url, json=dados)
        
        if response.status_code == 200:
            resultado = response.json()
            estado = resultado['estado_atual']
            
            
            print("✅ RESPOSTA DA NUVEM:")
            print(f"   --> Irrigação: {estado['nivel_irrigacao']:.1f}%")
            print(f"   --> Ventilação: {estado['velocidade_ventilacao']:.1f}%")
            print(f"   --> Iluminação: {estado['nivel_iluminacao']:.1f}% (Time-Based)")
            
            # Análise Rápida
            if estado['nivel_irrigacao'] > 30:
                print("   💧 AÇÃO ESP32: LIGAR BOMBA (Proporcional)")
            else:
                print("   🛑 AÇÃO ESP32: MANTER DESLIGADO (Zona Morta)")
                
        else:
            print(f"❌ Erro {response.status_code}: {response.text}")
            
    except Exception as e:
        print(f"❌ Erro de conexão: {e}")

if __name__ == "__main__":
    print("🌍 TESTE DE INTEGRAÇÃO REMOTA - SMARTGROW")
    print("   Conectando a: " + url)
    
    # Cenário 1: Solo Seco (Média dos 3 sensores deu baixa)
    testar_cenario("Solo Seco e Quente", temp=30.0, umid_media=20.0)
    
    # Cenário 2: Solo Ideal (Média dos 3 sensores deu boa)
    testar_cenario("Solo Ideal e Agradável", temp=22.0, umid_media=50.0)