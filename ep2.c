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

int **pista = NULL;
Ciclista *ciclistas = NULL;
pthread_mutex_t pista_mutex;
pthread_mutex_t **controle_pista = NULL;

// Sincronização por tick
int barreira = 0;
int ciclistas_ativos = 0;
pthread_mutex_t barreira_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t barreira_cond = PTHREAD_COND_INITIALIZER;


int rodada_voltas = 2;
int ultimo_ciclista_id = -1;
pthread_mutex_t mutex_ultimo;

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
            printf("🚨 Último a completar %d voltas: Ciclista %d\n", rodada_voltas, ultimo_ciclista_id);
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
        if (id_na_frente == -1) {
            //printf("\t 🟢 Frente livre para o ciclista %d\n", c->id);
        } else {
            //printf("\t 🔴 Frente ocupada para o ciclista %d por ciclista %d\n", c->id, id_na_frente);
        }
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
        printf("⚙️ Ciclista %d definiu nova velocidade: %d km/h\n", c->id, c->velocidade);
    }
}


void verifica_volta_completada(Ciclista *c, int col_antiga, int col_nova) {
    if (col_nova == 0 && col_antiga == d - 1) {
        c->voltas_completadas++;

        verifica_ultimo(c);

        if (debug) {
            printf("🔁 Ciclista %d completou %d volta(s)\n",
                   c->id, c->voltas_completadas);
        }
        atualiza_velocidade(c);
    }
}

void barreira_sincroniza() {
    pthread_mutex_lock(&barreira_mutex);
    barreira++;
    if (barreira < ciclistas_ativos) {
        pthread_cond_wait(&barreira_cond, &barreira_mutex);
    } else {
        barreira = 0;
        pthread_cond_broadcast(&barreira_cond);
    }
    pthread_mutex_unlock(&barreira_mutex);
}


void verifica_quebra(Ciclista *c) {
    if (c->voltas_completadas > 0 && c->voltas_completadas % 5 == 0) {
        int sorteio = rand() % 100; // 0 a 99
        if (sorteio < 10) { // 10% de chance
            c->quebrado = 1;
            printf("💥 Ciclista %d QUEBROU na volta %d!\n",
                   c->id, c->voltas_completadas);
        } else if (debug) {
            printf("✅ Ciclista %d sobreviveu à verificação de quebra na volta %d\n",
                   c->id, c->voltas_completadas);
        }
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
                printf("🏃 Ciclista %d andou de [%d][%d] para [%d][%d]\n",
                       c->id, linha, col_atual, linha, col_prox);
            }

            moveu = 1;
        } else {
            if (debug) {
                printf("⛔ Ciclista %d bloqueado em [%d][%d], ocupado por %d\n",
                       c->id, linha, col_prox, pista[linha][col_prox]);
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
                printf("🏃 Ciclista %d andou de [%d][%d] para [%d][%d]\n",
                       c->id, linha, col_atual, linha, col_prox);
            }

            moveu = 1;
        } else {
            if (debug) {
                printf("⛔ Ciclista %d bloqueado em [%d][%d], ocupado por %d\n",
                       c->id, linha, col_prox, pista[linha][col_prox]);
            }
        }

        pthread_mutex_unlock(&controle_pista[linha][col_prox]);
        pthread_mutex_unlock(&controle_pista[linha][col_atual]);
    }

    return moveu;
}

void *logica_ciclista(void *arg) {
    Ciclista *c = (Ciclista *)arg;

    for (int tick = 0; tick < MAX_TICKS; tick++) {
        if (c->eliminado || c->quebrado) break;

        if (c->ticks_para_mover > 0) {
            c->ticks_para_mover--;
        } else {
            tenta_mover_para_frente(c);
        }

        barreira_sincroniza();
    }

    pthread_mutex_lock(&barreira_mutex);
    ciclistas_ativos--;


    
    
    // Caso ele era o último esperado, desbloqueia a barreira
    if (barreira == ciclistas_ativos) {
        barreira = 0;
        pthread_cond_broadcast(&barreira_cond);
    }
    
    pthread_mutex_unlock(&barreira_mutex);
    
    printf("🏁 Ciclista %d finalizou a simulação.\n", c->id);
    pthread_exit(NULL);
}    



void inicializa_threads_ciclistas() {
    // cria uma thread para cada um dos ciclistas
    for (int i = 0; i < k; i++) {
        pthread_create(&ciclistas[i].thread, NULL, logica_ciclista, &ciclistas[i]);
    }
}

void aguarda_threads_ciclistas() {
    for (int i = 0; i < k; i++) {
        pthread_join(ciclistas[i].thread, NULL);
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
    arrive = malloc(sizeof(int) * k);
    continue_flag = malloc(sizeof(int) * k);
    tick_mutexes = malloc(sizeof(pthread_mutex_t) * k);
    tick_conds = malloc(sizeof(pthread_cond_t) * k);

    for (int i = 0; i < k; i++) {
        arrive[i] = 0;
        continue_flag[i] = 0;
        pthread_mutex_init(&tick_mutexes[i], NULL);
        pthread_cond_init(&tick_conds[i], NULL);
    }
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



int main(int argc, char *argv[]) {

    d = atoi(argv[1]); // tamanho da pista
    k = atoi(argv[2]); // quantidade de ciclistas
    modo = argv[3][0]; // modo; i = ingenuo, e = eficiente
    // verifica se tem o valor do debug
    if (argc == 5 && strcmp(argv[4], "-debug") == 0) {
        debug = 1;
    }

    inicializa_pista();
    inicializa_mutexes();
    pthread_mutex_init(&mutex_ultimo, NULL);

    
    srand(time(NULL));
    inicializa_ciclistas();
    ciclistas_ativos = k;
    inicializa_sincronizacao();

    inicializa_threads_ciclistas();
    

    aguarda_threads_ciclistas();
    imprime_pista();

    libera_mutexes();
    libera_pista();

    for (int i = 0; i < k; i++) {
        pthread_mutex_destroy(&tick_mutexes[i]);
        pthread_cond_destroy(&tick_conds[i]);
    }

    free(arrive);
    free(continue_flag);
    free(tick_mutexes);
    free(tick_conds);
    

    free(ciclistas);
    pthread_mutex_destroy(&mutex_ultimo);

    
    return 0;
}