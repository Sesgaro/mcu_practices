#include <MKL25Z4.h>
#include <stdint.h>

#include "I2C.h"
#include "RTC.h"
#include "LCD.h"
#include "SPI.h"
#include "MAX7219.h"
#include "Keypad.h"

typedef enum {
    MODE_NORMAL = 0,
    MODE_CONFIG_TIME,
    MODE_CONFIG_DATE
} SystemMode;

const unsigned int CLK_HZ = 20970000;

static const RTC_Time_t HORA_POR_DEFECTO = {
    0, 36, 22,      	// segundos, minutos, horas
    7,           	// dia (lunes, martes, etc)
    27, 9, 2026  	// dia, mes, ano
};

static const char *const DIA[8] = {
    "---", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab", "Dom"
};

static void UART0_init(void)
{
    SIM->SCGC4 |= 0x0400;
    SIM->SOPT2  = (SIM->SOPT2 & ~0x0C000000u) | 0x04000000u;

    UART0->C2 = 0x00;

    UART0->C4  = 0x0D;
    UART0->BDH = 0x00;
    UART0->BDL = 0x0D;

    UART0->C1 = 0x00;
    UART0->C2 = 0x0C;

    SIM->SCGC5   |= 0x0200;
    PORTA->PCR[2] = 0x0200;
    PORTA->PCR[1] = 0x0200;

    UART0->S1 = 0x1F;
    (void)UART0->D;
}

static void UART0_putc(char c)
{
    while (!(UART0->S1 & 0x80)) { }
    UART0->D = (uint8_t)c;
}

static void UART0_puts(const char *str)
{
    while (*str != '\0') {
        if (*str == '\n') {
            UART0_putc('\r');
        }
        UART0_putc(*str);
        str++;
    }
}

static void UART0_puthex8(uint8_t v)
{
    static const char hex[16] = "0123456789ABCDEF";

    UART0_puts("0x");
    UART0_putc(hex[(v >> 4) & 0x0Fu]);
    UART0_putc(hex[v & 0x0Fu]);
}

static void delayMs(int n)
{
    int i;

    SysTick->LOAD = (CLK_HZ / 1000u) - 1u;
    SysTick->CTRL = 0x5;
    for (i = 0; i < n; i++) {
        while ((SysTick->CTRL & 0x10000) == 0) { }
    }
    SysTick->CTRL = 0;
}

static void dos(char *dst, uint8_t v)
{
    dst[0] = (char)('0' + (v / 10u));
    dst[1] = (char)('0' + (v % 10u));
}

static void mostrarEnLCD(const RTC_Time_t *t)
{
    char l0[LCD_COLS + 1];
    char l1[LCD_COLS + 1];
    const char *d = DIA[(t->weekday <= 7u) ? t->weekday : 0u];
    uint8_t i;

    for (i = 0; i < LCD_COLS; i++) {
        l0[i] = ' ';
        l1[i] = ' ';
    }
    l0[LCD_COLS] = '\0';
    l1[LCD_COLS] = '\0';

    l0[0] = d[0];  l0[1] = d[1];  l0[2] = d[2];
    dos(&l0[4],  t->day);          l0[6]  = '/';
    dos(&l0[7],  t->month);        l0[9]  = '/';
    dos(&l0[10], (uint8_t)(t->year / 100u));
    dos(&l0[12], (uint8_t)(t->year % 100u));

    dos(&l1[4], t->hours);         l1[6]  = ':';
    dos(&l1[7], t->minutes);       l1[9]  = ':';
    dos(&l1[10], t->seconds);

    LCD_setCursor(0, 0);
    LCD_puts(l0);
    LCD_setCursor(0, 1);
    LCD_puts(l1);
}

static void rgb_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;

    /* LED RGB */
    PORTB->PCR[18] = 0x100;
    PORTB->PCR[19] = 0x100;
    PTB->PDDR |= (1u << 18) | (1u << 19);
    PTB->PSOR  = (1u << 18) | (1u << 19);
}

static void rgb_set(uint8_t estado)
{
    PTB->PSOR = (1u << 18) | (1u << 19);
    if (estado == 0) {
        PTB->PCOR = (1u << 18) | (1u << 19); /* Amarillo */
    } else if (estado == 1) {
        PTB->PCOR = (1u << 19);              /* Verde */
    } else if (estado == 2) {
        PTB->PCOR = (1u << 18);              /* Rojo */
    }
}

static void lcd_config_time(uint32_t val)
{
    char buf[17];
    int hh = (int)(val / 100u);
    int mm = (int)(val % 100u);
    int i;

    LCD_setCursor(0, 0);
    LCD_puts("SET TIME (HH:MM)");
    LCD_setCursor(0, 1);

    for (i = 0; i < 16; i++) buf[i] = ' ';
    buf[16] = '\0';

    buf[5] = (char)((hh / 10) + '0');
    buf[6] = (char)((hh % 10) + '0');
    buf[7] = ':';
    buf[8] = (char)((mm / 10) + '0');
    buf[9] = (char)((mm % 10) + '0');
    LCD_puts(buf);

    MAX7219_showTime((uint8_t)hh, (uint8_t)mm);
}

