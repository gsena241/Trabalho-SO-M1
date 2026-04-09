# Trabalho-SO-M1

**Grupo:** Kauã de Oliveira, Gustavo Sena, Arthur Fontana  
**Professor:** Felipe Viel — Engenharia da Computação — UNIVALI 2026/1

## Sobre o Projeto
Este projeto implementa um sistema de processamento digital de imagens (formato `.pgm`) em linguagem C, focado em utilizar conceitos avançados de **Sistemas Operacionais**, especificamente:
- **Comunicação Entre Processos (IPC):** Utilização de *Named Pipes (FIFOs)* para a transferência segura de dados entre processos isolados.
- **Concorrência e Paralelismo:** Uso de *POSIX Threads (pthreads)* para dividir o esforço computacional e acelerar o processamento da imagem.
- **Sincronização de Processos:** Implementação da arquitetura Produtor-Consumidor controlada por *Mutexes* e *Semáforos*.

## Arquitetura do Sistema
O sistema é composto por dois programas distintos que rodam simultaneamente:
1. **Sender (Emissor / Produtor):** Lê a imagem `.pgm` original do disco e injeta seus bytes no tubo (FIFO). Após o envio, o processo é encerrado.
2. **Worker (Trabalhador / Consumidor):** Fica bloqueado aguardando os dados do FIFO. Ao receber a imagem, a *thread* principal fatia o trabalho em blocos de 10 linhas e o distribui em uma fila circular para um *Thread Pool* (ex: 4 threads) aplicar o filtro simultaneamente.

## Como executar o código
O projeto foi desenvolvido para rodar de forma nativa em ambientes Linux. Abra o terminal na pasta raiz do repositório e compile os códigos com o `gcc`:

```bash
gcc worker.c -o worker -pthread
gcc sender.c -o sender
# divide a tela
./worker meu_fifo saida.pgm negativo 4
ou ./worker meu_fifo saida_slice2.pgm slice 0 50 4
./sender meu_fifo entrada.pgm
