#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <stdbool.h>

void connections_init(void * arg);
void connections_start_pairing(void);
bool connections_is_network_ready(void);

#endif
