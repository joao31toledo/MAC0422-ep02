# MAC0422-ep02

## variáveis:
- d: tamanho da pista, definido na entrada;
- k: quantidade de ciclistas, definido na entrada;
- modo: se é ingenuo(i) ou eficiente(e);
- debug: se vai entrar no modo debug ou não;
- MAX_TICKS: controla os testes, tem que excluir depois!!!!!
- tick_global: contador central de tempo de simulação;
- pista: matriz de inteiros pra mostrar a posição dos ciclistas;
- ciclistas: vetor da struct Ciclista;
- ultimo_ciclista_id: ID do ultimo a cruzar a linha de chegada a cada 2 rodadas pra ser eliminado;

#### variáveis de sincronização e controle:
- ciclistas_ativos: conta quantos ciclistas ainda estão correndo, serve pra barreira;
- tick_chegadas: conta quantos chegaram e estão esperando o proximo tick;
- rodada_voltas: controla a rodada atual para eliminação a cada 2 voltas;

## semáforos:
- controle_pista: matriz dos mutexes de cada posição da matriz pista, usado no modo e (eficiente);
- barreira_mutex: protege o acesso pra alterar o valor de ciclistas_ativos;
- print_mutex: protege as chamadas printf pra não ficar no meio do texto;
- tick_mutex: protege a variavel tick_chegadas na sincronização dos ticks;
- tick_cond: coordena o avanço dos ticks
- mutex_ultimo: pra proteger a eliminação do último colocado a cada 2 voltas;


## Funções:
#### inicializa_pista()
- cria a matriz pista com N_FAIXAS (10) linahs e d colunas;
- inicializa todas as posições com -1;

#### inicializa_mutexes()
- inicializa os mutexes pra proteger o acesso à pista;
- se o modo for i, é um mutex só
- se o modo for e, são vários mutexes, um pra cada posição da pista;

#### inicializa_ciclistas()
- aloca o vetor ciclistas;
- define os parametros iniciais de cada ciclista;
- sorteia a posição inicial do ciclista;

#### verifica_ultimo(ciclista c)
- chamada quando o ciclista termina uma volta;
- verifica se todos os ciclistas ativos já terminaram a volta;
- se sim, imprime qual foi o último a completar;

#### verifica_frente(ciclista c)
- consulta a posição na frente do ciclista (d+1)
- retorna o ID do ciclista que tá na frente ou -1 se não tiver ninguém;
- usa o mutex pra proteger a leitura conforme o modo (i ou e);

#### atualiza_velocidade(ciclista c)
- atualiza a velocidade de acordo com as probabilidades diferentes;
- atualiza 'ticks_para_mover'

#### aguarda e libera_tick()
- chama aguarda_tick() pra indicar que chegou ao fim do tick e aguarda o coordenador liberar pro próximo;
- chama libera_tick() para liberar todos os ciclistas e avança o tick_global;

#### verifica_volta_completada(Ciclista *c, int col_antiga, int col_nova)
- detecta quando o ciclista completou uma volta (se saiu de d-1 para 0);
- atualiza o número de voltas completadas;
- chama 'verifica_ultimo()' e 'atualiza_ultimo()';

#### verifica_quebra(ciclista c)
- a probabilidade de quebrar a cada 5 voltas;
- decrementa ciclistas_ativos se algum quebrar;

#### verifica_eliminacao(ciclista c)
- a cada rodada, contabiliza ciclistas que terminaram a volta;
- elimina o ultimo a completar a volta;
- atualiza 'ciclistas_ativos'

#### tenta_mover_para_frente(ciclista c)
- protege com mutexes;
- atualiza a posicao do ciclista na struct;

#### logica_ciclista()
- Função executada por cada thread de ciclista.
- Loop até MAX_TICKS ou até que ciclista quebre ou seja eliminado.
- A cada tick:
- Decrementa ticks_para_mover ou tenta se mover.
- Chama verificações de eliminação/quebra.
- Aguarda liberação do próximo tick (aguarda_tick()).
- Atualiza ciclistas_ativos ao terminar.
- Marca terminou para permitir o join.
- Imprime finalização protegida por mutex.

