// Grupo 07
// Rui Lopes 47900 Rui Teixeira 47889 João Miranda 48666
//

/*
   Programa que implementa um servidor de uma tabela hash com chainning.
   Uso: table-server <port> <table1_size> [<table2_size> ...]
   Exemplo de uso: ./table_server 5000 10 15 20 25
*/
#include <errno.h>
#include <error.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "inet.h"
#include "message-private.h"
#include "network_client-private.h"
#include "table-private.h"
#include "client_stub-private.h"
#include "table_skel-private.h"

#define MAX_C 4
#define TIME -1
struct rtables_t *rtable;
//pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
struct message_t *msg_pedido;
int isPrimary;  // 1 se primario, 0 para secundario
int number = 0;
/* Função para preparar uma socket de receção de pedidos de ligação.
*/

void *thread_main(void* params){
  signal(SIGPIPE, SIG_IGN);
  //struct server_t *server = network_connect((const char *) params);
   //pthread_mutex_lock(&mutex1);
    pthread_mutex_lock(&mutex2);
    printf("Entrei na thread\n");

    network_send_receive(rtable->server,msg_pedido);
    print_message(msg_pedido);
    /* Se já fiz o que tinha a fazer, liberto o mutex*/
    pthread_mutex_unlock(&mutex2);
    
  }
int make_server_socket(short port) {
  int socket_fd;
  struct sockaddr_in server;

  if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Erro ao criar socket");
    return -1;
  }

  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  int so_reuseaddr = 1;
  setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr,
             sizeof(so_reuseaddr));

  if (bind(socket_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("Erro ao fazer bind");
    close(socket_fd);
    return -1;
  }

  if (listen(socket_fd, 0) < 0) {
    perror("Erro ao executar listen");
    close(socket_fd);
    return -1;
  }
  return socket_fd;
}

/* Função "inversa" da função network_send_receive usada no table-client.
   Neste caso a função implementa um ciclo receive/send:

        Recebe um pedido;
        Aplica o pedido na tabela;
        Envia a resposta.
*/
int network_receive_send(int sockfd) {
  char *message_resposta, *message_pedido;
  int message_size, msg_size,result; // message -> recebe do client, msg->dá ao client
  struct message_t *msg_resposta;

  /* Verificar parâmetros de entrada */
  if (sockfd < 0) {
    return -1;
  }
  /* Com a função read_all, receber num inteiro o tamanho da
     mensagem de pedido que será recebida de seguida.*/
  result = read_all(sockfd, (char *)&msg_size, _INT);
  if(result == 0){
    return 0;
    }
  int msg_size_conv = 0;
  msg_size_conv = ntohl(msg_size);
  /* Verificar se a receção teve sucesso */
  if (result != _INT)
    return -1;
  /* Alocar memória para receber o número de bytes da
     mensagem de pedido. */
  message_pedido = (char *)malloc(msg_size_conv);
  /* Com a função read_all, receber a mensagem de pedido. */
  result = read_all(sockfd, message_pedido, msg_size_conv);
  if(result == 0){
    free(message_pedido);
    return 0;
    }
  /* Verificar se a receção teve sucesso */
  if (result != msg_size_conv) {
    return -1;
  }
  /* Desserializar a mensagem do pedido */
  msg_pedido = buffer_to_message(message_pedido, msg_size_conv);
  print_message(msg_pedido);
  /* Verificar se a desserialização teve sucesso */
  if (msg_pedido == NULL)
    return -1;
  /* Processar a mensagem */

  msg_resposta = invoke(msg_pedido);

    if((msg_pedido->opcode == OC_PUT || msg_pedido->opcode == OC_UPDATE) && isPrimary == 1){  
      pthread_t secThread;
    if(pthread_create(&secThread, NULL, &thread_main, NULL) < 0)
        printf("A thread n foi inicializada");
    else
        printf("A thread secundaria foi inicializada");
    pthread_join(secThread, NULL);
  }
  print_message(msg_resposta);

  /* Serializar a mensagem recebida */
  message_size = message_to_buffer(msg_resposta, &message_resposta); // Problema aqui, server manda a resposta

  /* Verificar se a serialização teve sucesso */
  if (message_size == -1) {
    return -1;
  }

  /* Enviar ao cliente o tamanho da mensagem que será enviada
     logo de seguida
  */
  msg_size = htonl(message_size);
  result = write_all(sockfd, (char *)&msg_size, _INT);

  /* Verificar se o envio teve sucesso */
  if (result != _INT) {
    return -1;
  }
  /* Enviar a mensagem que foi previamente serializada */

  result = write_all(sockfd, message_resposta, message_size);

  /* Verificar se o envio teve sucesso */
  if (result != message_size) {
    return -1;
  }
  /* Libertar memória */
  free(message_pedido);
  free(message_resposta);
  if(isPrimary == 0){
    free_message(msg_pedido);
  }
  return 1;
}


