import numpy as np
import skfuzzy as fuzz
from skfuzzy import control as ctrl

# --- Variáveis de Entrada (usadas por ambos os sistemas) ---
temperatura = ctrl.Antecedent(np.arange(0, 51, 1), 'temperatura')
umidade = ctrl.Antecedent(np.arange(0, 101, 1), 'umidade')

# --- Funções de Pertinência para as Entradas ---
temperatura['fria'] = fuzz.trimf(temperatura.universe, [0, 0, 20])
temperatura['agradavel'] = fuzz.trimf(temperatura.universe, [15, 22, 30])
temperatura['quente'] = fuzz.trimf(temperatura.universe, [25, 50, 50])

umidade['seca'] = fuzz.trimf(umidade.universe, [0, 0, 40])
umidade['ideal'] = fuzz.trimf(umidade.universe, [30, 50, 70])
umidade['umida'] = fuzz.trimf(umidade.universe, [60, 100, 100])

# ==============================================================================
# --- SISTEMA DE CONTROLE DE IRRIGAÇÃO ---
# ==============================================================================
irrigacao = ctrl.Consequent(np.arange(0, 101, 1), 'irrigacao')

irrigacao['baixa'] = fuzz.trimf(irrigacao.universe, [0, 0, 50])
irrigacao['media'] = fuzz.trimf(irrigacao.universe, [25, 50, 75])
irrigacao['alta'] = fuzz.trimf(irrigacao.universe, [50, 100, 100])

regra_irr_1 = ctrl.Rule(temperatura['quente'] & umidade['seca'], irrigacao['alta'])
regra_irr_2 = ctrl.Rule(temperatura['agradavel'] & umidade['seca'], irrigacao['media'])
regra_irr_3 = ctrl.Rule(umidade['ideal'], irrigacao['baixa'])
regra_irr_4 = ctrl.Rule(umidade['umida'], irrigacao['baixa'])

sistema_controle_irrigacao = ctrl.ControlSystem([regra_irr_1, regra_irr_2, regra_irr_3, regra_irr_4])
simulador_irrigacao = ctrl.ControlSystemSimulation(sistema_controle_irrigacao)

def calcular_nivel_irrigacao(temp_atual, umidade_atual):
    simulador_irrigacao.input['temperatura'] = temp_atual
    simulador_irrigacao.input['umidade'] = umidade_atual
    simulador_irrigacao.compute()
    return simulador_irrigacao.output['irrigacao']

# ==============================================================================
# --- NOVO: SISTEMA DE CONTROLE DE VENTILAÇÃO ---
# ==============================================================================
ventilacao = ctrl.Consequent(np.arange(0, 101, 1), 'ventilacao') # 0 a 100% de velocidade

ventilacao['baixa'] = fuzz.trimf(ventilacao.universe, [0, 0, 50])
ventilacao['media'] = fuzz.trimf(ventilacao.universe, [25, 50, 75])
ventilacao['alta'] = fuzz.trimf(ventilacao.universe, [50, 100, 100])

# Regras simples baseadas apenas na temperatura
regra_ven_1 = ctrl.Rule(temperatura['fria'], ventilacao['baixa']) # Frio, não ventila
regra_ven_2 = ctrl.Rule(temperatura['agradavel'], ventilacao['media'])
regra_ven_3 = ctrl.Rule(temperatura['quente'], ventilacao['alta'])

sistema_controle_ventilacao = ctrl.ControlSystem([regra_ven_1, regra_ven_2, regra_ven_3])
simulador_ventilacao = ctrl.ControlSystemSimulation(sistema_controle_ventilacao)

def calcular_velocidade_ventilacao(temp_atual):
    simulador_ventilacao.input['temperatura'] = temp_atual
    simulador_ventilacao.compute()
    return simulador_ventilacao.output['ventilacao']

# --- Testes (se executar este arquivo diretamente) ---
if __name__ == '__main__':
    temp_teste = 35
    umidade_teste = 25
    nivel_irr = calcular_nivel_irrigacao(temp_teste, umidade_teste)
    veloc_ven = calcular_velocidade_ventilacao(temp_teste)
    print(f"--- Cenário: Temp={temp_teste}, Umidade={umidade_teste} ---")
    print(f"  -> Nível de Irrigação: {nivel_irr:.2f}%")
    print(f"  -> Velocidade da Ventilação: {veloc_ven:.2f}%")