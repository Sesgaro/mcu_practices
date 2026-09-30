#include <MKL25Z4.h>
#include <stdint.h>

#include "I2C.h"
#include "RTC.h"
#include "LCD.h"
#include "SPI.h"
#include "MAX7219.h"

const unsigned int CLK_HZ = 20970000;

#define BUZZER_PIN (1U << 0)

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

static void BUZZER_init(void) {
    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;
    PORTC->PCR[0] = PORT_PCR_MUX(1);
    PTC->PDDR |= BUZZER_PIN;
    PTC->PCOR = BUZZER_PIN;
}

static void BUZZER_on(void)  { PTC->PSOR = BUZZER_PIN; }
static void BUZZER_off(void) { PTC->PCOR = BUZZER_PIN; }

typedef struct {
    uint8_t minutes;
    uint8_t seconds;
    uint8_t activa;
} Alarma_t;

static Alarma_t alarma = {0, 0, 0};

static void configurarAlarmaKeypad(void)
{
    char digitos[4] = {'0', '0', '0', '0'};
    uint8_t idx = 0;
    char key = 0;

    LCD_clear();
    LCD_setCursor(0, 0);
    LCD_puts("agrega tu alarma");
    LCD_setCursor(0, 1);
    LCD_puts("MM:SS -> --:--");

    while (idx < 4) {
        key = keypad_getKey();

        if (key >= '0' && key <= '9') {
            digitos[idx] = key;

            if (idx < 2) {
                LCD_setCursor(9 + idx, 1);
            } else {
                LCD_setCursor(10 + idx, 1);
            }

            char buffer[2] = {key, '\0'};
            LCD_puts(buffer);
            idx++;
        } else if (key == '*') {
            idx = 0;
            LCD_setCursor(0, 1);
            LCD_puts("MM:SS -> --:--");
        }
        delayMs(200);
    }

    LCD_setCursor(0, 0);
    LCD_puts("presiona * para guardar ");
    while (keypad_getKey() != '*') {
        delayMs(100);
    }

    uint8_t mm = (digitos[0] - '0') * 10 + (digitos[1] - '0');
    uint8_t ss = (digitos[2] - '0') * 10 + (digitos[3] - '0');

    if (mm < 60 && ss < 60) {
        alarma.minutes = mm;
        alarma.seconds = ss;
        alarma.activa = 1;
        LCD_clear();
        LCD_setCursor(0, 0);
        LCD_puts("alarma guardada ");
    } else {
        LCD_clear();
        LCD_setCursor(0, 0);
        LCD_puts("tiempo invalido ");
        alarma.activa = 0;
    }
    delayMs(1000);
    LCD_clear();
}


int main(void)
{
    RTC_Time_t hora;
    uint8_t sonando = 0;

    UART0_init();
    I2C1_init();
    SPI0_init();
    BUZZER_init();
    keypad_init();

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

    if (DS3231_lost_power()) {
        UART0_puts("AVISO: el DS3231 perdio la hora (OSF activo). Cargando hora por defecto.\n");
        DS3231_set_time(&HORA_POR_DEFECTO);
        DS3231_clear_osf();
    }

    configurarAlarmaKeypad();

    for (;;) {
        if (DS3231_get_time(&hora) == ERR_NONE) {

            if (alarma.activa && (hora.minutes == alarma.minutes) && (hora.seconds == alarma.seconds)) {
                sonando = 1;
            }

            if (sonando) {
                BUZZER_on();
                LCD_setCursor(0, 0);
                LCD_puts("*** alarma ***  ");
                LCD_setCursor(0, 1);
                LCD_puts("# para detener  ");
            } else {
                mostrarEnLCD(&hora);
            }

            MAX7219_showTime(hora.hours, hora.minutes);
        } else {
            LCD_setCursor(0, 0);
            LCD_puts("Error lectura   ");
            LCD_setCursor(0, 1);
            LCD_puts("RTC             ");
        }

        if (sonando) {
            char tecla = keypad_getKey();
            if (tecla == '#') {
                BUZZER_off();
                sonando = 0;
                alarma.activa = 0;
                LCD_clear();
            }
        }

        delayMs(200);
    }
}