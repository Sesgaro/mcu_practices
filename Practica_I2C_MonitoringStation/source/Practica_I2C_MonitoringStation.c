#include <MKL25Z4.h>
#include <stdint.h>

#include "I2C.h"
#include "RTC.h"
#include "INA3221.h"
#include "LCD.h"
#include "SPI.h"
#include "MAX7219.h"
#include "Keypad.h"
#include "Buzzer.h"
#include "Melody.h"

const unsigned int CLK_HZ = 20970000;

#define ALARM_PIN   4           /* PTD4 <- INT/SQW del DS3231 (PORTD tiene IRQ propia) */

#define LED_ROJO    18          /* PTB18 */
#define LED_VERDE   19          /* PTB19 */

static const RTC_Time_t HORA_POR_DEFECTO = {
    13, 23, 13,      	// segundos, minutos, horas
    3,           	// dia (lunes, martes, etc)
    30, 9, 2026  	// dia, mes, ano
};

typedef enum {
    MODE_NORMAL = 0,
    MODE_MENU,
    MODE_SET_TIME,
    MODE_SET_DATE,
    MODE_SET_ALARM,
    MODE_VIEW_INA,
    MODE_ALARM
} SystemMode;

static volatile uint8_t alarma_irq = 0;     /* la pone la ISR de PORTD */
static uint8_t ina_ok = 0;


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


static void led_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    PORTB->PCR[LED_ROJO]  = 0x100;
    PORTB->PCR[LED_VERDE] = 0x100;
    PTB->PDDR |= (1u << LED_ROJO) | (1u << LED_VERDE);
    PTB->PSOR  = (1u << LED_ROJO) | (1u << LED_VERDE);   /* apagados (activo bajo) */
}

static void led_set(uint8_t rojo, uint8_t verde)
{
    if (rojo)  PTB->PCOR = (1u << LED_ROJO);  else PTB->PSOR = (1u << LED_ROJO);
    if (verde) PTB->PCOR = (1u << LED_VERDE); else PTB->PSOR = (1u << LED_VERDE);
}

/* El INT/SQW del DS3231 es colector abierto y activo en bajo: se detecta
 * por flanco de bajada y el pin lleva pull-up interno. */
static void alarmPin_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTD_MASK;
    PORTD->PCR[ALARM_PIN] = PORT_PCR_MUX(1)     /* GPIO             */
                          | PORT_PCR_PE_MASK    /* pull habilitado  */
                          | PORT_PCR_PS_MASK    /* pull-up          */
                          | PORT_PCR_IRQC(0x0A) /* IRQ por flanco de bajada */
                          | PORT_PCR_ISF_MASK;  /* limpia la bandera */
    PTD->PDDR &= ~(1u << ALARM_PIN);

    NVIC_SetPriority(PORTD_IRQn, 2);
    NVIC_ClearPendingIRQ(PORTD_IRQn);
    NVIC_EnableIRQ(PORTD_IRQn);
}

void PORTD_IRQHandler(void)
{
    if (PORTD->PCR[ALARM_PIN] & PORT_PCR_ISF_MASK) {
        PORTD->PCR[ALARM_PIN] |= PORT_PCR_ISF_MASK;     /* W1C */
        alarma_irq = 1;
    }
}


static void dos(char *dst, uint8_t v)
{
    dst[0] = (char)('0' + (v / 10u));
    dst[1] = (char)('0' + (v % 10u));
}

static void limpiar(char *l)
{
    uint8_t i;

    for (i = 0; i < LCD_COLS; i++) l[i] = ' ';
    l[LCD_COLS] = '\0';
}

static void fmtVolt(char *dst, int32_t mV)
{
    int32_t v, f;
    uint8_t neg = 0;

    if (mV < 0) { mV = -mV; neg = 1; }
    v = mV / 1000;
    f = mV % 1000;

    if (v >= 10) dst[0] = (char)('0' + (char)((v / 10) % 10));
    else         dst[0] = neg ? '-' : ' ';

    dst[1] = (char)('0' + (char)(v % 10));
    dst[2] = '.';
    dst[3] = (char)('0' + (char)(f / 100));
    dst[4] = (char)('0' + (char)((f / 10) % 10));
    dst[5] = (char)('0' + (char)(f % 10));
    dst[6] = 'V';
}

static void intDerecha(char *l, uint8_t endCol, int32_t v)
{
    uint8_t neg = 0;
    int8_t  i = (int8_t)endCol;

    if (v < 0) { v = -v; neg = 1; }

    do {
        l[i--] = (char)('0' + (char)(v % 10));
        v /= 10;
    } while (v > 0 && i >= 0);

    if (neg && i >= 0) l[i] = '-';
}

static void pintar(const char *l0, const char *l1)
{
    LCD_setCursor(0, 0);
    LCD_puts(l0);
    LCD_setCursor(0, 1);
    LCD_puts(l1);
}

/*  27/09/26 5.008V
 *  22:36:05 ALM:ON     */
