# main.py
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
import sqlite3
import datetime

# Importa TODAS as funções do nosso cérebro fuzzy
from fuzzy_logic import (
    calcular_nivel_irrigacao, 
    calcular_velocidade_ventilacao,
    calcular_nivel_iluminacao  # <-- NOVO IMPORT
)

DATABASE_NAME = "leituras.db"

# --- NOVO: Atualiza o "Formulário" para aceitar a luminosidade ---
class LeituraSensor(BaseModel):
    temperatura_celsius: float
    umidade_solo: float
    luminosidade: float  # <-- CAMPO OBRIGATÓRIO

app = FastAPI(
    title="API da Estufa Inteligente com Lógica Fuzzy",
    version="1.1.0" # Versão 1.1! Controle de Iluminação
)

# --- NOVO: "Painel de Controle" agora está completo ---
estado_sistema = {
    "nivel_irrigacao": 0.0,
    "velocidade_ventilacao": 0.0,
    "nivel_iluminacao": 0.0  # <-- NOVO CONTROLE
}

@app.get("/")
def read_root():
    return {"status": "ok", "message": "Bem-vindo à API da Estufa Inteligente!"}

@app.post("/leituras")
def registrar_leitura(leitura: LeituraSensor):
    global estado_sistema

    # --- Lógica de Decisão FUZZY (Completa) ---

    # 1. Calcula o nível de irrigação
    nivel_irrigacao_calculado = calcular_nivel_irrigacao(
        leitura.temperatura_celsius,
        leitura.umidade_solo
    )

    # 2. Calcula a velocidade da ventilação
    velocidade_ventilacao_calculada = calcular_velocidade_ventilacao(
        leitura.temperatura_celsius
    )

    # 3. --- NOVO: Calcula o nível de iluminação ---
    nivel_iluminacao_calculado = calcular_nivel_iluminacao(
        leitura.luminosidade
    )

    # Atualiza o estado completo do sistema
    estado_sistema["nivel_irrigacao"] = nivel_irrigacao_calculado
    estado_sistema["velocidade_ventilacao"] = velocidade_ventilacao_calculada
    estado_sistema["nivel_iluminacao"] = nivel_iluminacao_calculado

    print(f"[{datetime.datetime.now()}] Leitura: Temp={leitura.temperatura_celsius}°C, Umidade={leitura.umidade_solo}%, Lum={leitura.luminosidade}%")
    print(f"  -> DECISÃO FUZZY: Irrigação={nivel_irrigacao_calculado:.2f}%, Ventilação={velocidade_ventilacao_calculada:.2f}%, Iluminação={nivel_iluminacao_calculado:.2f}%")

    # Salva no banco de dados
    # ATENÇÃO: O banco de dados NÃO salvará a luminosidade
    # (Podemos adicionar isso depois se for necessário, mas por enquanto funciona)
    try:
        conn = sqlite3.connect(DATABASE_NAME)
        cursor = conn.cursor()
        cursor.execute("INSERT INTO leituras (temperatura, umidade) VALUES (?, ?)", (leitura.temperatura_celsius, leitura.umidade_solo))
        conn.commit()
        conn.close()
        return {"status": "sucesso", "mensagem": "Leitura processada com lógica fuzzy.", "estado_atual": estado_sistema}
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Erro ao salvar no banco de dados: {e}")

@app.get("/status_sistema")
def obter_status_sistema():
    """ Endpoint para o hardware consultar o estado de TODOS os atuadores. """
    return estado_sistema # Retorna automaticamente o novo estado

@app.get("/leituras")
def obter_leituras():
    """ Busca e retorna todas as leituras salvas no banco de dados. """
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