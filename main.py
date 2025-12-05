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
HORA_LIGAR_LUZES = 18 
HORA_DESLIGAR_LUZES = 6 

# --- MODELOS DE DADOS ---
class LeituraSensor(BaseModel):
    temperatura_celsius: float
    umidade_solo: float
    distancia_cm: float # --- NOVO: Campo para o Ultrassônico ---

class ConfigAutomacao(BaseModel):
    sistema: str 
    ativo: bool

class ComandoManual(BaseModel):
    sistema: str 
    ligar: bool

# --- CONFIGURAÇÃO DA API ---

print("\n\n--- API v4.0 (Suporte a Nível de Reservatório) ---\n\n")

app = FastAPI(
    title="API da Estufa Inteligente",
    version="4.0.0" 
)

@app.on_event("startup")
def startup_db():
    conn = sqlite3.connect(DATABASE_NAME)
    cursor = conn.cursor()
    # ATENÇÃO: Apague o arquivo .db antigo para essa nova tabela funcionar!
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS leituras (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        temperatura REAL NOT NULL,
        umidade REAL NOT NULL,
        distancia_cm REAL, 
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

# --- ESTADO GLOBAL ---

estado_sistema = {
    "nivel_irrigacao": 0.0,
    "velocidade_ventilacao": 0.0,
    "nivel_iluminacao": 0.0,
    "nivel_reservatorio_cm": 0.0 # --- NOVO: Para o Frontend ler ---
}

modo_automatico = {
    "irrigacao": True,
    "iluminacao": True,
    "ventilacao": True 
}

controles_manuais = {
    "irrigacao": False,
    "iluminacao": False,
    "ventilacao": False
}

# --- ENDPOINTS ---

@app.get("/")
def read_root():
    return {"status": "ok", "message": "API SmartGrow Online v4"}

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
        # Atualiza o nível do reservatório sempre (é apenas leitura)
        estado_sistema["nivel_reservatorio_cm"] = leitura.distancia_cm

        if modo_automatico["irrigacao"]:
            estado_sistema["nivel_irrigacao"] = nivel_irrigacao_calculado

        if modo_automatico["ventilacao"]:
            estado_sistema["velocidade_ventilacao"] = velocidade_ventilacao_calculada

        if modo_automatico["iluminacao"]:
            estado_sistema["nivel_iluminacao"] = nivel_iluminacao_calculado

        print(f"[{datetime.datetime.now()}] T={leitura.temperatura_celsius}°C, U={leitura.umidade_solo}%, Dist={leitura.distancia_cm}cm")

        estado_para_retorno = estado_sistema.copy()
        modo_para_retorno = modo_automatico.copy()

    # 3. Salvar no Banco
    try:
        conn = sqlite3.connect(DATABASE_NAME)
        cursor = conn.cursor()
        cursor.execute(
            "INSERT INTO leituras (temperatura, umidade, distancia_cm) VALUES (?, ?, ?)",
            (leitura.temperatura_celsius, leitura.umidade_solo, leitura.distancia_cm)
        )
        conn.commit()
        conn.close()
        return {
            "status": "sucesso",
            "estado_atual": estado_para_retorno,
            "modo_automatico": modo_para_retorno
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Erro DB: {e}")

@app.get("/configuracao/automacao")
def obter_configuracao_automacao():
    with lock:
        return modo_automatico.copy()

@app.post("/configuracao/automacao")
def definir_automacao(config: ConfigAutomacao):
    if config.sistema in modo_automatico:
        with lock:
            modo_automatico[config.sistema] = config.ativo
            if not config.ativo:
                if config.sistema == "irrigacao": estado_sistema["nivel_irrigacao"] = 0.0
                elif config.sistema == "iluminacao": estado_sistema["nivel_iluminacao"] = 0.0
                elif config.sistema == "ventilacao": estado_sistema["velocidade_ventilacao"] = 0.0
            
            return {"status": "sucesso", "mensagem": f"{config.sistema} auto: {config.ativo}"}
            
    raise HTTPException(status_code=400, detail="Sistema inválido")

@app.post("/controle/manual")
def controle_manual(comando: ComandoManual):
    with lock:
        if modo_automatico.get(comando.sistema):
            raise HTTPException(status_code=400, detail=f"Erro: Desative automação de {comando.sistema}")

        if comando.sistema in controles_manuais:
            controles_manuais[comando.sistema] = comando.ligar
            valor_manual = 100.0 if comando.ligar else 0.0

            if comando.sistema == "irrigacao": estado_sistema["nivel_irrigacao"] = valor_manual
            elif comando.sistema == "iluminacao": estado_sistema["nivel_iluminacao"] = valor_manual
            elif comando.sistema == "ventilacao": estado_sistema["velocidade_ventilacao"] = valor_manual

            return {"status": "sucesso", "mensagem": f"{comando.sistema} manual: {valor_manual}%"}

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
        cursor.execute("SELECT * FROM leituras ORDER BY horario DESC")
        leituras = cursor.fetchall()
        conn.close()
        return [dict(row) for row in leituras]
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Erro DB: {e}")