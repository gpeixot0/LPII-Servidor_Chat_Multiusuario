# LPII-Servidor_Chat_Multiusuario
Servidor de Chat Multiusuário (TCP)
Descrição do Projeto

Este projeto implementa um servidor de chat multiusuário concorrente utilizando TCP em linguagem C, com suporte a múltiplos clientes simultâneos.
Cada cliente se conecta via terminal (CLI) e pode enviar e receber mensagens em tempo real.
O servidor retransmite as mensagens recebidas para todos os outros clientes conectados (broadcast).

O sistema segue o modelo cliente-servidor, com uso de threads (pthreads), mecanismos de exclusão mútua, monitores e semáforos para evitar condições de corrida, além de um sistema de logging concorrente via biblioteca libtslog.

 Objetivo

O principal objetivo é demonstrar o funcionamento de uma aplicação cliente-servidor TCP concorrente, que:

Gerencia múltiplas conexões simultâneas com threads seguras;

Garante sincronização entre threads usando mutexes, semáforos e variáveis condicionais;

Registra todas as ações e eventos do servidor com logging concorrente (libtslog);

Fornece um cliente CLI funcional que envia e recebe mensagens em tempo real;

Oferece tolerância a encerramentos (comandos /quit e Ctrl+C).

 Arquitetura do Sistema

O sistema é composto pelos seguintes módulos:

Arquivo	Função
Servidor.c	Contém o código principal do servidor TCP. Cria threads para cada cliente, gerencia lista de conexões, fila de mensagens e logging.
Cliente.c	Implementa o cliente CLI. Conecta ao servidor, envia mensagens e exibe o que recebe em tempo real.
libtslog.h	Biblioteca de logging concorrente usada para registrar eventos com timestamp e ID de thread.
Makefile	Arquivo de build para compilar e gerar os executáveis server e client.
Script_teste_server.sh	Script automatizado para simular múltiplos clientes conectando e trocando mensagens.
DiagramaSequenciaServerCliente.txt	Documento com diagramas de sequência explicando a comunicação entre cliente e servidor em diferentes fases.
Analise_Critica_Servidor_Multithread.pdf	Relatório de análise crítica do código, com detecção e discussão de possíveis race conditions, deadlocks e sugestões de melhoria.
 Descrição dos Headers e Estrutura de Sincronização
 libtslog.h

Biblioteca usada para logar eventos de forma concorrente e segura.
Cada mensagem de log inclui:

Timestamp formatado (data e hora);

Tipo da mensagem (INFO, WARNING, ERROR);

ID da thread que gerou o evento.

Usa mutexes internos para garantir exclusão mútua durante a escrita no log.

 Estruturas protegidas

Lista de clientes ativos: protegida por mutex para evitar acessos concorrentes simultâneos.

Fila de mensagens (monitor): utiliza variável condicional e semáforo para controlar o consumo e a produção de mensagens de broadcast.

Thread Broadcaster: monitora a fila de mensagens, garantindo que o envio de mensagens seja feito de forma síncrona e segura.

 Diagramas de Sequência

O arquivo DiagramaSequenciaServerCliente.txt contém representações textuais dos fluxos de comunicação e sincronização do sistema.
Ele detalha quatro cenários principais:

Comunicação TCP padrão: mostra as chamadas socket(), bind(), listen(), accept() e connect() entre cliente e servidor.

Conexão inicial e entrada no chat: explica o momento em que o cliente se conecta, envia seu nome e o servidor anuncia sua entrada aos demais usuários.

Envio e recebimento de mensagens: ilustra o fluxo de mensagens entre diferentes clientes, passando pela fila de mensagens e o broadcaster.

Saída do cliente e encerramento do servidor: descreve o encerramento de conexões e o tratamento de sinais (SIGINT e /quit).

Esses diagramas demonstram claramente a arquitetura multithread e o uso dos mecanismos de sincronização na aplicação.

 Análise Crítica do Código

O arquivo Analise_Critica_Servidor_Multithread.pdf contém um estudo detalhado sobre:

Potenciais race conditions na manipulação da lista de clientes e fila de mensagens;

Riscos de deadlocks entre mutexes e condvars;

Possibilidades de starvation (threads bloqueadas esperando broadcast);

Boas práticas aplicadas e sugestões de otimização para desempenho e segurança.

Essa análise foi feita utilizando prompts de IA para identificar padrões de risco e sugerir soluções adequadas — como encapsular a sincronização em monitores e revisar o uso de semáforos.

 Requisitos de Compilação

Sistema operacional: Linux 

Compilador: gcc

Dependências:

pthread

libtslog (já incluída no repositório via libtslog.h)

 Instruções de Compilação

Para compilar o projeto completo, basta executar:

make


Isso irá gerar os binários:

./servidor
./cliente


Para limpar os arquivos compilados:

make clean

 Como Executar
1. Iniciar o servidor:
./servidor

2. Em outro terminal, iniciar o cliente:
./cliente


O cliente solicitará seu nome de usuário e, em seguida, poderá enviar mensagens.
Use o comando:

/quit


para sair do chat.
O servidor também pode ser encerrado de forma segura com Ctrl+C.

 Testes Automatizados

Para simular múltiplos clientes conectando e enviando mensagens:

bash Script_teste_server.sh

 Arquitetura Geral

O servidor segue o modelo:

Cliente (CLI) <--TCP--> Servidor Principal
                          ├── Thread Broadcaster
                          ├── Thread Cliente 1
                          ├── Thread Cliente 2
                          └── ...


Cada cliente possui sua thread dedicada.
As mensagens são colocadas em uma fila protegida (monitor) e consumidas pelo broadcaster, que as envia para todos os clientes conectados.

 Conclusão

Este projeto demonstra um sistema cliente-servidor robusto e modular, aplicando conceitos de:

Concorrência e sincronização

Comunicação em rede TCP

Logging multithread seguro

Gerenciamento de recursos e threads

Arquitetura escalável para múltiplos clientes
