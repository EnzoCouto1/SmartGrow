# 🌱 SmartGrow - Backend de Estufa Inteligente

Este repositório contém o código do backend para o projeto de TCC "SmartGrow", um sistema de automação de estufa que utiliza **Lógica Fuzzy** para o controle inteligente de ambiente.

O sistema é construído em Python usando o framework **FastAPI** e é responsável por:
* Receber dados de sensores (temperatura, umidade do solo, luminosidade).
* Processar esses dados através de um motor de inferência Fuzzy.
* Retornar os níveis de controle (0-100%) para os atuadores (irrigação, ventilação, iluminação).
* Armazenar o histórico de leituras em um banco de dados SQLite.

## 🚀 Tecnologias Utilizadas
* **Python 3**
* **FastAPI:** Para a criação da API RESTful.
* **Scikit-Fuzzy (`skfuzzy`):** Para a implementação da Lógica de Controle Fuzzy.
* **SQLite3:** Para armazenamento leve e local do histórico de dados.
* **Uvicorn:** Como servidor ASGI para rodar a API.

## ⚙️ Como Rodar o Projeto Localmente

Siga estes passos para configurar e executar o backend no seu computador.

### 1. Pré-requisitos
* [Git](https://git-scm.com/)
* [Python 3.10+](https://www.python.org/)

### 2. Instalação

1.  **Clone o repositório:**
    ```bash
    git clone [https://github.com/EnzoCouto1/SmartGrow.git](https://github.com/EnzoCouto1/SmartGrow.git)
    cd SmartGrow/SmartGrow 
    ```
    *(Nota: Pode ser necessário navegar para a subpasta `SmartGrow/SmartGrow` onde os arquivos .py estão)*

2.  **Crie e ative um ambiente virtual:**
    ```bash
    # Criar o venv
    python -m venv venv
    
    # Ativar no Windows (PowerShell)
    .\venv\Scripts\Activate.ps1
    ```
    *(Se a ativação falhar no PowerShell, rode `Set-ExecutionPolicy Bypass -Scope Process` e tente novamente)*

3.  **Instale as dependências:**
    O arquivo `requirements.txt` [cite: 119] contém todas as bibliotecas necessárias.
    ```bash
    pip install -r requirements.txt
    ```

4.  **Crie o Banco de Dados:**
    Execute o script `database.py`  uma vez para criar o arquivo `leituras.db` e a tabela.
    ```bash
    python database.py
    ```

### 3. Execução

1.  **Inicie o Servidor da API:**
    ```bash
    # O --host 0.0.0.0 permite que o ESP32 na sua rede local acesse a API
    uvicorn main:app --reload --host 0.0.0.0
    ```

2.  **Acesse a API:**
    * **Servidor Rodando em:** `http://localhost:8000`
    * **Documentação Interativa (Swagger):** `http://localhost:8000/docs`

## 🧪 Como Testar o Backend

[cite_start]Enquanto o hardware (ESP32) não está conectado, você pode usar o script de teste interativo `teste_sensor.py`  para simular o envio de dados.

1.  Mantenha o servidor rodando no primeiro terminal.
2.  Abra um **segundo terminal**, ative o `venv` nele também.
3.  Execute o script de teste:
    ```bash
    python teste_sensor.py
    ```
4.  Siga as instruções no menu do console para enviar cenários de teste.

## 🤖 API Endpoints

### `POST /leituras`
Recebe as leituras dos sensores e aciona a lógica Fuzzy.
* **Corpo da Requisição (JSON):**
    ```json
    {
      "temperatura_celsius": 25.5,
      "umidade_solo": 45.2,
      "luminosidade": 80.0
    }
    ```
* **Resposta (JSON):**
    ```json
    {
      "status": "sucesso",
      "mensagem": "Leitura processada com lógica fuzzy.",
      "estado_atual": {
        "nivel_irrigacao": 15.0,
        "velocidade_ventilacao": 35.5,
        "nivel_iluminacao": 15.0
      }
    }
    ```

### `GET /status_sistema`
Consultado pelo hardware (ESP32) para obter as ordens de controle.
* **Resposta (JSON):**
    ```json
    {
      "nivel_irrigacao": 15.0,
      "velocidade_ventilacao": 35.5,
      "nivel_iluminacao": 15.0
    }
    ```

### `GET /leituras`
Consultado pelo frontend para exibir o histórico de dados.
* **Resposta (JSON):**
    ```json
    [
      {
        "id": 1,
        "temperatura": 25.5,
        "umidade": 45.2,
        "horario": "2025-11-05T20:30:00.123456"
      }
    ]
    ```