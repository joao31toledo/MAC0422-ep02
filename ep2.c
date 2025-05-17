#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "ep2.h"

// Variáveis globais
int d;
int k;
char modo;
int debug = 0;
int MAX_TICKS = 100;
int tick_global = 0;

int **pista = NULL;
Ciclista *ciclistas = NULL;
pthread_mutex_t pista_mutex;
pthread_mutex_t **controle_pista = NULL;

// Sincronização por tick
int ciclistas_ativos = 0;
pthread_mutex_t barreira_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex_print = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t tick_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tick_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t mutex_ultimo;

int tick_chegadas = 0;
int rodada_voltas = 2;
int ultimo_ciclista_id = -1;

void inicializa_pista()
{
    pista = malloc(sizeof(int *) * N_FAIXAS);

    for (int i = 0; i < N_FAIXAS; i++)
    {
        pista[i] = malloc(sizeof(int) * d);
        for (int j = 0; j < d; j++)
        {
            pista[i][j] = -1;
        }
    }
}

void inicializa_mutexes() {
    if (modo == 'i') {
        if (pthread_mutex_init(&pista_mutex, NULL) != 0) {
            perror("Erro ao inicializar mutex global");
            exit(EXIT_FAILURE);
        }
    } else if (modo == 'e') {
        controle_pista = malloc(N_FAIXAS * sizeof(pthread_mutex_t *));
        if (!controle_pista) {
            perror("Erro ao alocar controle_pista");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < N_FAIXAS; i++) {
            controle_pista[i] = malloc(d * sizeof(pthread_mutex_t));
            if (!controle_pista[i]) {
                perror("Erro ao alocar linha de mutexes");
                exit(EXIT_FAILURE);
            }

            for (int j = 0; j < d; j++) {
                if (pthread_mutex_init(&controle_pista[i][j], NULL) != 0) {
                    perror("Erro ao inicializar mutex individual");
                    exit(EXIT_FAILURE);
                }
            }
        }
    } else {
        fprintf(stderr, "Modo inválido em inicializa_mutexes: '%c'\n", modo);
        exit(EXIT_FAILURE);
    }
}

void inicializa_ciclistas()
{
    ciclistas = malloc(sizeof(Ciclista) * k);

    for (int i = 0; i < k; i++) {
        ciclistas[i].id = i + 1;
        ciclistas[i].velocidade = 30;
        ciclistas[i].velocidade_anterior = 30;
        ciclistas[i].ticks_para_mover = 2; // 30km/h = 120ms = 2 ticks
        ciclistas[i].voltas_completadas = 0;
        ciclistas[i].quebrado = 0;
        ciclistas[i].eliminado = 0;

        pthread_mutex_init(&ciclistas[i].mutex, NULL);
        
        // 🎯 Sorteia posição de largada livre na pista
        sorteia_largada(&ciclistas[i]);
    }
}

void verifica_ultimo(Ciclista *c) {
    pthread_mutex_lock(&mutex_ultimo);

    // Se o ciclista atingiu a rodada atual
    if (c->voltas_completadas == rodada_voltas) {
        ultimo_ciclista_id = c->id;

        // Verifica se todos atingiram essa rodada
        int todos_atingiram = 1;
        for (int i = 0; i < k; i++) {
            if (!ciclistas[i].quebrado && !ciclistas[i].eliminado && ciclistas[i].voltas_completadas < rodada_voltas) {
                todos_atingiram = 0;
                break;
            }
        }

        // Se sim, exibe o último e prepara a próxima rodada
        if (todos_atingiram) {
            pthread_mutex_lock(&mutex_print); // 🆕 sincroniza print
            printf("🚨 Último a completar %d voltas: Ciclista %d\n", rodada_voltas, ultimo_ciclista_id);
            pthread_mutex_unlock(&mutex_print);

            rodada_voltas += 2;
            ultimo_ciclista_id = -1;
        }
    }

    pthread_mutex_unlock(&mutex_ultimo);
}

