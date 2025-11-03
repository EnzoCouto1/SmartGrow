# fuzzy_logic.py
import numpy as np
import skfuzzy as fuzz
from skfuzzy import control as ctrl

# --- 1. Variáveis de Entrada (usadas por todos os sistemas) ---
temperatura = ctrl.Antecedent(np.arange(0, 51, 1), 'temperatura')
umidade = ctrl.Antecedent(np.arange(0, 101, 1), 'umidade')
# --- NOVO: Variável de Entrada de Luz ---
# Vamos assumir que o sensor envia um valor de 0 (escuro total) a 100 (luz total)
luminosidade = ctrl.Antecedent(np.arange(0, 101, 1), 'luminosidade')

# --- 2. Funções de Pertinência para as Entradas ---
# Temperatura
temperatura['fria'] = fuzz.trimf(temperatura.universe, [0, 0, 20])
temperatura['agradavel'] = fuzz.trimf(temperatura.universe, [15, 22, 30])
temperatura['quente'] = fuzz.trimf(temperatura.universe, [25, 50, 50])

# Umidade
umidade['seca'] = fuzz.trimf(umidade.universe, [0, 0, 40])
umidade['ideal'] = fuzz.trimf(umidade.universe, [30, 50, 70])
umidade['umida'] = fuzz.trimf(umidade.universe, [60, 100, 100])

# --- NOVO: Funções de Pertinência da Luminosidade ---
luminosidade['escuro'] = fuzz.trimf(luminosidade.universe, [0, 0, 30])
luminosidade['meia_luz'] = fuzz.trimf(luminosidade.universe, [20, 40, 60])
luminosidade['claro'] = fuzz.trimf(luminosidade.universe, [50, 100, 100])

# ==============================================================================
# --- SISTEMA DE CONTROLE DE IRRIGAÇÃO (Sem alteração) ---
# ==============================================================================
irrigacao = ctrl.Consequent(np.arange(0, 101, 1), 'irrigacao')
irrigacao['baixa'] = fuzz.trimf(irrigacao.universe, [0, 0, 50])
irrigacao['media'] = fuzz.trimf(irrigacao.universe, [25, 50, 75])
irrigacao['alta'] = fuzz.trimf(irrigacao.universe, [50, 100, 100])
# Regras de Irrigação
regra_irr_1 = ctrl.Rule(temperatura['quente'] & umidade['seca'], irrigacao['alta'])
regra_irr_2 = ctrl.Rule(temperatura['agradavel'] & umidade['seca'], irrigacao['media'])
regra_irr_3 = ctrl.Rule(umidade['ideal'], irrigacao['baixa'])
regra_irr_4 = ctrl.Rule(umidade['umida'], irrigacao['baixa'])
# Sistema de Irrigação
sistema_controle_irrigacao = ctrl.ControlSystem([regra_irr_1, regra_irr_2, regra_irr_3, regra_irr_4])
simulador_irrigacao = ctrl.ControlSystemSimulation(sistema_controle_irrigacao)
def calcular_nivel_irrigacao(temp_atual, umidade_atual):
    simulador_irrigacao.input['temperatura'] = temp_atual
    simulador_irrigacao.input['umidade'] = umidade_atual
    simulador_irrigacao.compute()
    return simulador_irrigacao.output['irrigacao']

# ==============================================================================
# --- SISTEMA DE CONTROLE DE VENTILAÇÃO (Sem alteração) ---
# ==============================================================================
ventilacao = ctrl.Consequent(np.arange(0, 101, 1), 'ventilacao')
ventilacao['baixa'] = fuzz.trimf(ventilacao.universe, [0, 0, 50])
ventilacao['media'] = fuzz.trimf(ventilacao.universe, [25, 50, 75])
ventilacao['alta'] = fuzz.trimf(ventilacao.universe, [50, 100, 100])
# Regras de Ventilação
regra_ven_1 = ctrl.Rule(temperatura['fria'], ventilacao['baixa'])
regra_ven_2 = ctrl.Rule(temperatura['agradavel'], ventilacao['media'])
regra_ven_3 = ctrl.Rule(temperatura['quente'], ventilacao['alta'])
# Sistema de Ventilação
sistema_controle_ventilacao = ctrl.ControlSystem([regra_ven_1, regra_ven_2, regra_ven_3])
simulador_ventilacao = ctrl.ControlSystemSimulation(sistema_controle_ventilacao)
def calcular_velocidade_ventilacao(temp_atual):
    simulador_ventilacao.input['temperatura'] = temp_atual
    simulador_ventilacao.compute()
    return simulador_ventilacao.output['ventilacao']

# ==============================================================================
# --- NOVO: SISTEMA DE CONTROLE DE ILUMINAÇÃO ---
# ==============================================================================
iluminacao = ctrl.Consequent(np.arange(0, 101, 1), 'iluminacao') # 0 a 100% de força da luz

# Esta lógica é invertida: quanto mais luz natural, menor a luz artificial.
iluminacao['desligada'] = fuzz.trimf(iluminacao.universe, [0, 0, 40])
iluminacao['media'] = fuzz.trimf(iluminacao.universe, [30, 50, 70])
iluminacao['alta'] = fuzz.trimf(iluminacao.universe, [60, 100, 100])

# Regras (Lógica Invertida)
regra_ilu_1 = ctrl.Rule(luminosidade['escuro'], iluminacao['alta'])
regra_ilu_2 = ctrl.Rule(luminosidade['meia_luz'], iluminacao['media'])
regra_ilu_3 = ctrl.Rule(luminosidade['claro'], iluminacao['desligada'])

sistema_controle_iluminacao = ctrl.ControlSystem([regra_ilu_1, regra_ilu_2, regra_ilu_3])
simulador_iluminacao = ctrl.ControlSystemSimulation(sistema_controle_iluminacao)

def calcular_nivel_iluminacao(lum_atual):
    simulador_iluminacao.input['luminosidade'] = lum_atual
    simulador_iluminacao.compute()
    return simulador_iluminacao.output['iluminacao']

# --- Testes (se executar este arquivo diretamente) ---
if __name__ == '__main__':
    temp_teste = 35
    umidade_teste = 25
    lum_teste = 10 # Simula um ambiente escuro

    nivel_irr = calcular_nivel_irrigacao(temp_teste, umidade_teste)
    veloc_ven = calcular_velocidade_ventilacao(temp_teste)
    nivel_ilu = calcular_nivel_iluminacao(lum_teste)

    print(f"--- Cenário: Temp={temp_teste}, Umidade={umidade_teste}, Lum={lum_teste} ---")
    print(f"  -> Nível de Irrigação: {nivel_irr:.2f}%")
    print(f"  -> Velocidade da Ventilação: {veloc_ven:.2f}%")
    print(f"  -> Nível de Iluminação: {nivel_ilu:.2f}%")