static void lcd_config_date(uint32_t val)
{
    char buf[17];
    int dd = (int)(val / 10000u);
    int mm = (int)((val / 100u) % 100u);
    int yy = (int)(val % 100u);
    int i;

    LCD_setCursor(0, 0);
    LCD_puts("SET DATE(DDMMYY)");
    LCD_setCursor(0, 1);

    for (i = 0; i < 16; i++) buf[i] = ' ';
    buf[16] = '\0';

    buf[4]  = (char)((dd / 10) + '0');
    buf[5]  = (char)((dd % 10) + '0');
    buf[6]  = '/';
    buf[7]  = (char)((mm / 10) + '0');
    buf[8]  = (char)((mm % 10) + '0');
    buf[9]  = '/';
    buf[10] = (char)((yy / 10) + '0');
    buf[11] = (char)((yy % 10) + '0');
    LCD_puts(buf);
}

int main(void)
{
    RTC_Time_t hora;
    RTC_Time_t config_hora_temp;

    int tick_1ms = 0;
    int tick_500ms = 0;
    char last_key = 0;

    SystemMode current_state = MODE_NORMAL;
    uint32_t input_buffer = 0;
    int input_digits = 0;

    UART0_init();
    I2C1_init();
    SPI0_init();
    Keypad_init();
    rgb_init();

    delayMs(100);

    UART0_puts("\n\n practica I2C\n");

    MAX7219_init();

    while (!LCD_check()) {
        UART0_puts("LCD no encontrado\n");
        delayMs(1000);
    }
    UART0_puts("LCD encontrado en ");
    UART0_puthex8(LCD_address());
    UART0_puts("\n");

    LCD_init();
    LCD_backlight(1);
    LCD_setCursor(0, 0);
    LCD_puts("Buscando RTC...");

    while (!DS3231_check()) {
        UART0_puts("DS3231 no encontrado\n");
        delayMs(1000);
    }
    UART0_puts("DS3231 encontrado\n");

    DS3231_init();
    DS3231_set_time(&HORA_POR_DEFECTO);
    DS3231_clear_osf();

    if (DS3231_lost_power()) {
        UART0_puts("AVISO: el DS3231 perdio la hora (OSF activo). Cargando hora por defecto.\n");
        DS3231_set_time(&HORA_POR_DEFECTO);
        DS3231_clear_osf();
    }

    LCD_clear();
    rgb_set(0);

    SysTick->LOAD = 20970 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = 5;

    for (;;) {
        if (SysTick->CTRL & 0x10000) {
            tick_1ms++;
            tick_500ms++;

            if (tick_1ms >= 20) {
                char key;

                tick_1ms = 0;
                key = Keypad_scan();

                if (key != 0 && last_key == 0) {
                    if (current_state == MODE_NORMAL) {
                        if (key == 'A') {
                            current_state = MODE_CONFIG_TIME;
                            input_buffer = 0;
                            input_digits = 0;
                            rgb_set(1); /* Verde indica modo config */
                            LCD_clear();
                            lcd_config_time(input_buffer);
                        }
                    }
                    else if (current_state == MODE_CONFIG_TIME) {
                        if (key >= '0' && key <= '9') {
                            if (input_digits < 4) {
                                input_buffer = (input_buffer * 10u) + (uint32_t)(key - '0');
                                input_digits++;
                                lcd_config_time(input_buffer);
                            }
                        } else if (key == '*') {
                            current_state = MODE_NORMAL;
                            rgb_set(0);
                            LCD_clear();
                        } else if (key == '#') {
                            if (input_digits == 4) {
                                config_hora_temp.hours   = (uint8_t)(input_buffer / 100u);
                                config_hora_temp.minutes = (uint8_t)(input_buffer % 100u);
                                config_hora_temp.seconds = 0;

                                current_state = MODE_CONFIG_DATE;
                                input_buffer = 0;
                                input_digits = 0;
                                LCD_clear();
                                lcd_config_date(input_buffer);
                            }
                        }
                    }
                    else if (current_state == MODE_CONFIG_DATE) {
                        if (key >= '0' && key <= '9') {
                            if (input_digits < 6) {
                                input_buffer = (input_buffer * 10u) + (uint32_t)(key - '0');
                                input_digits++;
                                lcd_config_date(input_buffer);
                            }
                        } else if (key == '*') {
                            current_state = MODE_NORMAL;
                            rgb_set(0);
                            LCD_clear();
                        } else if (key == '#') {
                            if (input_digits == 6) {
                                config_hora_temp.day     = (uint8_t)(input_buffer / 10000u);
                                config_hora_temp.month   = (uint8_t)((input_buffer / 100u) % 100u);
                                config_hora_temp.year    = (uint16_t)(2000u + (input_buffer % 100u));
                                config_hora_temp.weekday = 1; /* Valor por defecto */

                                DS3231_set_time(&config_hora_temp);

                                current_state = MODE_NORMAL;
                                rgb_set(0);
                                LCD_clear();
                            }
                        }
                    }
                }
                last_key = key;
            }

            if (tick_500ms >= 500) {
                tick_500ms = 0;

                if (current_state == MODE_NORMAL) {
                    if (DS3231_get_time(&hora) == ERR_NONE) {
                        mostrarEnLCD(&hora);
                        MAX7219_showTime(hora.hours, hora.minutes);
                    } else {
                        LCD_setCursor(0, 0);
                        LCD_puts("Error lectura   ");
                        LCD_setCursor(0, 1);
                        LCD_puts("RTC             ");
                    }
                }
            }
        }
    }
}
