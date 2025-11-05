import sqlite3

DATABASE_NAME = "leituras.db"

# Conecta ao banco de dados (cria o arquivo se ele não existir)
conn = sqlite3.connect(DATABASE_NAME)
cursor = conn.cursor()

# Cria a tabela "leituras" se ela ainda não existir
# Esta tabela vai armazenar os dados dos sensores
cursor.execute("""
CREATE TABLE IF NOT EXISTS leituras (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    temperatura REAL NOT NULL,
    umidade REAL NOT NULL,
    luminosidade REAL NOT NULL,
    horario TEXT DEFAULT CURRENT_TIMESTAMP
);
""")

print("Banco de dados e tabela 'leituras' verificados/criados com sucesso.")

# Salva as alterações e fecha a conexão
conn.commit()
conn.close()