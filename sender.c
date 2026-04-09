/*
Processo Emissor — Trabalho M1 Sistemas Operacionais
Grupo: Kauã de Oliveira, Gustavo Sena, Arthur Fontana
Professor: Felipe Viel — Engenharia da Computação — UNIVALI 2026/1
PARA USAR VOCÊ 
gcc worker.c -o worker -pthread
gcc sender.c -o sender
divide a tela
./worker meu_fifo saida.pgm negativo 4
ou ./worker meu_fifo saida_slice2.pgm slice 0 50 4
./sender meu_fifo entrada.pgm
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
 
/* structs */
 
typedef struct {
    int w, h, maxv;
    unsigned char *data;
} PGM;
 
typedef struct {
    int w, h, maxv;
} Header;
 
/* Constantes */
 
#define FIFO_PERMS 0666
 
/* Leitura de imagem PGM binária (P5) */
 
int read_pgm(const char *path, PGM *img) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[sender] Erro: nao foi possivel abrir '%s'\n", path);
        return -1;
    }
 
    char magic[3];
    if (fscanf(f, "%2s", magic) != 1 || strcmp(magic, "P5") != 0) {
        fprintf(stderr, "[sender] Erro: formato invalido (esperado P5)\n");
        fclose(f);
        return -1;
    }
 
    /* Pula comentários */
    int c = fgetc(f);
    while (c == '#') {
        while (fgetc(f) != '\n');
        c = fgetc(f);
    }
    ungetc(c, f);
 
    if (fscanf(f, "%d %d %d", &img->w, &img->h, &img->maxv) != 3) {
        fprintf(stderr, "[sender] Erro: cabecalho PGM invalido\n");
        fclose(f);
        return -1;
    }
 
    fgetc(f); /* consome '\n' apos o maxval */
 
    size_t total = (size_t)(img->w * img->h);
    img->data = (unsigned char *)malloc(total);
    if (!img->data) {
        fprintf(stderr, "[sender] Erro: falha ao alocar memoria\n");
        fclose(f);
        return -1;
    }
 
    if (fread(img->data, 1, total, f) != total) {
        fprintf(stderr, "[sender] Erro: leitura incompleta dos pixels\n");
        free(img->data);
        fclose(f);
        return -1;
    }
 
    fclose(f);
    printf("[sender] Imagem carregada: %dx%d pixels\n", img->w, img->h);
    return 0;
}
  
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <fifo_path> <entrada.pgm>\n", argv[0]);
        return 1;
    }
 
    const char *fifo_path = argv[1];
    const char *img_path  = argv[2];
 
    /* Cria o FIFO — ignora erro se ja existir */
    if (mkfifo(fifo_path, FIFO_PERMS) == -1 && errno != EEXIST) {
        perror("[sender] Erro ao criar FIFO");
        return 1;
    }
 
    /* Le a imagem PGM do disco */
    PGM img;
    if (read_pgm(img_path, &img) != 0) {
        return 1;
    }
 
    /* Prepara cabecalho */
    Header hdr;
    hdr.w    = img.w;
    hdr.h    = img.h;
    hdr.maxv = img.maxv;
 
    /* Abre FIFO para escrita — bloqueia ate worker abrir para leitura */
    printf("[sender] Aguardando worker conectar ao FIFO '%s'...\n", fifo_path);
    int fd = open(fifo_path, O_WRONLY);
    if (fd == -1) {
        perror("[sender] Erro ao abrir FIFO para escrita");
        free(img.data);
        return 1;
    }
    printf("[sender] Conectado! Enviando dados...\n");
 
    /* Envia cabecalho com loop seguro */
    size_t enviado = 0;
    while (enviado < sizeof(Header)) {
        ssize_t w = write(fd, ((char *)&hdr) + enviado, sizeof(Header) - enviado);
        if (w <= 0) {
            perror("[sender] Erro ao enviar cabecalho");
            close(fd);
            free(img.data);
            return 1;
        }
        enviado += (size_t)w;
    }
 
    /* Envia pixels com loop seguro */
    size_t total = (size_t)(img.w * img.h);
    enviado = 0;
    while (enviado < total) {
        ssize_t w = write(fd, img.data + enviado, total - enviado);
        if (w <= 0) {
            perror("[sender] Erro ao enviar pixels");
            close(fd);
            free(img.data);
            return 1;
        }
        enviado += (size_t)w;
    }
 
    printf("[sender] Transmissao concluida: %zu bytes enviados.\n", total);
 
    /* Fecha e libera */
    close(fd);
    free(img.data);
 
    printf("[sender] Encerrado com sucesso.\n");
    return 0;
}