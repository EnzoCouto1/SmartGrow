from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from fastapi.middleware.cors import CORSMiddleware
import sqlite3
import datetime
import threading

# Importa as funções do cérebro fuzzy
from fuzzy_logic import calcular_nivel_irrigacao, calcular_velocidade_ventilacao

DATABASE_NAME = "leituras.db"

# --- THREAD LOCK ---
lock = threading.Lock()

# --- CONFIGURAÇÕES GERAIS ---
HORA_LIGAR_LUZES = 18  # 18:00
HORA_DESLIGAR_LUZES = 6 # 06:00

# --- MODELOS DE DADOS ---
class LeituraSensor(BaseModel):
    temperatura_celsius: float
    umidade_solo: float

class ConfigAutomacao(BaseModel):
    sistema: str # "irrigacao", "iluminacao" ou "ventilacao"
    ativo: bool

class ComandoManual(BaseModel):
    sistema: str # "irrigacao", "iluminacao" ou "ventilacao"
    ligar: bool

# --- CONFIGURAÇÃO DA API ---

print("\n\n--- CARREGANDO API COM SUPORTE A VENTILAÇÃO! ---\n\n")

app = FastAPI(
    title="API da Estufa Inteligente",
    version="3.0.0" 
)

@app.on_event("startup")
def startup_db():
    conn = sqlite3.connect(DATABASE_NAME)
    cursor = conn.cursor()
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS leituras (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        temperatura REAL NOT NULL,
        umidade REAL NOT NULL,
        horario TEXT DEFAULT CURRENT_TIMESTAMP
    );
    """)
    conn.commit()
    conn.close()
    print("--- BANCO DE DADOS VERIFICADO ---")

origins = [
    "http://localhost:5173",
    "http://localhost:3000",
    "https://smart-grow-4enawmc3i-gustavos-projects-6aab325e.vercel.app",
    "*"
]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# --- ESTADO GLOBAL (PROTEGIDO) ---

estado_sistema = {
    "nivel_irrigacao": 0.0,
    "velocidade_ventilacao": 0.0,
    "nivel_iluminacao": 0.0
}

# --- CORREÇÃO 1: Adicionando 'ventilacao' aqui ---
modo_automatico = {
    "irrigacao": True,
    "iluminacao": True,
    "ventilacao": True 
}

# --- CORREÇÃO 2: Adicionando 'ventilacao' aqui ---
controles_manuais = {
    "irrigacao": False,
    "iluminacao": False,
    "ventilacao": False
}

# --- ENDPOINTS ---

@app.get("/")
def read_root():
    return {"status": "ok", "message": "API da Estufa Inteligente Online v3"}

@app.post("/leituras")
def registrar_leitura(leitura: LeituraSensor):
    global estado_sistema

    # 1. Cálculos
    nivel_irrigacao_calculado = calcular_nivel_irrigacao(
        leitura.temperatura_celsius,
        leitura.umidade_solo
    )

    velocidade_ventilacao_calculada = calcular_velocidade_ventilacao(
        leitura.temperatura_celsius
    )

    hora_atual = datetime.datetime.now().hour
    if hora_atual >= HORA_LIGAR_LUZES or hora_atual < HORA_DESLIGAR_LUZES:
        nivel_iluminacao_calculado = 100.0
    else:
        nivel_iluminacao_calculado = 0.0

    # 2. Atualização do Estado
    with lock:
        # Irrigação
        if modo_automatico["irrigacao"]:
            estado_sistema["nivel_irrigacao"] = nivel_irrigacao_calculado

        # --- CORREÇÃO 3: Ventilação agora respeita o automático ---
        if modo_automatico["ventilacao"]:
            estado_sistema["velocidade_ventilacao"] = velocidade_ventilacao_calculada

        # Iluminação
        if modo_automatico["iluminacao"]:
            estado_sistema["nivel_iluminacao"] = nivel_iluminacao_calculado

        print(f"[{datetime.datetime.now()}] Leitura: T={leitura.temperatura_celsius}°C, U={leitura.umidade_solo}%")
        print(f"  -> Estado: Irr={estado_sistema['nivel_irrigacao']:.1f}%, Ven={estado_sistema['velocidade_ventilacao']:.1f}%, Luz={estado_sistema['nivel_iluminacao']:.1f}%")

        estado_para_retorno = estado_sistema.copy()
        modo_para_retorno = modo_automatico.copy()

    # 3. Salvar no Banco
    try:
        conn = sqlite3.connect(DATABASE_NAME)
        cursor = conn.cursor()
        cursor.execute(
            "INSERT INTO leituras (temperatura, umidade) VALUES (?, ?)",
            (leitura.temperatura_celsius, leitura.umidade_solo)
        )
        conn.commit()
        conn.close()
        return {
            "status": "sucesso",
            "estado_atual": estado_para_retorno,
            "modo_automatico": modo_para_retorno
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Erro ao salvar no banco de dados: {e}")

@app.get("/configuracao/automacao")
def obter_configuracao_automacao():
    with lock:
        return modo_automatico.copy()

@app.post("/configuracao/automacao")
def definir_automacao(config: ConfigAutomacao):
    # Agora 'ventilacao' vai passar aqui porque adicionamos no dicionário lá em cima
    if config.sistema in modo_automatico:
        with lock:
            modo_automatico[config.sistema] = config.ativo
            
            # Se desativar, zera o valor
            if not config.ativo:
                if config.sistema == "irrigacao":
                    estado_sistema["nivel_irrigacao"] = 0.0
                elif config.sistema == "iluminacao":
                    estado_sistema["nivel_iluminacao"] = 0.0
                elif config.sistema == "ventilacao": # Adicionado
                    estado_sistema["velocidade_ventilacao"] = 0.0
            
            return {"status": "sucesso", "mensagem": f"Automação de {config.sistema} definida para {config.ativo}"}
            
    raise HTTPException(status_code=400, detail="Sistema inválido")

@app.post("/controle/manual")
def controle_manual(comando: ComandoManual):
    with lock:
        if modo_automatico.get(comando.sistema):
            raise HTTPException(status_code=400, detail=f"Erro: Desative a automação de {comando.sistema} antes de controlar manualmente.")

        if comando.sistema in controles_manuais:
            controles_manuais[comando.sistema] = comando.ligar
            valor_manual = 100.0 if comando.ligar else 0.0

            if comando.sistema == "irrigacao":
                estado_sistema["nivel_irrigacao"] = valor_manual
            elif comando.sistema == "iluminacao":
                estado_sistema["nivel_iluminacao"] = valor_manual
            elif comando.sistema == "ventilacao": # Adicionado para aceitar comando manual
                estado_sistema["velocidade_ventilacao"] = valor_manual

            return {"status": "sucesso", "mensagem": f"{comando.sistema} manual definido para {valor_manual}%"}

    raise HTTPException(status_code=400, detail="Sistema inválido")

@app.get("/status_sistema")
def obter_status_sistema():
    with lock:
        return estado_sistema.copy()

@app.get("/leituras")
def obter_leituras():
    try:
        conn = sqlite3.connect(DATABASE_NAME)
        conn.row_factory = sqlite3.Row
        cursor = conn.cursor()
        cursor.execute("SELECT id, temperatura, umidade, horario FROM leituras ORDER BY horario DESC")
        leituras = cursor.fetchall()
        conn.close()
        return [dict(row) for row in leituras]
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Erro ao ler o banco de dados: {e}")