static void pantallaNormal(const RTC_Time_t *t, int32_t mV, uint8_t alm)
{
    char l0[LCD_COLS + 1];
    char l1[LCD_COLS + 1];
    uint8_t i;

    limpiar(l0);
    limpiar(l1);

    dos(&l0[0], t->day);                        l0[2] = '/';
    dos(&l0[3], t->month);                      l0[5] = '/';
    dos(&l0[6], (uint8_t)(t->year % 100u));

    if (ina_ok) {
        fmtVolt(&l0[9], mV);
    } else {
        for (i = 9; i < 15; i++) l0[i] = '-';
        l0[15] = 'V';
    }

    dos(&l1[0], t->hours);                      l1[2] = ':';
    dos(&l1[3], t->minutes);                    l1[5] = ':';
    dos(&l1[6], t->seconds);
    l1[9]  = 'A'; l1[10] = 'L'; l1[11] = 'M'; l1[12] = ':';
    if (alm) { l1[13] = 'O'; l1[14] = 'N'; }
    else     { l1[13] = 'O'; l1[14] = 'F'; l1[15] = 'F'; }

    pintar(l0, l1);
}

/*  1Hora 2Fecha
 *  3Alm 4 ON 5INA *   */
static void pantallaMenu(uint8_t alm)
{
    pintar("1Hora 2Fecha    ",
           alm ? "3Alm 4 ON 5INA *" : "3Alm 4OFF 5INA *");
}

/*  Vbus     5.008V
 *  Ich        8mA *   */
static void pantallaINA(const INA_Data_t *d)
{
    char l0[LCD_COLS + 1];
    char l1[LCD_COLS + 1];
    uint8_t i;

    limpiar(l0);
    limpiar(l1);

    l0[0] = 'V'; l0[1] = 'b'; l0[2] = 'u'; l0[3] = 's';
    if (ina_ok) {
        fmtVolt(&l0[8], d->bus_mV);
    } else {
        for (i = 8; i < 14; i++) l0[i] = '-';
        l0[14] = 'V';
    }

    l1[0] = 'I'; l1[1] = 'c'; l1[2] = 'h';
    if (ina_ok) {
        intDerecha(l1, 11, d->current_mA);
    } else {
        l1[10] = '-'; l1[11] = '-';
    }
    l1[12] = 'm'; l1[13] = 'A';
    l1[15] = '*';

    pintar(l0, l1);
}

static void pantallaEntrada(const char *titulo, uint32_t val, uint8_t ndig)
{
    char l1[LCD_COLS + 1];
    uint8_t i;
    uint32_t div = 1;

    limpiar(l1);
    for (i = 1; i < ndig; i++) div *= 10u;

    for (i = 0; i < ndig; i++) {
        l1[4 + i] = (char)('0' + ((val / div) % 10u));
        div /= 10u;
    }

    pintar(titulo, l1);
}


