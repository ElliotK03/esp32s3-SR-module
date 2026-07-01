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

/*
 * Generate a random UUID session ID
 */
void generate_session_id(char *buf, size_t len);

/*
 * Push session history to Firebase Firestore
 */
void connections_push_session(const char *session_id, const char *start_date, const char *start_time, int num_rounds, char *rounds_str);

#endif
