// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// Variáveis globais
volatile sig_atomic_t client_running = 1;  // Controla se cliente está ativo
int sock = -1;                             // Socket de conexão

// Função chamada quando aperta Ctrl+C
void handle_sigint(int sig) {
    client_running = 0;                    // Marca para parar
    if (sock >= 0) {
        shutdown(sock, SHUT_RDWR);         // Fecha conexão
    }
}

// Thread que recebe mensagens do servidor
void *receive_handler(void *arg) {
    char buffer[BUFFER_SIZE];
    int bytes;
    
    // Fica recebendo mensagens enquanto estiver conectado
    while (client_running && (bytes = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';              // Termina a string
        printf("%s", buffer);              // Mostra mensagem
        fflush(stdout);                    // Força exibição imediata
    }
    
    // Servidor caiu ou erro
    if (bytes <= 0 && client_running) {
        printf("\n--- Conexão com o servidor perdida ---\n");
        client_running = 0;                // Para o cliente
    }
    
    return NULL;
}

int main() {
    struct sockaddr_in server_addr;
    pthread_t tid;
    char msg[BUFFER_SIZE];
    
    // Configura tratamento do Ctrl+C
    signal(SIGINT, handle_sigint);

    // Cria socket TCP
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    // Configura endereço do servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    // Converte IP de texto para binário
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Endereço inválido");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Conecta ao servidor
    printf("Conectando ao servidor...\n");
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Falha na conexão");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Conectado ao servidor! Digite /quit para sair.\n");
    printf("--------------------------------------------\n");

    // Cria thread para receber mensagens
    if (pthread_create(&tid, NULL, receive_handler, NULL) != 0) {
        perror("Erro ao criar thread de recebimento");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Loop principal: lê mensagens do usuário e envia
    while (client_running && fgets(msg, sizeof(msg), stdin) != NULL) {
        if (!client_running) break;
        
        // Remove enter do final da mensagem
        size_t len = strlen(msg);
        if (len > 0 && msg[len-1] == '\n') {
            msg[len-1] = '\0';
        }
        
        // Verifica se é comando de saída
        if (strcmp(msg, "/quit") == 0) {
            // Envia /quit com enter para o servidor
            strcat(msg, "\n");
            send(sock, msg, strlen(msg), 0);
            break;
        }
        
        // Adiciona enter para enviar mensagem normal
        if (len < BUFFER_SIZE - 1) {
            msg[len] = '\n';
            msg[len+1] = '\0';
        }
        
        // Envia mensagem para servidor
        if (send(sock, msg, strlen(msg), 0) < 0) {
            if (client_running) {
                perror("Erro ao enviar mensagem");
            }
            break;
        }
    }

    // Limpeza final
    client_running = 0;
    printf("Desconectando...\n");
    
    // Fecha conexão
    if (sock >= 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }
    
    // Para e espera thread de recebimento terminar
    pthread_cancel(tid);
    pthread_join(tid, NULL);
    
    printf("Cliente encerrado.\n");
    return 0;
}
