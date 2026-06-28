#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <stdbool.h>

void connections_init(void * arg);
void connections_start_pairing(void);
bool connections_is_network_ready(void);

/*
 * Get the esp32's MAC address
 */
void get_device_id(char *buf, size_t len);

#endif
