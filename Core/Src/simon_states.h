#ifndef SIMON_STATES_H
#define SIMON_STATES_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "stm32h5xx_it.h"
#include "buzzer.h"

void lightPad(uint8_t color, bool on);
void play_note(uint8_t color_index);
void stop_note(void);
uint8_t readButton(void);
void Game_GPIO_Init(void);
void Simon_State_Machine(void);

#endif