// Servidor.c 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>
#include <semaphore.h>
#include <errno.h>
#include "libtslog.h"

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define MSG_QUEUE_SIZE 128

/* Estrutura que representa um cliente */
typedef struct {
    int socket;
    struct sockaddr_in addr;
    pthread_t thread_id;
    char username[32];
    int active;
} client_t;

/* Monitor que gerencia a lista de clientes conectados */
typedef struct {
    client_t *clients[MAX_CLIENTS];
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} ClientList;

/* Monitor que implementa uma fila de mensagens  */
typedef struct {
    char *buf[MSG_QUEUE_SIZE];
    int sender[MSG_QUEUE_SIZE];
    int head;
    int tail;
    pthread_mutex_t mutex;
    sem_t slots;
    sem_t items;
    int shutdown;
} MessageQueue;

/* Variáveis globais */
int server_fd = -1;
Logger logger;
ClientList client_list;
MessageQueue msg_queue;
sem_t client_slots;
pthread_t broadcaster_tid;

/* --------------------------
   Funções do ClientList
   -------------------------- */
void clientlist_init(ClientList *cl) {
    memset(cl->clients, 0, sizeof(cl->clients));
    cl->count = 0;
    pthread_mutex_init(&cl->mutex, NULL);
    pthread_cond_init(&cl->cond, NULL);
}

/* Adiciona um cliente à lista */
int clientlist_add(ClientList *cl, client_t *cli) {
    pthread_mutex_lock(&cl->mutex);
    int added = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!cl->clients[i]) {
            cl->clients[i] = cli;
            cl->count++;
            added = 1;
            break;
        }
    }
    if (added) pthread_cond_broadcast(&cl->cond);
    pthread_mutex_unlock(&cl->mutex);
    return added;
}

/* Remove um cliente da lista com base no socket */
void clientlist_remove_by_sock(ClientList *cl, int sock) {
    pthread_mutex_lock(&cl->mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (cl->clients[i] && cl->clients[i]->socket == sock) {
            client_t *tmp = cl->clients[i];
            LOG_INFO(&logger, "Removendo cliente %s (socket %d)", tmp->username, sock);
            close(tmp->socket);
            free(tmp);
            cl->clients[i] = NULL;
            cl->count--;
            sem_post(&client_slots);
            break;
        }
    }
    if (cl->count == 0) pthread_cond_broadcast(&cl->cond);
    pthread_mutex_unlock(&cl->mutex);
}

/* Retorna um snapshot dos sockets ativos */
int *clientlist_snapshot_sockets(ClientList *cl, int *out_count) {
    pthread_mutex_lock(&cl->mutex);
    int n = cl->count;
    int *arr = NULL;
    if (n > 0) {
        arr = malloc(sizeof(int) * n);
        int idx = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (cl->clients[i]) arr[idx++] = cl->clients[i]->socket;
        }
    }
    *out_count = n;
    pthread_mutex_unlock(&cl->mutex);
    return arr;
}

/* Espera até que não existam clientes conectados */
void clientlist_wait_until_empty(ClientList *cl) {
    pthread_mutex_lock(&cl->mutex);
    while (cl->count > 0)
        pthread_cond_wait(&cl->cond, &cl->mutex);
    pthread_mutex_unlock(&cl->mutex);
}

/* --------------------------
   Funções da MessageQueue
   -------------------------- */
void msgqueue_init(MessageQueue *q) {
    q->head = q->tail = 0;
    pthread_mutex_init(&q->mutex, NULL);
    sem_init(&q->slots, 0, MSG_QUEUE_SIZE);
    sem_init(&q->items, 0, 0);
    q->shutdown = 0;
}

/* Libera recursos da fila */
void msgqueue_destroy(MessageQueue *q) {
    pthread_mutex_lock(&q->mutex);
    while (q->head != q->tail) {
        free(q->buf[q->head]);
        q->head = (q->head + 1) % MSG_QUEUE_SIZE;
    }
    pthread_mutex_unlock(&q->mutex);
    pthread_mutex_destroy(&q->mutex);
    sem_destroy(&q->slots);
    sem_destroy(&q->items);
}

