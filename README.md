# LPII-Servidor_Chat_Multiusuario
Etapa 2 da entrega do servidor chat multiusuario


Estrutura do Projeto
.
├── libtslog.h               # Biblioteca de logging thread-safe
├── server.c                 # Código do servidor
├── client.c                 # Código do cliente
├── Script_teste_server.sh   # Script de teste multiusuário
└── README.md

Compilação
Servidor
gcc Servidor.c -o server -pthread

Cliente
gcc Cliente.c -o client -pthread

O script Script_teste_server.sh não compila os arquivos, apenas inicia o servidor e múltiplos clientes automaticamente para teste.

Execução
Rodar servidor
./server

Rodar cliente
./client

O cliente solicita o nome de usuário ao conectar.
Para sair do chat, use o comando /quit.

Teste multiusuário automático
./Script_Teste_Server.sh

Inicia o servidor e múltiplos clientes simultaneamente.
Cada cliente envia mensagens pré-definidas e depois executa /quit.
Logs do servidor são exibidos no terminal em tempo real.

Arquitetura:

Servidor

Escuta conexões TCP na porta 8080.
Aceita até MAX_CLIENTS (configurável).
Cada cliente é gerenciado por uma thread própria.
Faz broadcast de mensagens para todos os clientes conectados.
Logger thread-safe registra eventos em tempo real.

Cliente

Conecta ao servidor via TCP.
Thread principal lê a entrada do usuário e envia mensagens.
Thread separada recebe mensagens do servidor e imprime no terminal.
Comando /quit encerra o cliente.

Logger (libtslog.h)

Garante thread-safety usando pthread_mutex_t.
Suporta níveis de log: DEBUG, INFO, WARN e ERROR.
Usado pelo servidor para registrar conexões, mensagens e erros.
