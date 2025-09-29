#ifndef LIBTSLOG_H
#define LIBTSLOG_H

#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <stdarg.h>

// ===== Macros de conveniência =====
#define LOG_DEBUG(logger, msg, ...) log(logger, LOG_DEBUG, msg, ##__VA_ARGS__)
#define LOG_INFO(logger, msg, ...)  log(logger, LOG_INFO,  msg, ##__VA_ARGS__)
#define LOG_WARN(logger, msg, ...)  log(logger, LOG_WARN,  msg, ##__VA_ARGS__)
#define LOG_ERROR(logger, msg, ...) log(logger, LOG_ERROR, msg, ##__VA_ARGS__)

// ===== Níveis de log =====
typedef enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR } log_level_t;

// ===== Estrutura do logger =====
typedef struct {
    pthread_mutex_t mutex;    // Mutex para thread safety
    log_level_t level;        // Nível mínimo de log
} Logger;

// ===== Inicializa o logger =====
static int log_init(Logger *logger, log_level_t level) {
    if (!logger) return -1;
    logger->level = level;
    return pthread_mutex_init(&logger->mutex, NULL);
}

// ===== Fecha o logger =====
static void log_close(Logger *logger) {
    if (!logger) return;
    pthread_mutex_destroy(&logger->mutex);
}

// ===== Função interna: timestamp =====
static void log_get_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buf, size, "%02d-%02d-%04d %02d:%02d:%02d",
             t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
             t->tm_hour, t->tm_min, t->tm_sec);
}

// ===== Função de log thread-safe =====
static void log(Logger *logger, log_level_t level, const char *fmt, ...) {
    if (!logger || level < logger->level) return;

    pthread_mutex_lock(&logger->mutex);

    char ts[20];
    log_get_timestamp(ts, sizeof(ts));
    const char *level_str[] = {"DEBUG", "INFO", "WARN", "ERROR"};

    // Cabeçalho do log
    fprintf(stderr, "[%s] [%s] [Thread %lu] ", ts, level_str[level], (unsigned long)pthread_self());

    // Mensagem formatada
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n"); 

    pthread_mutex_unlock(&logger->mutex);
}

#endif // LIBTSLOG_H