int verifica_frente(Ciclista *c) {
    int linha = c->linha_pista;
    int col_atual = c->coluna_pista;
    int col_prox = (col_atual + 1) % d;
    int id_na_frente;

    if (modo == 'i') {
        pthread_mutex_lock(&pista_mutex);
        id_na_frente = pista[linha][col_prox];
        pthread_mutex_unlock(&pista_mutex);
    } else {
        pthread_mutex_lock(&controle_pista[linha][col_prox]);
        id_na_frente = pista[linha][col_prox];
        pthread_mutex_unlock(&controle_pista[linha][col_prox]);
    }

    if (debug) {
        pthread_mutex_lock(&mutex_print); // 🆕 sincroniza print
        if (id_na_frente == -1) {
            printf("\t 🟢 Frente livre para o ciclista %d\n", c->id);
        } else {
            printf("\t 🔴 Frente ocupada para o ciclista %d por ciclista %d\n", c->id, id_na_frente);
        }
        pthread_mutex_unlock(&mutex_print);
    }

    return id_na_frente;
}

void atualiza_velocidade(Ciclista *c) {
    int r = rand() % 100;

    if (c->velocidade_anterior == 30) {
        // 75% para 60, 25% para 30
        c->velocidade = (r < 75) ? 60 : 30;
    } else {
        // 45% para 60, 55% para 30
        c->velocidade = (r < 45) ? 60 : 30;
    }

    c->ticks_para_mover = (c->velocidade == 60) ? 1 : 2;
    c->velocidade_anterior = c->velocidade;

    if (debug) {
        pthread_mutex_lock(&mutex_print); // 🆕 sincroniza print
        printf("⚙️ Ciclista %d definiu nova velocidade: %d km/h\n", c->id, c->velocidade);
        pthread_mutex_unlock(&mutex_print);
    }
}

void aguarda_tick() {
    pthread_mutex_lock(&tick_mutex);
    tick_chegadas++;

    if (tick_chegadas == ciclistas_ativos) {
        pthread_cond_signal(&tick_cond);  // Acorda o coordenador
    }

    pthread_cond_wait(&tick_cond, &tick_mutex); // Espera o coordenador liberar
    pthread_mutex_unlock(&tick_mutex);
}

void libera_tick() {
    pthread_mutex_lock(&tick_mutex);
    tick_chegadas = 0;
    tick_global++;
    pthread_cond_broadcast(&tick_cond);  // Libera todos os ciclistas
    pthread_mutex_unlock(&tick_mutex);
}

void verifica_volta_completada(Ciclista *c, int col_antiga, int col_nova) {
    if (col_nova == 0 && col_antiga == d - 1) {
        c->voltas_completadas++;

        verifica_ultimo(c);

        if (debug) {
            pthread_mutex_lock(&mutex_print); // 🆕 sincroniza print
            printf("🔁 Ciclista %d completou %d volta(s)\n",
                   c->id, c->voltas_completadas);
            pthread_mutex_unlock(&mutex_print);
        }
        atualiza_velocidade(c);
    }
}

void verifica_quebra(Ciclista *c) {
    if (c->voltas_completadas > 0 && c->voltas_completadas % 5 == 0) {
        int sorteio = rand() % 100;
        if (sorteio < 10) {
            pthread_mutex_lock(&mutex_print); // 🆕 sincroniza print
            printf("💥 Ciclista %d QUEBROU na volta %d!\n", c->id, c->voltas_completadas);
            pthread_mutex_unlock(&mutex_print);

            c->quebrado = 1;

            pthread_mutex_lock(&barreira_mutex);
            ciclistas_ativos--;
            pthread_mutex_unlock(&barreira_mutex);
        }
    }
}

// 🆕 Função auxiliar para ordenação do ranking
int comparar_ciclistas(const void *a, const void *b) {
    Ciclista *c1 = (Ciclista *)a;
    Ciclista *c2 = (Ciclista *)b;

    if (c1->voltas_completadas != c2->voltas_completadas)
        return c2->voltas_completadas - c1->voltas_completadas;

    return c1->coluna_pista - c2->coluna_pista; // desempate por proximidade da linha de chegada
}

