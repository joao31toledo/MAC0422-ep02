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

int **pista = NULL;
Ciclista *ciclistas = NULL;
pthread_mutex_t pista_mutex;
pthread_mutex_t **controle_pista = NULL;

// Sincronização por tick
int *arrive;
int *continue_flag;
pthread_mutex_t *tick_mutexes;
pthread_cond_t *tick_conds;

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


void *logica_ciclista(void *arg) {
    Ciclista *c = (Ciclista *)arg;
    int id = c->id - 1;

    for (int tick = 0; tick < 10; tick++) {
        printf("🟡 Ciclista %d pronto para o tick %d\n", c->id, tick);

        pthread_mutex_lock(&tick_mutexes[id]);
        while (!continue_flag[id]) {
            printf("⏸️ Ciclista %d esperando coordenador (tick %d)\n", c->id, tick);
            pthread_cond_wait(&tick_conds[id], &tick_mutexes[id]);
        }
        continue_flag[id] = 0;
        pthread_mutex_unlock(&tick_mutexes[id]);

        printf("🚴 Ciclista %d executando tick %d\n", c->id, tick);

        pthread_mutex_lock(&tick_mutexes[id]);
        arrive[id] = 1;
        pthread_cond_signal(&tick_conds[id]);
        pthread_mutex_unlock(&tick_mutexes[id]);
    }

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

void *coordenador_tick(void *arg) {

    // 🔓 Libera o primeiro tick antes de começar a esperar
    for (int i = 0; i < k; i++) {
        pthread_mutex_lock(&tick_mutexes[i]);
        continue_flag[i] = 1;
        pthread_cond_signal(&tick_conds[i]);
        pthread_mutex_unlock(&tick_mutexes[i]);
    }

    for (int tick = 0; tick < 10; tick++) {
        printf("\n⏱️ Coordenador aguardando tick %d...\n", tick);

        for (int i = 0; i < k; i++) {
            pthread_mutex_lock(&tick_mutexes[i]);
            while (arrive[i] == 0) {
                pthread_cond_wait(&tick_conds[i], &tick_mutexes[i]);
            }
            // já está com mutex travado, pode resetar
            arrive[i] = 0;
            pthread_mutex_unlock(&tick_mutexes[i]);
        }

        printf("🟢 Tick %d finalizado. Liberando próximo tick...\n", tick);

        for (int i = 0; i < k; i++) {
            pthread_mutex_lock(&tick_mutexes[i]);
            continue_flag[i] = 1;
            pthread_cond_signal(&tick_conds[i]);
            pthread_mutex_unlock(&tick_mutexes[i]);
        }
    }

    pthread_exit(NULL);
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

    
    srand(time(NULL));
    inicializa_ciclistas();
    inicializa_sincronizacao();

    inicializa_threads_ciclistas();
    
    pthread_t coordenador;
    pthread_create(&coordenador, NULL, coordenador_tick, NULL);
    pthread_join(coordenador, NULL);

    aguarda_threads_ciclistas();
    imprime_pista();
    // inicializa_mutexes();

    printf("✅ Pista de %d metros criada com sucesso!\n", d);
    printf("🚴‍♂️ %d ciclistas configurados.\n", k);
    printf("🔒 Controle de pista: %s\n", (modo == 'i') ? "INGÊNUO (mutex único)" : "EFICIENTE (mutex por célula)");

    if (debug) {
        fprintf(stderr, "🛠️  Modo DEBUG ativado!\n");
    }

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
    
    return 0;
}