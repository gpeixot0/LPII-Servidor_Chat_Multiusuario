#!/bin/bash

# Configurações
NUM_CLIENTS=3
CLIENT_EXEC="./client"
SERVER_EXEC="./server"
RUN_TIME=15   # tempo total do teste em segundos

# Função para encerrar todos os processos ao sair
cleanup() {
    echo "Encerrando teste..."
    pkill -f "$CLIENT_EXEC"
    pkill -f "$SERVER_EXEC"
    exit
}

trap cleanup SIGINT SIGTERM

# Iniciar servidor em background
echo "Iniciando servidor..."
$SERVER_EXEC &
SERVER_PID=$!
sleep 1   # esperar o servidor iniciar

# Iniciar clientes
for i in $(seq 1 $NUM_CLIENTS); do
    (
        # Nome de usuário
        echo "User$i"
        sleep 1
        # Loop enviando mensagens aleatórias
        for j in $(seq 1 5); do
            MSG="Mensagem $j do User$i"
            echo "$MSG"
            sleep $((RANDOM % 3 + 1))  # pausa de 1 a 3 segundos
        done
        echo "/quit"
    ) | $CLIENT_EXEC &
done

# Esperar tempo de execução do teste
sleep $RUN_TIME

# Cleanup
cleanup
