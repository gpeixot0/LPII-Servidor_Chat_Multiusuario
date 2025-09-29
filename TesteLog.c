#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "libtslog.h"

#define NUM_THREADS 10  // número de threads simuladas

Logger logger;

// Função que cada thread vai executar
void* test_function(void* arg) {
    int id = *((int*)arg);

    // Cada thread faz alguns logs
    LOG_INFO(&logger, "Thread %d iniciando", id);
    LOG_DEBUG(&logger, "Thread %d debug", id);
    LOG_WARN(&logger, "Thread %d aviso", id);
    LOG_ERROR(&logger, "Thread %d encontrou erro", id);

    return NULL;
}

int main() {
    // Inicializa o logger com nível DEBUG
    if (log_init(&logger, LOG_DEBUG) != 0) {
        fprintf(stderr, "Falha ao inicializar logger\n");
        return 1;
    }

    pthread_t threads[NUM_THREADS];
    int ids[NUM_THREADS];

    // Cria múltiplas threads
    for (int i = 0; i < NUM_THREADS; i++) {
        ids[i] = i + 1;
        if (pthread_create(&threads[i], NULL, test_function, &ids[i]) != 0) {
            fprintf(stderr, "Falha ao criar thread %d\n", i + 1);
        }
    }

    // Espera todas as threads terminarem
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Fecha o logger
    log_close(&logger);

    return 0;
}

