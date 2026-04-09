/*
Processo Trabalhador — Trabalho M1 Sistemas Operacionais
Grupo: Kauã de Oliveira, Gustavo Sena, Arthur Fontana
Professor: Felipe Viel — Engenharia da Computação — UNIVALI 2026/1
PARA USAR VOCÊ 
gcc worker.c -o worker -pthread
gcc sender.c -o sender
divide a tela
./worker meu_fifo saida.pgm negativo 4
./sender meu_fifo entrada.pgm
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
 
/* ===== Constantes ===== */
 
#define MODE_NEG   0
#define MODE_SLICE 1
#define QMAX       128
#define FIFO_PERMS 0666
#define BLOCO      10   /* linhas por tarefa — melhor balanceamento entre threads */
 
/* ===== Estruturas ===== */
 
typedef struct {
    int w, h, maxv;
    unsigned char *data;
} PGM;
 
typedef struct {
    int w, h, maxv;
} Header;
 
typedef struct {
    int row_start;
    int row_end;
} Task;
 
/* ===== Fila circular de tarefas ===== */
 
Task            queue_buf[QMAX];
int             q_head = 0;
int             q_tail = 0;
pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;
sem_t           sem_items;  /* quantas tarefas disponiveis */
sem_t           sem_space;  /* quantos espacos livres na fila */
 
/* ===== Dados globais compartilhados pelas threads ===== */
 
PGM g_in, g_out;
int g_mode     = MODE_NEG;
int g_t1       = 0;
int g_t2       = 255;
int g_nthreads = 4;
 
/* ===== Utilitario: medicao de tempo ===== */
 
double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}
 
/* ===== Fila: enqueue (produtor) ===== */
 
void enqueue(Task t) {
    sem_wait(&sem_space);           /* bloqueia se fila cheia */
    pthread_mutex_lock(&q_lock);    /* regiao critica */
 
    queue_buf[q_tail] = t;
    q_tail = (q_tail + 1) % QMAX;
 
    pthread_mutex_unlock(&q_lock);
    sem_post(&sem_items);           /* avisa que ha nova tarefa */
}
 
/* ===== Fila: dequeue (consumidor) ===== */
 
Task dequeue(void) {
    sem_wait(&sem_items);           /* bloqueia se fila vazia */
    pthread_mutex_lock(&q_lock);    /* regiao critica */
 
    Task t = queue_buf[q_head];
    q_head = (q_head + 1) % QMAX;
 
    pthread_mutex_unlock(&q_lock);
    sem_post(&sem_space);           /* libera espaco na fila */
    return t;
}
 
/* ===== Filtro Negativo: s = 255 - r ===== */
 
void apply_negative(int row_start, int row_end) {
    for (int i = row_start; i < row_end; i++) {
        for (int j = 0; j < g_in.w; j++) {
            int idx = i * g_in.w + j;
            g_out.data[idx] = (unsigned char)(255 - g_in.data[idx]);
        }
    }
}
 
/* ===== Filtro Slice (Limiarizacao com Fatiamento) =====
 * Conforme enunciado:
 *   se pixel <= t1 OU pixel >= t2  -> 255 (branco)
 *   se t1 < pixel < t2             -> mantem valor original
 */
 
void apply_slice(int row_start, int row_end) {
    for (int i = row_start; i < row_end; i++) {
        for (int j = 0; j < g_in.w; j++) {
            int idx = i * g_in.w + j;
            unsigned char p = g_in.data[idx];
 
            if (p <= g_t1 || p >= g_t2)
                g_out.data[idx] = 255;
            else
                g_out.data[idx] = p;
        }
    }
}
 
/* ===== Funcao executada por cada thread ===== */
 
void *worker_thread(void *arg) {
    (void)arg;
 
    while (1) {
        Task t = dequeue();
 
        /* Sentinela de encerramento */
        if (t.row_start == -1)
            break;
 
        if (g_mode == MODE_NEG)
            apply_negative(t.row_start, t.row_end);
        else
            apply_slice(t.row_start, t.row_end);
    }
 
    return NULL;
}
 
/* ===== Main ===== */
 