/* Insere uma mensagem na fila */
int msgqueue_enqueue(MessageQueue *q, const char *msg, int sender_sock) {
    if (q->shutdown) return -1;
    if (sem_wait(&q->slots) != 0) return -1;
    pthread_mutex_lock(&q->mutex);
    if (q->shutdown) {
        pthread_mutex_unlock(&q->mutex);
        sem_post(&q->slots);
        return -1;
    }
    char *copy = strdup(msg);
    q->buf[q->tail] = copy;
    q->sender[q->tail] = sender_sock;
    q->tail = (q->tail + 1) % MSG_QUEUE_SIZE;
    pthread_mutex_unlock(&q->mutex);
    sem_post(&q->items);
    return 0;
}

/* Remove uma mensagem da fila */
int msgqueue_dequeue(MessageQueue *q, char **out_msg, int *out_sender) {
    if (sem_wait(&q->items) != 0) return -1;
    pthread_mutex_lock(&q->mutex);
    if (q->shutdown && q->head == q->tail) {
        pthread_mutex_unlock(&q->mutex);
        sem_post(&q->items);
        return -1;
    }
    *out_msg = q->buf[q->head];
    *out_sender = q->sender[q->head];
    q->head = (q->head + 1) % MSG_QUEUE_SIZE;
    pthread_mutex_unlock(&q->mutex);
    sem_post(&q->slots);
    return 0;
}

/* Ativa o modo de desligamento da fila */
void msgqueue_shutdown(MessageQueue *q) {
    pthread_mutex_lock(&q->mutex);
    q->shutdown = 1;
    pthread_mutex_unlock(&q->mutex);
    sem_post(&q->items);
}

/* --------------------------
   Thread de broadcast
   -------------------------- */
void *broadcaster(void *arg) {
    (void)arg;
    while (1) {
        char *msg = NULL;
        int sender = -1;
        int r = msgqueue_dequeue(&msg_queue, &msg, &sender);
        if (r != 0) {
            pthread_mutex_lock(&msg_queue.mutex);
            int sd = msg_queue.shutdown;
            pthread_mutex_unlock(&msg_queue.mutex);
            if (sd) break;
            else continue;
        }

        int count = 0;
        int *sockets = clientlist_snapshot_sockets(&client_list, &count);
        if (count > 0 && sockets != NULL) {
            for (int i = 0; i < count; i++) {
                int dst = sockets[i];
                if (dst != sender) send(dst, msg, strlen(msg), 0);
            }
            free(sockets);
        }
        free(msg);
    }
    LOG_INFO(&logger, "Broadcaster finalizado");
    return NULL;
}

/* --------------------------
   Thread do cliente
   -------------------------- */