int main(void)
{
    RTC_Time_t hora;
    RTC_Time_t nueva;
    INA_Data_t canal;

    SystemMode modo = MODE_NORMAL;
    uint32_t buffer = 0;
    uint8_t  ndig = 0;
    uint8_t  alm_on = 0;
    uint8_t  alm_h = 7, alm_m = 0;
    char last_key = 0;

    int tick_20ms = 0;
    int tick_500ms = 0;

    UART0_init();
    I2C1_init();
    SPI0_init();
    Keypad_init();
    BUZZER_init();
    led_init();
    alarmPin_init();

    delayMs(100);
    UART0_puts("\n\n=== Estacion de monitoreo ===\n");

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
        UART0_puts("AVISO: el DS3231 perdio la hora. Cargando hora por defecto.\n");
        DS3231_set_time(&HORA_POR_DEFECTO);
        DS3231_clear_osf();
    }

    ina_ok = INA3221_check();
    if (ina_ok) {
        INA3221_init();
        UART0_puts("INA3221 encontrado en 0x40\n");
    } else {
        UART0_puts("AVISO: INA3221 no responde en 0x40\n");
    }

    DS3231_setAlarm1(alm_h, alm_m, 0);
    DS3231_alarmEnable(0);
    DS3231_clearAlarmFlag();
    alm_on = 0;

    LCD_clear();
    led_set(0, 0);

    SysTick->LOAD = 20970 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = 5;

    for (;;) {
        if (!(SysTick->CTRL & 0x10000)) {
            continue;
        }

        Melody_tick();

        /* --- la alarma la dispara el DS3231, no una comparacion en software --- */
        if (alarma_irq && modo != MODE_ALARM) {
            alarma_irq = 0;
            modo = MODE_ALARM;
            Melody_start(MELODIA_LAVADORA, MELODIA_LAVADORA_N, 1);
            led_set(1, 0);
            pintar("*** ALARMA ***  ", "# para apagar   ");
        }

        tick_20ms++;
        if (tick_20ms >= 20) {
            char key;

            tick_20ms = 0;
            key = Keypad_scan();

            if (key != 0 && last_key == 0) {
                switch (modo) {

                case MODE_ALARM:
                    if (key == '#') {
                        Melody_stop();
                        DS3231_clearAlarmFlag();
                        led_set(0, 0);
                        modo = MODE_NORMAL;
                        LCD_clear();
                    }
                    break;

                case MODE_NORMAL:
                    if (key == 'A') {
                        modo = MODE_MENU;
                        led_set(0, 1);
                        pantallaMenu(alm_on);
                    }
                    break;

                case MODE_MENU:
                    if (key == '1') {
                        modo = MODE_SET_TIME; buffer = 0; ndig = 0;
                        pantallaEntrada("SET HORA  HHMMSS", 0, 6);
                    } else if (key == '2') {
                        modo = MODE_SET_DATE; buffer = 0; ndig = 0;
                        pantallaEntrada("SET FECHA DDMMAA", 0, 6);
                    } else if (key == '3') {
                        modo = MODE_SET_ALARM; buffer = 0; ndig = 0;
                        pantallaEntrada("SET ALARMA HHMM ", 0, 4);
                    } else if (key == '4') {
                        alm_on = (uint8_t)(!alm_on);
                        DS3231_alarmEnable(alm_on);
                        DS3231_clearAlarmFlag();
                        pantallaMenu(alm_on);
                    } else if (key == '5') {
                        modo = MODE_VIEW_INA;
                        LCD_clear();
                    } else if (key == '*') {
                        modo = MODE_NORMAL;
                        led_set(0, 0);
                        LCD_clear();
                    }
                    break;

                case MODE_VIEW_INA:
                    if (key == '*') {
                        modo = MODE_MENU;
                        pantallaMenu(alm_on);
                    }
                    break;

                case MODE_SET_TIME:
                    if (key >= '0' && key <= '9') {
                        if (ndig < 6) {
                            buffer = (buffer * 10u) + (uint32_t)(key - '0');
                            ndig++;
                            pantallaEntrada("SET HORA  HHMMSS", buffer, 6);
                        }
                    } else if (key == '*') {
                        modo = MODE_MENU; pantallaMenu(alm_on);
                    } else if (key == '#' && ndig == 6) {
                        if (DS3231_get_time(&nueva) == ERR_NONE) {
                            nueva.hours   = (uint8_t)(buffer / 10000u);
                            nueva.minutes = (uint8_t)((buffer / 100u) % 100u);
                            nueva.seconds = (uint8_t)(buffer % 100u);
                            DS3231_set_time(&nueva);
                        }
                        modo = MODE_MENU; pantallaMenu(alm_on);
                    }
                    break;

                case MODE_SET_DATE:
                    if (key >= '0' && key <= '9') {
                        if (ndig < 6) {
                            buffer = (buffer * 10u) + (uint32_t)(key - '0');
                            ndig++;
                            pantallaEntrada("SET FECHA DDMMAA", buffer, 6);
                        }
                    } else if (key == '*') {
                        modo = MODE_MENU; pantallaMenu(alm_on);
                    } else if (key == '#' && ndig == 6) {
                        if (DS3231_get_time(&nueva) == ERR_NONE) {
                            nueva.day   = (uint8_t)(buffer / 10000u);
                            nueva.month = (uint8_t)((buffer / 100u) % 100u);
                            nueva.year  = (uint16_t)(2000u + (buffer % 100u));
                            DS3231_set_time(&nueva);
                        }
                        modo = MODE_MENU; pantallaMenu(alm_on);
                    }
                    break;

                case MODE_SET_ALARM:
                    if (key >= '0' && key <= '9') {
                        if (ndig < 4) {
                            buffer = (buffer * 10u) + (uint32_t)(key - '0');
                            ndig++;
                            pantallaEntrada("SET ALARMA HHMM ", buffer, 4);
                        }
                    } else if (key == '*') {
                        modo = MODE_MENU; pantallaMenu(alm_on);
                    } else if (key == '#' && ndig == 4) {
                        alm_h = (uint8_t)(buffer / 100u);
                        alm_m = (uint8_t)(buffer % 100u);
                        DS3231_setAlarm1(alm_h, alm_m, 0);
                        DS3231_clearAlarmFlag();
                        modo = MODE_MENU; pantallaMenu(alm_on);
                    }
                    break;

                default:
                    break;
                }
            }
            last_key = key;
        }

        tick_500ms++;
        if (tick_500ms >= 500) {
            tick_500ms = 0;

            if (modo == MODE_VIEW_INA) {
                canal.bus_mV = 0;
                canal.current_mA = 0;
                if (ina_ok) {
                    INA3221_read_channel(3, &canal);
                }
                pantallaINA(&canal);
            }

            if (modo == MODE_NORMAL) {
                if (DS3231_get_time(&hora) == ERR_NONE) {
                    canal.bus_mV = 0;
                    if (ina_ok) {
                        INA3221_read_channel(3, &canal);
                    }
                    pantallaNormal(&hora, canal.bus_mV, alm_on);
                    MAX7219_showTime(hora.hours, hora.minutes);
                } else {
                    pintar("Error lectura   ", "RTC             ");
                }
            }
        }
    }
}
