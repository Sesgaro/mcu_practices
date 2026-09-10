#include <MKL25Z4.H>

#define DECODE 9
#define INTENSITY 10
#define SCANLIMIT 11
#define SHUTDOWN 12
#define TEST 15


int current_state = 0;

void SPI0_init(void);
void max7219_write(unsigned char command, unsigned char data);
void hardware_init(void);
void rgb_set(TimerState state);
char keypad_scan(void);
void display_time(int total_seconds);
void display_raw(int digits);

int configured_time = 0;
int current_time = 0;

int input_buffer = 0;
int input_digits = 0;

int main(void) {

    SPI0_init();
    hardware_init();

    max7219_write(DECODE, 0x0F);
    max7219_write(SCANLIMIT, 3);
    max7219_write(INTENSITY, 4);
    max7219_write(TEST, 0);
    max7219_write(SHUTDOWN, 1);

    rgb_set(0);
    display_time(configured_time);


    SysTick->LOAD = 20970 - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = 5;

    int tick_1ms = 0;
    int tick_1000ms = 0;
    int last_pb1 = 1, last_pb2 = 1;
    char last_key = 0;

    while(1) {
        if (SysTick->CTRL & 0x10000) {
            tick_1ms++;

            if (tick_1ms >= 20) {
                tick_1ms = 0;

                int pb1 = (PTA->PDIR & (1<<1)) ? 1 : 0;
                int pb2 = (PTA->PDIR & (1<<2)) ? 1 : 0;

                if (last_pb1 == 1 && pb1 == 0) {
                    if (current_state == 0 && current_time > 0) {
                        current_state = 1;
                        rgb_set(1);
                        tick_1000ms = 0;
                    } else if (current_state == 1) {
                        current_state = 0;
                        rgb_set(0);
                    }
                }
                last_pb1 = pb1;

                if (last_pb2 == 1 && pb2 == 0) {
                    current_state = 0;
                    current_time = configured_time;
                    rgb_set(0);
                    display_time(current_time);
                    input_buffer = 0;
                    input_digits = 0;
                }
                last_pb2 = pb2;

                char key = keypad_scan();
                if (key != 0 && last_key == 0 && current_state != 1) {

                    if (key >= '0' && key <= '9') {
                        if (input_digits < 4) {
                            input_buffer = (input_buffer * 10) + (key - '0');
                            input_digits++;
                            display_raw(input_buffer);
                        }
                    }
                    else if (key == '*') {
                        input_buffer = 0;
                        input_digits = 0;
                        display_time(configured_time);
                    }
                    else if (key == '#') {
                        if (input_digits == 4) {
                            int mm = input_buffer / 100;
                            int ss = input_buffer % 100;
                            if (ss <= 59) {
                                configured_time = (mm * 60) + ss;
                                current_time = configured_time;
                                current_state = 0;
                                rgb_set(0);
                            }
                        }
                        display_time(current_time);
                        input_buffer = 0;
                        input_digits = 0;
                    }
                }
                last_key = key;
            }


            if (current_state == 1) {
                tick_1000ms++;
                if (tick_1000ms >= 1000) {
                    tick_1000ms = 0;
                    if (current_time > 0) {
                        current_time--;
                        display_time(current_time);

                        if (current_time == 0) {
                            current_state = 2;
                            rgb_set(2);
                        }
                    }
                }
            }
        }
    }
}

void SPI0_init(void) {
    SIM->SCGC5 |= 0x1000;    /* Habilitar reloj para el Puerto D */
    PORTD->PCR[1] = 0x200;   /* PTD1 como SPI SCK (Aquí está el LED Azul) */
    PORTD->PCR[2] = 0x200;   /* PTD2 como SPI MOSI */
    PORTD->PCR[0] = 0x100;   /* PTD0 como GPIO (Chip Select) */

    PTD->PDDR |= 0x01;       /* PTD0 como salida */
    PTD->PSOR = 0x01;        /* PTD0 en alto por defecto */

    SIM->SCGC4 |= 0x400000;  /* Habilitar reloj para SPI0 */
    SPI0->C1 = 0x1C;         /* Deshabilitar SPI, Master, CPOL=1, CPHA=1 */
    SPI0->BR = 0x60;         /* Configurar Baud rate a 1 MHz */
    SPI0->C1 |= 0x40;        /* Habilitar módulo SPI */
}

void hardware_init(void) {
    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTC_MASK;
/*OTONES*/
    PORTA->PCR[1] = 0x103;
    PORTA->PCR[2] = 0x103;
    PTA->PDDR &= ~((1<<1) | (1<<2));

/*LEDRGB*/
    PORTB->PCR[18] = 0x100;
    PORTB->PCR[19] = 0x100;
    PTB->PDDR |= (1<<18) | (1<<19);
    PTB->PSOR = (1<<18) | (1<<19);


    for(int i = 0; i < 4; i++) {
        PORTC->PCR[i] = 0x100;
    }
    PTC->PDDR |= 0x0F;
    PTC->PSOR = 0x0F;

    for(int i = 4; i < 8; i++) {
        PORTC->PCR[i] = 0x103;
    }
    PTC->PDDR &= ~0xF0;
}

void rgb_set(TimerState state) {

    PTB->PSOR = (1<<18) | (1<<19);

    if (state == 0) {
        PTB->PCOR = (1<<18) | (1<<19); /* Rojo + Verde = Amarillo */
    } else if (state == 1) {
        PTB->PCOR = (1<<19);           /* Solo Verde */
    } else if (state == 2) {
        PTB->PCOR = (1<<18);           /* Solo Rojo */
    }
}

char keypad_scan(void) {

    char keys[4][4] = {
        {'1','2','3','A'},
        {'4','5','6','B'},
        {'7','8','9','C'},
        {'*','0','#','D'}
    };

    for(int r = 0; r < 4; r++) {
        PTC->PCOR = (1<<r);

        for(int c = 0; c < 4; c++) {
            if (!(PTC->PDIR & (1<<(c+4)))) {
                PTC->PSOR = (1<<r);
                return keys[r][c];
            }
        }
        PTC->PSOR = (1<<r);
    }
    return 0;
}

void max7219_write(unsigned char command, unsigned char data){
    volatile char dummy;
    PTD->PCOR = 1;
    while(!(SPI0->S & 0x20)) { }
    SPI0->D = command;
    while(!(SPI0->S & 0x80)) { }
    dummy = SPI0->D;
    while(!(SPI0->S & 0x20)) { }
    SPI0->D = data;
    while(!(SPI0->S & 0x80)) { }
    dummy = SPI0->D;
    PTD->PSOR = 1;
}

/* Convierte segundos en formato MMSS (sin punto decimal) */
void display_time(int total_seconds) {
    int mm = total_seconds / 60;
    int ss = total_seconds % 60;

    int m_tens = (mm / 10) % 10;
    int m_units = mm % 10;
    int s_tens = (ss / 10) % 10;
    int s_units = ss % 10;

    max7219_write(0x01, m_tens);
    max7219_write(0x02, m_units);
    max7219_write(0x03, s_tens);
    max7219_write(0x04, s_units);
}


void display_raw(int value) {
    int units = value % 10;
    int tens = (value / 10) % 10;
    int hundreds = (value / 100) % 10;
    int thousands = (value / 1000) % 10;

    max7219_write(0x01, thousands);
    max7219_write(0x02, hundreds);
    max7219_write(0x03, tens);
    max7219_write(0x04, units);
}
