// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>
#include "libtslog.h"

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

// Variáveis globais
int server_fd;
Logger logger;

// Estrutura para armazenar dados do cliente
typedef struct {
    int socket;                    // Socket de conexão
    struct sockaddr_in addr;       // Endereço do cliente
    pthread_t thread_id;           // ID da thread
    int active;                    // Se está ativo
    char username[32];             // Nome do usuário
} client_t;

client_t *clients[MAX_CLIENTS];    // Array de clientes conectados
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;  // Protege o array

// Remove cliente da lista
void remove_client(int sock) {
    pthread_mutex_lock(&clients_mutex);  // Trava para acesso seguro
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket == sock) {
            LOG_INFO(&logger, "Removendo cliente %s (socket %d)", clients[i]->username, sock);
            close(clients[i]->socket);   // Fecha conexão
            free(clients[i]);            // Libera memória
            clients[i] = NULL;           // Marca como vazio
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);  // Destrava
}

// Envia mensagem para todos os clientes, exceto o remetente
void broadcast(const char *msg, int sender_sock) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket != sender_sock) {
            // Envia para cada cliente (ignora erros)
            send(clients[i]->socket, msg, strlen(msg), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Thread que cuida de cada cliente
void *client_handler(void *arg) {
    client_t *cli = (client_t *)arg;
    char buffer[BUFFER_SIZE];
    int bytes;

    // Pede nome de usuário
    char *prompt = "Digite seu nome de usuário: ";
    send(cli->socket, prompt, strlen(prompt), 0);

    // Recebe nome
    bytes = recv(cli->socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        remove_client(cli->socket);
        pthread_exit(NULL);
    }
    buffer[bytes] = '\0';
    
    // Remove quebra de linha do nome
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') {
        buffer[len-1] = '\0';
    }
    
    // Salva nome (ou "Anonimo" se vazio)
    if (strlen(buffer) == 0) {
        strncpy(cli->username, "Anonimo", sizeof(cli->username) - 1);
    } else {
        strncpy(cli->username, buffer, sizeof(cli->username) - 1);
    }
    cli->username[sizeof(cli->username) - 1] = '\0';

    LOG_INFO(&logger, "Novo cliente: %s (socket %d)", cli->username, cli->socket);

    // Avisa que cliente entrou no chat
    char welcome_msg[BUFFER_SIZE];
    snprintf(welcome_msg, sizeof(welcome_msg), "--- %s entrou no chat ---\n", cli->username);
    broadcast(welcome_msg, cli->socket);

    // Loop principal: recebe mensagens do cliente
    while ((bytes = recv(cli->socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        
        // Log da mensagem (trunca se for muito longa)
        if (strlen(buffer) > 50) {
            char truncated[54];
            strncpy(truncated, buffer, 50);
            truncated[50] = '\0';
            strcat(truncated, "...");
            LOG_DEBUG(&logger, "Mensagem de %s: %s", cli->username, truncated);
        } else {
            LOG_DEBUG(&logger, "Mensagem de %s: %s", cli->username, buffer);
        }

        // Verifica se quer sair
        if (strcmp(buffer, "/quit\n") == 0) {
            LOG_INFO(&logger, "Cliente %s saiu", cli->username);
            break;
        }

        // Formata mensagem com nome do usuário
        char formatted_msg[BUFFER_SIZE];
        char clean_msg[BUFFER_SIZE];
        strncpy(clean_msg, buffer, sizeof(clean_msg) - 1);
        clean_msg[sizeof(clean_msg) - 1] = '\0';
        
        // Remove quebra de linha
        size_t msg_len = strlen(clean_msg);
        if (msg_len > 0 && clean_msg[msg_len-1] == '\n') {
            clean_msg[msg_len-1] = '\0';
        }
        
        // Cria mensagem formatada: [nome] mensagem
        snprintf(formatted_msg, sizeof(formatted_msg), "[%s] %s\n", cli->username, clean_msg);
        
        // Envia para todos os outros clientes
        broadcast(formatted_msg, cli->socket);
    }

    // Cliente desconectou ou teve erro
    if (bytes == 0) {
        LOG_INFO(&logger, "Cliente %s desconectou", cli->username);
    } else if (bytes < 0) {
        LOG_ERROR(&logger, "Erro com %s: %s", cli->username, strerror(errno));
    }

    // Avisa que cliente saiu do chat
    char leave_msg[BUFFER_SIZE];
    snprintf(leave_msg, sizeof(leave_msg), "--- %s saiu do chat ---\n", cli->username);
    broadcast(leave_msg, cli->socket);

    remove_client(cli->socket);
    pthread_exit(NULL);
}

// Função chamada quando aperta Ctrl+C
void handle_sigint(int sig) {
    LOG_WARN(&logger, "Recebido Ctrl+C, parando servidor...");
    
    // Fecha socket do servidor para sair do accept()
    shutdown(server_fd, SHUT_RDWR);
}

// Limpa tudo antes de sair
void cleanup_server() {
    LOG_WARN(&logger, "Limpando recursos...");

    // Fecha todos os clientes conectados
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i]) {
            LOG_INFO(&logger, "Desconectando %s", clients[i]->username);
            shutdown(clients[i]->socket, SHUT_RDWR);
            close(clients[i]->socket);
            free(clients[i]);
            clients[i] = NULL;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    // Fecha socket do servidor
    if (server_fd >= 0) {
        close(server_fd);
    }

    log_close(&logger);
    LOG_INFO(&logger, "Servidor parado");
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    // Configura tratamento do Ctrl+C
    signal(SIGINT, handle_sigint);

    // Inicializa sistema de logs
    log_init(&logger, LOG_DEBUG);

    LOG_INFO(&logger, "Iniciando servidor de chat...");

    // Cria socket do servidor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        LOG_ERROR(&logger, "Erro ao criar socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Permite reusar a porta rapidamente
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configura endereço do servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Aceita de qualquer IP
    server_addr.sin_port = htons(PORT);

    // Associa socket ao endereço
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR(&logger, "Erro no bind: %s", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Coloca socket em modo escuta
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        LOG_ERROR(&logger, "Erro no listen: %s", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    LOG_INFO(&logger, "Servidor rodando na porta %d", PORT);
    LOG_INFO(&logger, "Aguardando conexões...");

    // Inicializa array de clientes como vazio
    memset(clients, 0, sizeof(clients));

    // Loop principal: aceita novas conexões
    while (1) {
        client_len = sizeof(client_addr);
        int new_sock = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (new_sock < 0) {
            // Ignora erros se foi por causa do Ctrl+C
            if (errno != EINTR) {
                LOG_ERROR(&logger, "Erro ao aceitar conexão: %s", strerror(errno));
            }
            continue;
        }

        // Cria estrutura para novo cliente
        client_t *cli = malloc(sizeof(client_t));
        if (!cli) {
            LOG_ERROR(&logger, "Erro de memória");
            close(new_sock);
            continue;
        }

        cli->socket = new_sock;
        cli->addr = client_addr;
        cli->active = 1;

        // Adiciona cliente na primeira posição vazia
        pthread_mutex_lock(&clients_mutex);
        int added = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i]) {
                clients[i] = cli;
                added = 1;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (!added) {
            // Servidor cheio - rejeita cliente
            LOG_WARN(&logger, "Servidor cheio! Rejeitando cliente.");
            close(new_sock);
            free(cli);
            continue;
        }

        // Cria thread para cuidar do cliente
        if (pthread_create(&cli->thread_id, NULL, client_handler, (void *)cli) != 0) {
            LOG_ERROR(&logger, "Erro ao criar thread");
            remove_client(new_sock);
            continue;
        }

        // Não precisa esperar thread terminar
        pthread_detach(cli->thread_id);
        
        LOG_INFO(&logger, "Cliente aceito. Total: %d", added);
    }

    cleanup_server();
    return 0;
}
