#ifndef EP2_H
#define EP2_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define N_FAIXAS 10

// Variáveis globais
extern int d; // comprimento da pista
extern int k; // número de ciclistas
extern char modo; // 'i' ou 'e'
extern int debug; // modo debug

// Pista e controle
extern int **pista; // matriz da pista
extern pthread_mutex_t pista_mutex; // para o modo ingenuo
extern pthread_mutex_t **controle_pista; // para o modo eficiente

// Barreira de sincronização
int *arrive; 
int *continue_flag;
pthread_mutex_t *tick_mutexes;
pthread_cond_t *tick_conds;

// Struct do ciclista
typedef struct {
    int id; // numero que identifica o ciclista na pista
    int linha_pista; // posicao que o ciclista ocupa
    int coluna_pista; // posicao que o ciclista ocupa

    int velocidade; // velocidade que o ciclista esta'
    int velocidade_anterior; // velocidade anterior do ciclista, usada pra calcular a velocidade atual
    int ticks_para_mover; // dependendo da velocidade, o ciclista demora mais pra se mover

    int voltas_completadas; // contador de voltas que o ciclista completou
    int quebrado; // se ele quebrou ou nao
    int eliminado; // se ele foi eliminado ou nao

    pthread_t thread; // thread que controla a logica do ciclista
    pthread_mutex_t mutex; // controla quem tem acesso aos valores da struct
} Ciclista;


extern Ciclista *ciclistas;

// Funções utilitárias
void inicializa_pista();
void inicializa_ciclistas();
void inicializa_threads_ciclistas();
void aguarda_threads_ciclistas();
void sorteia_largada(Ciclista *c);
void inicializa_mutexes();
void libera_pista();
void libera_mutexes();
void imprime_pista();
void *logica_cilista(void *arg);


#endif
