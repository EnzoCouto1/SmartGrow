from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from fastapi.middleware.cors import CORSMiddleware
import sqlite3
import datetime

from fuzzy_logic import (
    calcular_nivel_irrigacao, 
    calcular_velocidade_ventilacao,
    calcular_nivel_iluminacao
)

DATABASE_NAME = "leituras.db"

# --- MODELOS DE DADOS ---

class LeituraSensor(BaseModel):
    temperatura_celsius: float
    umidade_solo: float
    luminosidade: float 

class ConfigAutomacao(BaseModel):
    sistema: str # "irrigacao" ou "iluminacao"
    ativo: bool

class ComandoManual(BaseModel):
    sistema: str # "irrigacao" ou "iluminacao"
    ligar: bool

# --- CONFIGURAÇÃO DA API ---

app = FastAPI(
    title="API da Estufa Inteligente",
    version="1.2.0"
)

# Configuração de CORS
origins = [
    "http://localhost:5173", 
    "http://localhost:3000", 
    "https://smart-grow-4enawmc3i-gustavos-projects-6aab325e.vercel.app"
    "*"                      
]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"], 
    allow_headers=["*"],
)

# --- ESTADO GLOBAL ---

# Estado atual dos atuadores (0 a 100%)
estado_sistema = {
    "nivel_irrigacao": 0.0,
    "velocidade_ventilacao": 0.0,
    "nivel_iluminacao": 0.0  
}

# Configuração de Automação
modo_automatico = {
    "irrigacao": True,
    "iluminacao": True
}

# Estado dos controles manuais
controles_manuais = {
    "irrigacao": False, 
    "iluminacao": False
}

# --- ENDPOINTS ---

@app.get("/")
def read_root():
    return {"status": "ok", "message": "API da Estufa Inteligente Online"}

@app.post("/leituras")
def registrar_leitura(leitura: LeituraSensor):
    global estado_sistema

    # 1. Processamento Fuzzy
    nivel_irrigacao_calculado = calcular_nivel_irrigacao(
        leitura.temperatura_celsius,
        leitura.umidade_solo
    )

    velocidade_ventilacao_calculada = calcular_velocidade_ventilacao(
        leitura.temperatura_celsius
    )

    nivel_iluminacao_calculado = calcular_nivel_iluminacao(
        leitura.luminosidade
    )

    # 2. Atualização do Estado (Lógica Híbrida Auto/Manual)
    if modo_automatico["irrigacao"]:
        estado_sistema["nivel_irrigacao"] = nivel_irrigacao_calculado
    
    # Ventilação é sempre automática neste modelo
    estado_sistema["velocidade_ventilacao"] = velocidade_ventilacao_calculada

    if modo_automatico["iluminacao"]:
        estado_sistema["nivel_iluminacao"] = nivel_iluminacao_calculado

    # Log operacional
    print(f"[{datetime.datetime.now()}] Leitura: T={leitura.temperatura_celsius}°C, U={leitura.umidade_solo}%, L={leitura.luminosidade}%")
    print(f"  -> Estado: Irr={estado_sistema['nivel_irrigacao']:.1f}%, Ven={estado_sistema['velocidade_ventilacao']:.1f}%, Luz={estado_sistema['nivel_iluminacao']:.1f}%")

    # 3. Persistência no Banco de Dados
    try:
        conn = sqlite3.connect(DATABASE_NAME)
        cursor = conn.cursor()
        cursor.execute(
            "INSERT INTO leituras (temperatura, umidade, luminosidade) VALUES (?, ?, ?)", 
            (leitura.temperatura_celsius, leitura.umidade_solo, leitura.luminosidade)
        )        
        conn.commit()
        conn.close()
        return {
            "status": "sucesso", 
            "mensagem": "Leitura processada.", 
            "estado_atual": estado_sistema,
            "modo_automatico": modo_automatico
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Erro ao salvar no banco de dados: {e}")

@app.get("/configuracao/automacao")
def obter_configuracao_automacao():
    return modo_automatico

@app.post("/configuracao/automacao")
def definir_automacao(config: ConfigAutomacao):
    if config.sistema in modo_automatico:
        modo_automatico[config.sistema] = config.ativo
        return {"status": "sucesso", "mensagem": f"Automação de {config.sistema} definida para {config.ativo}"}
    raise HTTPException(status_code=400, detail="Sistema inválido")

@app.post("/controle/manual")
def controle_manual(comando: ComandoManual):
    if modo_automatico.get(comando.sistema):
        raise HTTPException(status_code=400, detail="Erro: Desative a automação antes de controlar manualmente.")
    
    if comando.sistema in controles_manuais:
        controles_manuais[comando.sistema] = comando.ligar
        
        # Sobrescreve o estado imediatamente (0% ou 100%)
        valor_manual = 100.0 if comando.ligar else 0.0
        
        if comando.sistema == "irrigacao":
            estado_sistema["nivel_irrigacao"] = valor_manual
        elif comando.sistema == "iluminacao":
            estado_sistema["nivel_iluminacao"] = valor_manual
            
        return {"status": "sucesso", "mensagem": f"{comando.sistema} manual definido para {valor_manual}%"}
    
    raise HTTPException(status_code=400, detail="Sistema inválido")

@app.get("/status_sistema")
def obter_status_sistema():
    return estado_sistema

@app.get("/leituras")
def obter_leituras():
    try:
        conn = sqlite3.connect(DATABASE_NAME)
        conn.row_factory = sqlite3.Row
        cursor = conn.cursor()
        cursor.execute("SELECT id, temperatura, umidade, luminosidade, horario FROM leituras ORDER BY horario DESC")
        leituras = cursor.fetchall()
        conn.close()
        return [dict(row) for row in leituras]
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Erro ao ler o banco de dados: {e}")