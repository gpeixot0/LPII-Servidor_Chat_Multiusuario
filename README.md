# LPII-Servidor_Chat_Multiusuario
Resumo

Este projeto implementa um servidor de chat concorrente utilizando sockets TCP e threads. O sistema permite que múltiplos clientes se conectem simultaneamente, enviem mensagens e recebam mensagens de outros usuários através de broadcast.


Arquitetura do Projeto

O sistema é dividido em headers principais:

libtslog.h
Biblioteca de logging thread-safe. Define níveis de log (DEBUG, INFO, WARN, ERROR) e funções para registrar mensagens com timestamp, nível de severidade e ID da thread.

server.h
Contém definições e funções relacionadas ao servidor TCP: inicialização do socket, aceitação de clientes e gerenciamento das threads.

client_handler.h
Define a lógica de atendimento de cada cliente conectado ao servidor. Inclui a recepção de mensagens e a retransmissão (broadcast).

client_cli.h
Implementa o cliente em linha de comando (CLI). Responsável por se conectar ao servidor, enviar mensagens e exibir mensagens recebidas.

chat.h
Define estruturas compartilhadas entre threads para criação do chat, como a lista de clientes e o histórico de mensagens, além da proteção por mutex.
