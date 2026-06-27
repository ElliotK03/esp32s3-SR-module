#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void motor_init(void);
void motor_brake(void);
void motor_turn_cw(uint32_t cmpr_value);
void motor_turn_ccw(uint32_t cmpr_value);
void motor_lock(void);
void motor_unlock(void);

#ifdef __cplusplus
}
#endif

#endif // MOTOR_CONTROL_H