// 🆕 Exibe ranking parcial
void imprime_ranking() {
    Ciclista *ativos = malloc(sizeof(Ciclista) * k);
    int n = 0;

    for (int i = 0; i < k; i++) {
        if (!ciclistas[i].quebrado && !ciclistas[i].eliminado) {
            ativos[n++] = ciclistas[i];
        }
    }

    qsort(ativos, n, sizeof(Ciclista), comparar_ciclistas);

    fprintf(stderr, "\n🏆 Ranking parcial (tick %d):\n", tick_global);
    for (int i = 0; i < n; i++) {
        fprintf(stderr, "%dº - Ciclista %d (voltas: %d, pos: [%d][%d])\n",
                i + 1, ativos[i].id, ativos[i].voltas_completadas,
                ativos[i].linha_pista, ativos[i].coluna_pista);
    }

    free(ativos);
}

void verifica_eliminacao(Ciclista *c) {
    static int voltas_alvo = 2;

    if (c->voltas_completadas >= voltas_alvo) {
        pthread_mutex_lock(&mutex_ultimo);

        static int contabilizados = 0;
        static int ultimo_id = -1;

        if (!c->eliminado && !c->quebrado && !c->ja_contabilizado_na_rodada) {
            contabilizados++;
            c->ja_contabilizado_na_rodada = 1;
            ultimo_id = c->id;
        }

        if (contabilizados == ciclistas_ativos) {
            for (int i = 0; i < k; i++) {
                if (ciclistas[i].id == ultimo_id) {
                    pthread_mutex_lock(&barreira_mutex);
                    ciclistas[i].eliminado = 1;
                    ciclistas_ativos--;
                    pthread_mutex_unlock(&barreira_mutex);

                    pthread_mutex_lock(&mutex_print); // 🆕 sincroniza print
                    printf("☠️ Ciclista %d ELIMINADO após %d voltas\n", ultimo_id, voltas_alvo);
                    pthread_mutex_unlock(&mutex_print);

                    pthread_detach(ciclistas[i].thread);  // liberando recursos da thread
                    break;
                }
            }

            // Reset para próxima rodada
            contabilizados = 0;
            voltas_alvo += 2;
            for (int i = 0; i < k; i++) {
                ciclistas[i].ja_contabilizado_na_rodada = 0;
            }
        }

        pthread_mutex_unlock(&mutex_ultimo);
    }
}

