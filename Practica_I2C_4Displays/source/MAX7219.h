#ifndef _MAX7219_H_
#define _MAX7219_H_

#include <stdint.h>

#define MAX7219_DECODE     9
#define MAX7219_INTENSITY  10
#define MAX7219_SCANLIMIT  11
#define MAX7219_SHUTDOWN   12
#define MAX7219_TEST       15

void MAX7219_write(uint8_t command, uint8_t data);
void MAX7219_init(void);
void MAX7219_showTime(uint8_t hours, uint8_t minutes);

#endif /* _MAX7219_H_ */
