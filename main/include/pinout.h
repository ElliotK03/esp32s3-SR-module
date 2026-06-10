#ifndef PINOUT_H
#define PINOUT_H

#include "driver/gpio.h"

#define LCD_WR     GPIO_NUM_46 // Data control pin, labelled as RS in schematic
#define LCD_BL     GPIO_NUM_3
#define LCD_CS     GPIO_NUM_11
#define LCD_SDA    GPIO_NUM_13
#define LCD_SCK    GPIO_NUM_12
#define LCD_RESET  GPIO_NUM_10

#define TP_SCL GPIO_NUM_2
#define TP_SDA GPIO_NUM_1
#define TP_INT GPIO_NUM_42
#define TP_RST GPIO_NUM_41

#define MOTOR_B_L GPIO_NUM_6  // SET B NEVER ON AT SAME TIME
#define MOTOR_B_H GPIO_NUM_17 // SET B NEVER ON AT SAME TIME
#define MOTOR_A_L GPIO_NUM_7  // SET A NEVER ON AT SAME TIME 
#define MOTOR_A_H GPIO_NUM_18 // SET A NEVER ON AT SAME TIME

#define MOTOR_B2_L GPIO_NUM_4
#define MOTOR_A2_L GPIO_NUM_5
#define MOTOR_B2_H GPIO_NUM_15
#define MOTOR_A2_H GPIO_NUM_16

#endif