void *client_handler(void *arg) {
    client_t *cli = (client_t *)arg;
    char buffer[BUFFER_SIZE];
    ssize_t bytes;

    const char *prompt = "Digite seu nome de usuário: ";
    send(cli->socket, prompt, strlen(prompt), 0);

    bytes = recv(cli->socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        clientlist_remove_by_sock(&client_list, cli->socket);
        pthread_exit(NULL);
    }

    buffer[bytes] = '\0';
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') buffer[len-1] = '\0';


    if (strlen(buffer) == 0) {
        strncpy(cli->username, "Anonimo", sizeof(cli->username) - 1);
    } else {
        strncpy(cli->username, buffer, sizeof(cli->username) - 1);
    }
    cli->username[sizeof(cli->username)-1] = '\0';  

    LOG_INFO(&logger, "Novo cliente: %s (socket %d)", cli->username, cli->socket);

    char welcome[BUFFER_SIZE];
    snprintf(welcome, sizeof(welcome), "--- %s entrou no chat ---\n", cli->username);
    msgqueue_enqueue(&msg_queue, welcome, cli->socket);

    /* Recebe e encaminha mensagens */
    while ((bytes = recv(cli->socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';

        /* verifica saída */
        if (strcmp(buffer, "/quit\n") == 0 || strcmp(buffer, "/quit") == 0) {
            break;
        }

        /* loga a mensagem recebida  */
        if (strlen(buffer) > 100) {
            char truncated[104];
            strncpy(truncated, buffer, 100);
            truncated[100] = '\0';
            LOG_DEBUG(&logger, "Mensagem de %s (trunc): %s", cli->username, truncated);
        } else {
            LOG_DEBUG(&logger, "Mensagem de %s: %s", cli->username, buffer);
        }

        /* prepara mensagem formatada e enfileira */
        char formatted[BUFFER_SIZE];
        /* garante que o buffer enviado contenha um '\n' ao final para o cliente receptor */
        size_t mlen = strlen(buffer);
        if (mlen > 0 && buffer[mlen-1] == '\n') {
            snprintf(formatted, sizeof(formatted), "[%s] %s", cli->username, buffer);
        } else {
            snprintf(formatted, sizeof(formatted), "[%s] %s\n", cli->username, buffer);
        }

        if (msgqueue_enqueue(&msg_queue, formatted, cli->socket) != 0) {
            LOG_WARN(&logger, "Fila cheia ou shutting down — descartando mensagem de %s", cli->username);
            break;
        }
    }

    /* Remove cliente ao sair */
    char leave_msg[BUFFER_SIZE];
    snprintf(leave_msg, sizeof(leave_msg), "--- %s saiu do chat ---\n", cli->username);
    msgqueue_enqueue(&msg_queue, leave_msg, cli->socket);

    clientlist_remove_by_sock(&client_list, cli->socket);
    pthread_exit(NULL);
}

/* --------------------------
   Encerramento e limpeza
   -------------------------- */
void handle_sigint(int sig) {
    (void)sig;
    LOG_WARN(&logger, "Recebido SIGINT, iniciando shutdown...");
    if (server_fd >= 0) shutdown(server_fd, SHUT_RDWR);
    msgqueue_shutdown(&msg_queue);
}

/* Libera todos os recursos e encerra o servidor */
void cleanup_server() {
    LOG_INFO(&logger, "Iniciando cleanup...");
    msgqueue_shutdown(&msg_queue);
    pthread_join(broadcaster_tid, NULL);
    pthread_mutex_lock(&client_list.mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_list.clients[i]) {
            shutdown(client_list.clients[i]->socket, SHUT_RDWR);
            close(client_list.clients[i]->socket);
        }
    }
    pthread_mutex_unlock(&client_list.mutex);
    clientlist_wait_until_empty(&client_list);
    msgqueue_destroy(&msg_queue);
    pthread_mutex_destroy(&client_list.mutex);
    pthread_cond_destroy(&client_list.cond);
    LOG_INFO(&logger, "Cleanup finalizado");
    log_close(&logger);
}

/* --------------------------
   Função principal
   -------------------------- */
int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    signal(SIGINT, handle_sigint);
    log_init(&logger, LOG_DEBUG);
    LOG_INFO(&logger, "Iniciando servidor ...");

    clientlist_init(&client_list);
    msgqueue_init(&msg_queue);
    sem_init(&client_slots, 0, MAX_CLIENTS);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) exit(EXIT_FAILURE);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) exit(EXIT_FAILURE);
    if (listen(server_fd, MAX_CLIENTS) < 0) exit(EXIT_FAILURE);
    LOG_INFO(&logger, "Servidor rodando na porta %d", PORT);

    pthread_create(&broadcaster_tid, NULL, broadcaster, NULL);
    
    
    /* Aceita novas conexões e cria threads para cada cliente */
    while (1) {
    client_len = sizeof(client_addr);
    int new_sock = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

    if (new_sock < 0) {
        if (errno == EINTR) {  // interrupção por sinal (Ctrl+C)
            break;
        } else if (errno == EBADF || errno == EINVAL) {
            // socket fechado pelo shutdown()
            LOG_INFO(&logger, "Socket principal encerrado, saindo do loop accept().");
            break;
        } else {
            LOG_ERROR(&logger, "Erro no accept: %s", strerror(errno));
            continue;
        }
    }
        if (sem_trywait(&client_slots) != 0) {
            const char *busy = "Servidor cheio. Tente novamente mais tarde.\n";
            send(new_sock, busy, strlen(busy), 0);
            close(new_sock);
            continue;
        }

        client_t *cli = malloc(sizeof(client_t));
        cli->socket = new_sock;
        cli->addr = client_addr;
        cli->active = 1;
        cli->username[0] = '\0';

        if (!clientlist_add(&client_list, cli)) {
            close(new_sock);
            free(cli);
            sem_post(&client_slots);
            continue;
        }

        pthread_create(&cli->thread_id, NULL, client_handler, (void *)cli);
        pthread_detach(cli->thread_id);
    }

    cleanup_server();
    return 0;
}