int tenta_mover_para_frente(Ciclista *c) {
    int linha = c->linha_pista;
    int col_atual = c->coluna_pista;
    int col_prox = (col_atual + 1) % d;
    int moveu = 0;

    if (modo == 'i') {
        pthread_mutex_lock(&pista_mutex);

        if (pista[linha][col_prox] == -1) {
            // Atualiza pista
            pista[linha][col_atual] = -1;
            pista[linha][col_prox] = c->id;

            // Atualiza posição na struct
            c->coluna_pista = col_prox;

            // Reseta ticks para mover de novo
            c->ticks_para_mover = (c->velocidade == 30) ? 2 : 1;

            if (debug) {
                pthread_mutex_lock(&mutex_print); // 🆕 sincroniza print
                printf("🏃 Ciclista %d andou de [%d][%d] para [%d][%d]\n",
                       c->id, linha, col_atual, linha, col_prox);
                pthread_mutex_unlock(&mutex_print);
            }

            moveu = 1;
        } else {
            if (debug) {
                pthread_mutex_lock(&mutex_print); // 🆕 sincroniza print
                printf("⛔ Ciclista %d bloqueado em [%d][%d], ocupado por %d\n",
                       c->id, linha, col_prox, pista[linha][col_prox]);
                pthread_mutex_unlock(&mutex_print);
            }
        }

        pthread_mutex_unlock(&pista_mutex);

    } else {
        // Modo eficiente: tranca duas posições
        pthread_mutex_lock(&controle_pista[linha][col_atual]);
        pthread_mutex_lock(&controle_pista[linha][col_prox]);

        if (pista[linha][col_prox] == -1) {
            pista[linha][col_atual] = -1;
            pista[linha][col_prox] = c->id;

            int col_antiga = c->coluna_pista;
            c->coluna_pista = col_prox;
            verifica_volta_completada(c, col_antiga, col_prox);
            verifica_quebra(c);

            c->ticks_para_mover = (c->velocidade == 30) ? 2 : 1;

            if (debug) {
                pthread_mutex_lock(&mutex_print); // 🆕 sincroniza print
                printf("🏃 Ciclista %d andou de [%d][%d] para [%d][%d]\n",
                       c->id, linha, col_atual, linha, col_prox);
                pthread_mutex_unlock(&mutex_print);
            }

            moveu = 1;
        } else {
            if (debug) {
                pthread_mutex_lock(&mutex_print); // 🆕 sincroniza print
                printf("⛔ Ciclista %d bloqueado em [%d][%d], ocupado por %d\n",
                       c->id, linha, col_prox, pista[linha][col_prox]);
                pthread_mutex_unlock(&mutex_print);
            }
        }

        pthread_mutex_unlock(&controle_pista[linha][col_prox]);
        pthread_mutex_unlock(&controle_pista[linha][col_atual]);
    }

    return moveu;
}

void *logica_ciclista(void *arg) {
    Ciclista *c = (Ciclista *)arg;

    pthread_mutex_lock(&mutex_print);
    printf("▶️ Ciclista %d iniciou a thread.\n", c->id);
    pthread_mutex_unlock(&mutex_print);

    if (c->eliminado || c->quebrado) {
        pthread_mutex_lock(&mutex_print);
        printf("⏹️ Ciclista %d já eliminado ou quebrado ao iniciar.\n", c->id);
        pthread_mutex_unlock(&mutex_print);
        return NULL;
    }

    for (int tick = 0; tick < MAX_TICKS; tick++) {
        if (c->eliminado || c->quebrado)
            break;

        if (c->ticks_para_mover > 0) {
            c->ticks_para_mover--;
        } else {
            tenta_mover_para_frente(c);
            if (!c->quebrado && !c->eliminado) {
                verifica_eliminacao(c);
                verifica_quebra(c);
            }
        }

        if (!c->eliminado && !c->quebrado) {
            aguarda_tick();
        } else {
            break;
        }
    }

    pthread_mutex_lock(&barreira_mutex);
    ciclistas_ativos--;

    pthread_mutex_unlock(&barreira_mutex);

    pthread_mutex_lock(&mutex_print);
    printf("🏁 Ciclista %d finalizou a simulação.\n", c->id);
    pthread_mutex_unlock(&mutex_print);

    c->terminou = 1;  // marca que pode ser join
    pthread_exit(NULL);
}

void inicializa_threads_ciclistas() {
    // cria uma thread para cada um dos ciclistas
    for (int i = 0; i < k; i++) {
        pthread_create(&ciclistas[i].thread, NULL, logica_ciclista, &ciclistas[i]);
    }
}

void sorteia_largada(Ciclista *c)
{
    int lin, col;
    int achou_lugar = 0;

    while (!achou_lugar) {
        lin = rand() % N_FAIXAS;
        col = rand() % d;

        if (modo == 'i') {
            pthread_mutex_lock(&pista_mutex);
            if (pista[lin][col] == -1) {
                pista[lin][col] = c->id;
                achou_lugar = 1;
            }
            pthread_mutex_unlock(&pista_mutex);
        } else {
            pthread_mutex_lock(&controle_pista[lin][col]);
            if (pista[lin][col] == -1) {
                pista[lin][col] = c->id;
                achou_lugar = 1;
            }
            pthread_mutex_unlock(&controle_pista[lin][col]);
        }
    }

    c->linha_pista = lin;
    c->coluna_pista = col;
}

