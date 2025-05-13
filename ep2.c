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

void inicializa_pista()
{
    pista = malloc(sizeof(int *) * d);

    for (int i = 0; i < d; i++)
    {
        pista[i] = malloc(sizeof(int) * N_FAIXAS);
        for (int j = 0; j < N_FAIXAS; j++)
        {
            pista[i][j] = -1;
        }
    }
}

void inicializa_mutexes() {
    if (modo == 'i') {
        // Modo ingênuo: um único mutex
        if (pthread_mutex_init(&pista_mutex, NULL) != 0) {
            perror("Erro ao inicializar mutex global");
            exit(EXIT_FAILURE);
        }
    } else if (modo == 'e') {
        // Modo eficiente: aloca matriz de mutexes
        controle_pista = malloc(d * sizeof(pthread_mutex_t *));
        if (!controle_pista) {
            perror("Erro ao alocar controle_pista");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < d; i++) {
            controle_pista[i] = malloc(N_FAIXAS * sizeof(pthread_mutex_t));
            if (!controle_pista[i]) {
                perror("Erro ao alocar linha de mutexes");
                exit(EXIT_FAILURE);
            }

            for (int j = 0; j < N_FAIXAS; j++) {
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

void sorteia_largada(Ciclista *c)
{
    int pos, faixa;
    int achou_lugar = 0;

    while (!achou_lugar) {
        pos = rand() % d;
        faixa = rand() % N_FAIXAS;

        if (modo == 'i') {
            pthread_mutex_lock(&pista_mutex);
            if (pista[pos][faixa] == -1) {
                pista[pos][faixa] = c->id;
                achou_lugar = 1;
            }
            pthread_mutex_unlock(&pista_mutex);
        } else {
            pthread_mutex_lock(&controle_pista[pos][faixa]);
            if (pista[pos][faixa] == -1) {
                pista[pos][faixa] = c->id;
                achou_lugar = 1;
            }
            pthread_mutex_unlock(&controle_pista[pos][faixa]);
        }
    }

    // Registra no ciclista a posição definida
    c->posicao = pos;
    c->faixa = faixa;
}

void libera_pista() {
    if (pista) {
        for (int i = 0; i < d; i++) {
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
        for (int i = 0; i < d; i++) {
            for (int j = 0; j < N_FAIXAS; j++) {
                pthread_mutex_destroy(&controle_pista[i][j]);
            }
            free(controle_pista[i]);
        }
        free(controle_pista);
        controle_pista = NULL;
    }
}

void imprime_pista() {
    fprintf(stderr, "\nEstado atual da pista:\n");

    if (modo == 'i') {
        pthread_mutex_lock(&pista_mutex);
    }

    for (int faixa = 0; faixa < N_FAIXAS; faixa++) {
        fprintf(stderr, "Faixa %2d: ", faixa);
        for (int pos = 0; pos < d; pos++) {
            int id = pista[pos][faixa]; // leitura SEM lock no modo 'e'
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
    free(ciclistas);
    
    return 0;
}