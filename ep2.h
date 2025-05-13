#ifndef EP2_H
#define EP2_H

#include <pthread.h>

#define N_FAIXAS 10

// Variáveis globais
extern int d; // comprimento da pista
extern int k; // número de ciclistas
extern char modo; // 'i' ou 'e'
extern int debug; // modo debug

// Pista e controle
extern int **pista;
extern pthread_mutex_t pista_mutex;
extern pthread_mutex_t **controle_pista;

// Struct do ciclista
typedef struct {
    int id;                         
    int posicao;                    
    int faixa;                      

    int velocidade;                
    int velocidade_anterior;      
    int ticks_para_mover;         

    int voltas_completadas;       
    int quebrado;                 
    int eliminado;                

    pthread_t thread;             
    pthread_mutex_t mutex;       
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