void inicializa_sincronizacao() {
    // removido pois não utilizado no código atual
}

void libera_pista() {
    if (pista) {
        for (int i = 0; i < N_FAIXAS; i++) {
            free(pista[i]);
        }
        free(pista);
        pista = NULL;
    }
}

void libera_mutexes() {
    if (modo == 'i') {
        pthread_mutex_destroy(&pista_mutex);
    } else if (modo == 'e' && controle_pista) {
        for (int i = 0; i < N_FAIXAS; i++) {
            for (int j = 0; j < d; j++) {
                pthread_mutex_destroy(&controle_pista[i][j]);
            }
            free(controle_pista[i]);
        }
        free(controle_pista);
        controle_pista = NULL;
    }
}

void imprime_pista() {
    fprintf(stderr, "\nEstado atual da pista (faixa x posição):\n");

    if (modo == 'i') {
        pthread_mutex_lock(&pista_mutex);
    }

    for (int faixa = 0; faixa < N_FAIXAS; faixa++) {
        fprintf(stderr, "Faixa %2d: ", faixa);
        for (int pos = 0; pos < d; pos++) {
            int id = pista[faixa][pos];
            if (id == -1) {
                fprintf(stderr, ". ");
            } else {
                fprintf(stderr, "%d ", id);
            }
        }
        fprintf(stderr, "\n");
    }

    if (modo == 'i') {
        pthread_mutex_unlock(&pista_mutex);
    }

    fprintf(stderr, "--------------------------------------------------\n");
}

void *coordenador(void *arg) {
    while (1) {
        pthread_mutex_lock(&barreira_mutex);
        if (ciclistas_ativos <= 1) {
            pthread_mutex_unlock(&barreira_mutex);
            break;
        }
        pthread_mutex_unlock(&barreira_mutex);

        pthread_mutex_lock(&mutex_print); // 🆕 sincroniza prints
        printf("\n=== TICK %d ===\n", tick_global); // 🆕 imprime tick
        imprime_pista();                             // 🆕 imprime pista atual
        imprime_ranking();                           // 🆕 imprime ranking atual
        pthread_mutex_unlock(&mutex_print); // 🆕

        libera_tick(); // sincroniza threads para próximo tick
    }

    // última liberação para destravar threads ainda presas
    libera_tick();
    return NULL;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0); // Desabilita buffering de stdout

    d = atoi(argv[1]); // tamanho da pista
    k = atoi(argv[2]); // quantidade de ciclistas
    modo = argv[3][0]; // modo; i = ingenuo, e = eficiente
    if (argc == 5 && strcmp(argv[4], "-debug") == 0) {
        debug = 1;
    }

    inicializa_pista();
    inicializa_mutexes();

    srand(time(NULL));
    inicializa_ciclistas();
    ciclistas_ativos = k;

    pthread_mutex_init(&barreira_mutex, NULL);
    pthread_cond_init(&tick_cond, NULL);
    pthread_mutex_init(&tick_mutex, NULL);
    pthread_mutex_init(&mutex_print, NULL);
    pthread_mutex_init(&mutex_ultimo, NULL);

    inicializa_threads_ciclistas();

    pthread_t thread_coordenador;
    pthread_create(&thread_coordenador, NULL, coordenador, NULL);

    pthread_join(thread_coordenador, NULL);

    for (int i = 0; i < k; i++) {
        if (ciclistas[i].terminou) {
            pthread_join(ciclistas[i].thread, NULL);
        }
    }

    libera_mutexes();
    libera_pista();

    pthread_mutex_destroy(&barreira_mutex);
    pthread_mutex_destroy(&tick_mutex);
    pthread_cond_destroy(&tick_cond);
    pthread_mutex_destroy(&mutex_print);
    pthread_mutex_destroy(&mutex_ultimo);

    free(ciclistas);

    return 0;
}
