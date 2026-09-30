#ifndef _LCD_H_
#define _LCD_H_

#include <stdint.h>

/* HD44780 de 16x2 sobre expansor PCF8574.
 * 0x27 es el PCF8574T y 0x3F el PCF8574AT: LCD_check() prueba los dos. */
#define LCD_ADDR_A   0x27u
#define LCD_ADDR_B   0x3Fu

#define LCD_COLS     16u
#define LCD_ROWS     2u

uint8_t LCD_check(void);                /* 1 = encontrado, 0 = ausente */
uint8_t LCD_address(void);              /* direccion en la que respondio */
void    LCD_init(void);
void    LCD_clear(void);
void    LCD_setCursor(uint8_t col, uint8_t row);
void    LCD_putc(char c);
void    LCD_puts(const char *s);
void    LCD_backlight(uint8_t on);

#endif /* _LCD_H_ */