int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <fifo> <saida.pgm> <negativo|slice> [t1 t2] [nthreads]\n", argv[0]);
        fprintf(stderr, "Ex:  %s /tmp/imgpipe saida.pgm negativo 4\n", argv[0]);
        fprintf(stderr, "Ex:  %s /tmp/imgpipe saida.pgm slice 50 100 4\n", argv[0]);
        return 1;
    }
 
    const char *fifo_path = argv[1];
    const char *out_path  = argv[2];
 
    /* Parse do modo e argumentos */
    if (strcmp(argv[3], "negativo") == 0) {
        g_mode     = MODE_NEG;
        g_nthreads = (argc >= 5) ? atoi(argv[4]) : 4;
 
    } else if (strcmp(argv[3], "slice") == 0) {
        if (argc < 6) {
            fprintf(stderr, "[worker] Erro: slice requer t1 e t2\n");
            return 1;
        }
        g_mode = MODE_SLICE;
        g_t1   = atoi(argv[4]);
        g_t2   = atoi(argv[5]);
 
        if (g_t1 < 0 || g_t2 > 255 || g_t1 >= g_t2) {
            fprintf(stderr, "[worker] Erro: limites invalidos (necessario 0 <= t1 < t2 <= 255)\n");
            return 1;
        }
 
        g_nthreads = (argc >= 7) ? atoi(argv[6]) : 4;
 
    } else {
        fprintf(stderr, "[worker] Erro: modo invalido '%s'. Use 'negativo' ou 'slice'.\n", argv[3]);
        return 1;
    }
 
    if (g_nthreads < 1) g_nthreads = 1;
 
    printf("[worker] Modo: %s | Threads: %d\n",
           g_mode == MODE_NEG ? "negativo" : "slice", g_nthreads);
    if (g_mode == MODE_SLICE)
        printf("[worker] Intervalo de preservacao: (%d, %d)\n", g_t1, g_t2);
 
    /* Cria FIFO se nao existir e abre para leitura */
    if (mkfifo(fifo_path, FIFO_PERMS) == -1 && errno != EEXIST) {
        perror("[worker] Erro ao criar FIFO");
        return 1;
    }
 
    printf("[worker] Aguardando sender no FIFO '%s'...\n", fifo_path);
    int fd = open(fifo_path, O_RDONLY);
    if (fd == -1) {
        perror("[worker] Erro ao abrir FIFO para leitura");
        return 1;
    }
    printf("[worker] Conectado! Recebendo dados...\n");
 
    /* Le cabecalho com loop seguro */
    Header hdr;
    size_t rec = 0;
    while (rec < sizeof(Header)) {
        ssize_t r = read(fd, ((char *)&hdr) + rec, sizeof(Header) - rec);
        if (r <= 0) {
            perror("[worker] Erro ao ler cabecalho");
            close(fd);
            return 1;
        }
        rec += (size_t)r;
    }
 
    g_in.w    = hdr.w;
    g_in.h    = hdr.h;
    g_in.maxv = hdr.maxv;
    printf("[worker] Imagem: %dx%d pixels\n", g_in.w, g_in.h);
 
    size_t total = (size_t)(g_in.w * g_in.h);
 
    g_in.data  = (unsigned char *)malloc(total);
    g_out.data = (unsigned char *)malloc(total);
    if (!g_in.data || !g_out.data) {
        fprintf(stderr, "[worker] Erro: falha de alocacao de memoria\n");
        close(fd);
        return 1;
    }
 
    /* Le pixels com loop seguro */
    rec = 0;
    while (rec < total) {
        ssize_t r = read(fd, g_in.data + rec, total - rec);
        if (r <= 0) {
            perror("[worker] Erro ao ler pixels");
            close(fd);
            free(g_in.data);
            free(g_out.data);
            return 1;
        }
        rec += (size_t)r;
    }
    close(fd);
    printf("[worker] %zu bytes recebidos.\n", rec);
 
    /* Configura saida */
    g_out.w    = g_in.w;
    g_out.h    = g_in.h;
    g_out.maxv = g_in.maxv;
 
    /* Inicializa semaforos */
    sem_init(&sem_items, 0, 0);
    sem_init(&sem_space, 0, QMAX);
 
    /* Cria pool de threads */
    pthread_t *threads = (pthread_t *)malloc((size_t)g_nthreads * sizeof(pthread_t));
    if (!threads) {
        fprintf(stderr, "[worker] Erro: falha ao alocar threads\n");
        free(g_in.data);
        free(g_out.data);
        return 1;
    }
 
    printf("[worker] Iniciando processamento com %d thread(s)...\n", g_nthreads);
    double t_inicio = get_time_ms();
 
    for (int i = 0; i < g_nthreads; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, NULL) != 0) {
            perror("[worker] Erro ao criar thread");
            free(threads);
            free(g_in.data);
            free(g_out.data);
            return 1;
        }
    }
 
    /* Produz tarefas em blocos de BLOCO linhas — melhor balanceamento */
    for (int i = 0; i < g_in.h; i += BLOCO) {
        Task t;
        t.row_start = i;
        t.row_end   = (i + BLOCO > g_in.h) ? g_in.h : i + BLOCO;
        enqueue(t);
    }
 
    /* Envia sentinela para cada thread encerrar */
    for (int i = 0; i < g_nthreads; i++) {
        Task sentinela;
        sentinela.row_start = -1;
        sentinela.row_end   = -1;
        enqueue(sentinela);
    }
 
    /* Aguarda todas as threads finalizarem */
    for (int i = 0; i < g_nthreads; i++)
        pthread_join(threads[i], NULL);
 
    double t_fim = get_time_ms();
    printf("[worker] Processamento concluido em %.2f ms\n", t_fim - t_inicio);
 
    /* Salva imagem de saida */
    FILE *f = fopen(out_path, "wb");
    if (!f) {
        fprintf(stderr, "[worker] Erro: nao foi possivel salvar '%s'\n", out_path);
    } else {
        fprintf(f, "P5\n%d %d\n%d\n", g_out.w, g_out.h, g_out.maxv);
        fwrite(g_out.data, 1, total, f);
        fclose(f);
        printf("[worker] Imagem salva em '%s'\n", out_path);
    }
 
    /* Libera recursos */
    sem_destroy(&sem_items);
    sem_destroy(&sem_space);
    pthread_mutex_destroy(&q_lock);
    free(threads);
    free(g_in.data);
    free(g_out.data);
 
    printf("[worker] Encerrado com sucesso.\n");
    return 0;
}

