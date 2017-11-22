// Grupo 07
// Rui Lopes 47900 Rui Teixeira 47889 João Miranda 48666
//
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "message.h"
#include "table.h" /* for table_free_keys() */

void free_message(struct message_t *msg) {
  if (msg != NULL) {
    switch (msg->c_type) {
    case CT_VALUE:
      data_destroy(msg->content.data);
      break;
    case CT_ENTRY:
      data_destroy(msg->content.entry->value);
      free(msg->content.entry->key);
      free(msg->content.entry);
      break;
    case CT_KEYS:
      table_free_keys(msg->content.keys);
      break;
    case CT_KEY:
      free(msg->content.key);
      break;
    }
    free(msg);
  }
}

int message_to_buffer(struct message_t *msg, char **msg_buf) {
  int buffer_size = _SHORT + _SHORT + _SHORT;
  int i, k, int_v;
  short short_v;
  char *ptr;

  if (msg == NULL)
    return -1;

  switch (msg->c_type) {
  case CT_VALUE:
    buffer_size += _INT;
    buffer_size += msg->content.data->datasize;
    break;
  case CT_ENTRY:
    buffer_size += _SHORT + _INT;
    buffer_size += strlen(msg->content.entry->key);
    buffer_size += msg->content.entry->value->datasize;
    break;
  case CT_KEYS:
    buffer_size += _INT;
    i = 0;
    while (msg->content.keys[i] != NULL)
      buffer_size += _SHORT + strlen(msg->content.keys[i++]);
    k = i;
    break;
  case CT_KEY:
    buffer_size += _SHORT + strlen(msg->content.key);
    break;
  case CT_RESULT:
    buffer_size += _INT;
    break;
  }

  if ((*msg_buf = (char *)malloc(buffer_size)) == NULL)
    return -1;
  ptr = *msg_buf;

  short_v = htons(msg->opcode);
  memcpy(ptr, &short_v, _SHORT);
  ptr += _SHORT;

  short_v = htons(msg->c_type);
  memcpy(ptr, &short_v, _SHORT);
  ptr += _SHORT;

  short_v = htons(msg->table_num);
  memcpy(ptr, &short_v, _SHORT);
  ptr += _SHORT;

  if (msg->c_type == CT_RESULT) {
  }

  switch (msg->c_type) {
  case CT_RESULT:
    int_v = htonl(msg->content.result);
    memcpy(ptr, &int_v, _INT);
    break;
  case CT_KEY:
    short_v = htons(strlen(msg->content.key));
    memcpy(ptr, &short_v, _SHORT);
    ptr += _SHORT;
    memcpy(ptr, msg->content.key, strlen(msg->content.key));
    break;
  case CT_KEYS:
    int_v = htonl(k);
    memcpy(ptr, &int_v, _INT);
    ptr += _INT;
    i = 0;
    while (msg->content.keys[i] != NULL) {
      short_v = htons(strlen(msg->content.keys[i]));
      memcpy(ptr, &short_v, _SHORT);
      ptr += _SHORT;
      memcpy(ptr, msg->content.keys[i], strlen(msg->content.keys[i]));
      ptr += strlen(msg->content.keys[i++]);
    }
    break;
  case CT_VALUE:
    int_v = htonl(msg->content.data->datasize);
    memcpy(ptr, &int_v, _INT);
    ptr += _INT;
    memcpy(ptr, msg->content.data->data, msg->content.data->datasize);
    break;
  case CT_ENTRY:
    short_v = htons(strlen(msg->content.entry->key));
    memcpy(ptr, &short_v, _SHORT);
    ptr += _SHORT;
    memcpy(ptr, msg->content.entry->key, strlen(msg->content.entry->key));
    ptr += strlen(msg->content.entry->key);
    int_v = htonl(msg->content.entry->value->datasize);
    memcpy(ptr, &int_v, _INT);
    ptr += _INT;
    memcpy(ptr, msg->content.entry->value->data,
           msg->content.entry->value->datasize);
    break;
  }
  return buffer_size;
}

