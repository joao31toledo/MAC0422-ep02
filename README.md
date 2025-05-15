# MAC0422-ep02
## Objetivo
> simular uma corrida por eliminação em um velódromo, contendo:
> - k ciclistas iniciando a corrida ao mesmo tempo;
> - a pista possui *d* metros (100 <= d <= 2500);
> - a cada 2 voltas, o último a cruzar a linha de chegada é eliminado;
> - o último ciclista a permanecer, vence;

## Plano:
#### 1 - criar estruturas básicas
- definir o *Ciclista*, a *Pista*, *mutexes*, ...
- verificar quais funções serão necessárias

#### 2 - Inicializações
- alocar a pista;
- inicializar mutex/mutexes
- criar vetor de ciclistas
- sortear as posições de largada

## To-Do
#### 3 - criar e iniciar as threads dos ciclistas
- criar as threads para cada ciclista;
- implementar uma função para:
> - controlar a velocidade
> - verificar ultrapassagens
> - verificar a quebra
> - verificar eliminação
#### 4 - criar um relógio global

#### 5 - controlar o movimento dos ciclistas
!!!!!!!!! antes de remover o último, por exemplo, faz pra remover por ordem de ID, só pra remover mesmo, pra verificar a lógica de remoção!!!!!!!!!!!!!!!!!

#### 6- como coordenar os eventos?

#### 7 - encerramento

#### 8 - documentação e entrega
