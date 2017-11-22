// Grupo 07
// Rui Lopes 47900 Rui Teixeira 47889 João Miranda 48666
//
#ifndef _CLIENT_STUB_PRIVATE_H
#define _CLIENT_STUB_PRIVATE_H
#include "client_stub.h"

struct rtables_t {
  struct server_t *server;
  char *address_port;
  int nrTables;
  int activeTable; // Guardar a tabela ativa no momento
};
int rtables_get_ntables(struct rtables_t *rtables);
#endif