struct message_t *buffer_to_message(char *msg_buf, int msg_size) {
  struct message_t *m;
  struct data_t *d;
  char *key;
  int i, ks, k;
  short ss;

  if (msg_buf == NULL)
    return NULL;

  if ((m = (struct message_t *)malloc(sizeof(struct message_t))) == NULL)
    return NULL;

  m->opcode = ntohs(*(short *)msg_buf++);
  m->c_type = ntohs(*(short *)++msg_buf);
  msg_buf++;
  m->table_num = ntohs(*(short *)++msg_buf);

  if ((m->opcode < 10) || (m->opcode > 61)) {
    free_message(m);
    return NULL;
  }

  msg_buf += _SHORT;
  switch (m->c_type) {
  case CT_RESULT:
    m->content.result = ntohl(*(int *)msg_buf);
    return m;
  case CT_KEY:
    ks = (int)ntohs(*(short *)msg_buf);
    msg_buf += _SHORT;
    if ((m->content.key = (char *)malloc((ks * sizeof(char)) + 1)) == NULL) {
      free(m);
      return NULL;
    }
    memcpy(m->content.key, msg_buf, ks);
    m->content.key[ks] = '\0';
    return m;
  case CT_KEYS:
    ks = ntohl(*(int *)msg_buf);
    msg_buf += _INT;
    if ((m->content.keys = (char **)malloc((ks + 1) * sizeof(char *))) ==
        NULL) {
      free(m);
      return NULL;
    }
    for (i = 0; i < ks; i++) {
      ss = ntohs(*(short *)msg_buf);
      msg_buf += _SHORT;
      if ((m->content.keys[i] = malloc((ss * sizeof(char)) + 1)) == NULL) {
        for (k = 0; k < i; k++)
          free(m->content.keys[k]);
        free(m);
        return NULL;
      }
      memcpy(m->content.keys[i], msg_buf, ss);
      m->content.keys[i][ss] = '\0';
      msg_buf += ss;
    }
    m->content.keys[i] = NULL;
    return m;
  case CT_VALUE:
    ks = ntohl(*(int *)msg_buf);
    msg_buf += _INT;
    if ((m->content.data = data_create(ks)) == NULL) {
      struct data_t *temp = malloc(sizeof(struct data_t));
      if (temp == NULL)
        return NULL;
      temp->data = NULL;
      temp->datasize = 0;
      m->content.data = temp;
      return m;
    }
    memcpy(m->content.data->data, msg_buf, ks);
    return m;
  case CT_ENTRY:
    ss = ntohs(*(short *)msg_buf);
    msg_buf += _SHORT;
    if ((key = malloc((ss * sizeof(char)) + 1)) == NULL) {
      free(m);
      return NULL;
    }
    memcpy(key, msg_buf, ss);
    key[ss] = '\0';
    msg_buf += ss;
    ks = ntohl(*(int *)msg_buf);
    msg_buf += _INT;
    if ((d = data_create(ks)) == NULL) {
      free(key);
      free(m);
      return NULL;
    }
    memcpy(d->data, msg_buf, ks);
    m->content.entry = (struct entry_t *)malloc(sizeof(struct entry_t));
    if (m->content.entry == NULL) {
      data_destroy(d);
      free(key);
      free(m);
      return NULL;
    }
    m->content.entry->key = strdup(key);
    if (m->content.entry->key == NULL) {
      data_destroy(d);
      free(key);
      free(m->content.entry);
      free(m);
      return NULL;
    }
    m->content.entry->value = data_dup(d);
    if (m->content.entry->value == NULL) {
      data_destroy(d);
      free(key);
      free(m->content.entry->key);
      free(m->content.entry);
      free(m);
      return NULL;
    }
    data_destroy(d);
    free(key);
    return m;
  }
  free(m);
  return NULL;
}
