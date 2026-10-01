#ifndef _BUZZER_H_
#define _BUZZER_H_

#include <stdint.h>

#define BUZZER_TONE_HZ   2000u

void BUZZER_init(void);
void BUZZER_on(void);
void BUZZER_off(void);
void BUZZER_tone(uint16_t hz);

#endif
