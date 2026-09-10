#include <MKL25Z4.H>

#define DECODE 9
#define INTENSITY 10
#define SCANLIMIT 11
#define SHUTDOWN 12
#define TEST 15

void SPI0_init(void);
void max7219_write(unsigned char command, unsigned char data);
void buttons_init(void);
void display_update(int counter_val);
void delay_ms(int ms);

int main(void) {

    SPI0_init();
    buttons_init();

    max7219_write(DECODE, 0x0F);
    max7219_write(SCANLIMIT, 3);
    max7219_write(INTENSITY, 4);
    max7219_write(TEST, 0);
    max7219_write(SHUTDOWN, 1);


    int count = 0;
    int tick = 0;
    int last_b_man = 1, last_b_rst = 1;

    display_update(count);

    while(1) {

        int is_playing = !(PTA->PDIR & (1<<1));
        int is_incrementing = !(PTA->PDIR & (1<<2));

        int b_man  = (PTA->PDIR & (1<<4)) ? 1 : 0;
        int b_rst  = (PTA->PDIR & (1<<5)) ? 1 : 0;

        if (last_b_man == 1 && b_man == 0) {
            delay_ms(20);
            if (!(PTA->PDIR & (1<<4))) {
                if (!is_playing) {
                    if (is_incrementing) {
                        count = (count + 1) % 10000;
                    } else {
                        count = (count == 0) ? 9999 : count - 1;
                    }
                    display_update(count);
                }
            }
        }
        last_b_man = b_man;

        if (last_b_rst == 1 && b_rst == 0) {
            delay_ms(20);
            if (!(PTA->PDIR & (1<<5))) {
                count = is_incrementing ? 0 : 9999;
                display_update(count);
            }
        }
        last_b_rst = b_rst;

        if (is_playing) {
            tick++;
            if (tick >= 25) {
                tick = 0;

                if (is_incrementing) {
                    count = (count + 1) % 10000;
                } else {
                    count = (count == 0) ? 9999 : count - 1;
                }
                display_update(count);
            }
        }


        delay_ms(20);
    }
}

void SPI0_init(void) {
    SIM->SCGC5 |= 0x1000;    /* Habilitar reloj para el Puerto D */
    PORTD->PCR[1] = 0x200;   /* PTD1 como SPI SCK */
    PORTD->PCR[2] = 0x200;   /* PTD2 como SPI MOSI */
    PORTD->PCR[0] = 0x100;   /* PTD0 como GPIO (Chip Select) */

    PTD->PDDR |= 0x01;       /* PTD0 como salida */
    PTD->PSOR = 0x01;        /* PTD0 en alto por defecto (Inactivo) */

    SIM->SCGC4 |= 0x400000;  /* Habilitar reloj para SPI0 */
    SPI0->C1 = 0x10;         /* Deshabilitar SPI y ponerlo como Master */
    SPI0->BR = 0x60;         /* Configurar Baud rate a 1 MHz */
    SPI0->C1 |= 0x40;        /* Habilitar módulo SPI */
}

void buttons_init(void) {
    SIM->SCGC5 |= 0x200;

    /* Configurar como GPIO (MUX=1) y habilitar Pull-Up interno (PE=1, PS=1) -> 0x103 */
    PORTA->PCR[1] = 0x103;
    PORTA->PCR[2] = 0x103;
    PORTA->PCR[4] = 0x103;
    PORTA->PCR[5] = 0x103;

    PTA->PDDR &= ~0x36;
}

void max7219_write(unsigned char command, unsigned char data){
    volatile char dummy;
    PTD->PCOR = 1;               /* Bajar CS */

    while(!(SPI0->S & 0x20)) { } /* Esperar a que tx esté listo */
    SPI0->D = command;           /* Enviar comando */
    while(!(SPI0->S & 0x80)) { } /* Esperar a que se complete tx */
    dummy = SPI0->D;             /* Limpiar bandera SPRF */

    while(!(SPI0->S & 0x20)) { } /* Esperar a que tx esté listo */
    SPI0->D = data;              /* Enviar datos */
    while(!(SPI0->S & 0x80)) { } /* Esperar a que se complete tx */
    dummy = SPI0->D;             /* Limpiar bandera SPRF */

    PTD->PSOR = 1;               /* Subir CS */
}

void display_update(int counter_val) {
    int units = counter_val % 10;
    int tens = (counter_val / 10) % 10;
    int hundreds = (counter_val / 100) % 10;
    int thousands = (counter_val / 1000) % 10;

    max7219_write(0x01, thousands); /* Dígito de la extrema izquierda */
    max7219_write(0x02, hundreds);
    max7219_write(0x03, tens);
    max7219_write(0x04, units);     /* Dígito de la extrema derecha */
}

void delay_ms(int ms) {
    SysTick->LOAD = 20970 - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = 5;

    for (int i = 0; i < ms; i++) {
        while((SysTick->CTRL & 0x10000) == 0);
    }
    SysTick->CTRL = 0;
}