int main(int argc, char **argv) {
  int listening_socket, connsock; //fd do secundário
  struct sockaddr_in client;
  socklen_t size_client;
  int server_error = 0;
  
  char **n_tables = (char **)malloc(sizeof(char *) * ((argc - 4) + 1));
  int ts;
  int index = 0;
  if (argc < 2){
    printf("Exemplo de uso: Primario: ./table-server 5000 127.0.0.1:5001 10 15 20 25\n");
    printf("Exemplo de uso: Secundario ./table-server 5001\n");
    return -1;
  }

  if(argc > 4 ){ // Primario
    isPrimary = 1;
    //pthread_mutex_lock(&mutex2);
    /*
    if(pthread_join(secThread,NULL) < 0)
       printf("A thread n foi joined");
    else
        printf("A thread foi joined");
        */
    size_client = sizeof(struct sockaddr_in);
    if ((listening_socket = make_server_socket(atoi(argv[1]))) < 0){
      return -1;
    }

    for (ts = 4; ts < argc; ts++){ 
      n_tables[index] = strdup(argv[ts]);
      index++;
    }
    n_tables[index] = NULL;

    rtable = rtables_bind(argv[2]);
    int rc = 0;
    struct message_t *message =(struct message_t *)malloc(sizeof(struct message_t));
    if (message == NULL)
      return -1;
    message->table_num = 0;
    message->opcode = OC_TCREATE;
    message->c_type = CT_KEYS;
    message->content.keys = n_tables;
    struct message_t *msg_resposta = network_send_receive(rtable->server, message);
    if (msg_resposta == NULL) {
      rc = reconnect(rtable);
      if (rc == -1) {
        free_message(message);
        free_message(msg_resposta);
        return -1;
      } else
        msg_resposta = network_send_receive(rtable->server, message);
    }

    if (msg_resposta->opcode == OC_RT_ERROR) {
      free_message(message);
      free_message(msg_resposta);
      return -1;
    }
    if (table_skel_init(n_tables) == -1) {
    perror("Erro ao criar tabela");
    close(listening_socket);
    return -1;
  }
  }
    

    
  else{ // Sou secundário
    printf("Sou secundario\n");
    int result, connsock;
    size_client = sizeof(struct sockaddr_in);
    if ((listening_socket = make_server_socket(atoi(argv[1]))) < 0)
      return -1;
  }

  struct pollfd *poll_list = NULL;  

  // É primario

  int totalConnections = MAX_C + 2;

  poll_list = (struct pollfd *)malloc(sizeof(struct pollfd) * totalConnections);

  for (int i = 0; i < totalConnections; i++) {
    poll_list[i].fd = -1;
    poll_list[i].events = POLLIN;
  }
  poll_list[0].fd = listening_socket; // Socket de escuta toma o 1 lugar da lista
  poll_list[1].fd = fileno(stdin);
  poll_list[1].events = POLLIN;
  printf("Servidor operacional!\n");

  while (server_error == 0) {
    int resultado = poll(poll_list, totalConnections, TIME);
    if (resultado < 0) {
      if (errno != EINTR)
        server_error = 1;
      continue;
    }

    if (poll_list[0].revents &&
        POLLIN) { // Tem pedidos para ler(Socket de escuta)

      // Nova ligacao
      if ((connsock = accept(poll_list[0].fd, (struct sockaddr *)&client,
                             &size_client)) != -1) {
        int j = 2;
        int index2 = 0;
        // Encontrar a posicao onde adicionar
        while (index2 < MAX_C && poll_list[j].fd != -1){
          j++;
          index2++;
        }

        if (index2 < MAX_C) {
          printf("\nCliente %d ligou-se!\n", j - 1);
          poll_list[j].fd = connsock;
          poll_list[j].events = POLLIN;
        }
      }
    }

    if (poll_list[1].revents & POLLIN) {
      char input[81];
      fgets(input, 81, stdin); // Ver o size
      input[strlen(input) - 1] = '\0';
      char *aux = strdup(input);
      char *nT = strtok(aux, " ");
      int nr = atoi(strtok(NULL, " "));
      if (strcmp(nT, "print") == 0) {
        table_skel_print_table(nr);
      }
    }

    // para cada socket cliente
    for (int k = 2; k < totalConnections; k++) {
      if (poll_list[k].revents & POLLIN) {
        if(network_receive_send(poll_list[k].fd) == 0){
          printf("\nCliente %d saiu!\n", k-1);
          close(poll_list[k].fd);
          poll_list[k].fd = -1;
        }
      }
    }
  }
  table_skel_destroy();

  // Fechar conexoes
  for (int t = 0; t < totalConnections; t++) {
    if (poll_list[t].fd != -1)
      close(poll_list[t].fd);
  }
  printf("Tudo limpo, servidor a terminar\n");
  return 0;
}
