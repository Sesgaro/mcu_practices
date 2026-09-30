#include <MKL25Z4.h>
#include "Keypad.h"

/* PTC0..PTC3 filas (salida), PTC4..PTC7 columnas (entrada con pull-up) */
void Keypad_init(void)
{
    int i;

    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;

    for (i = 0; i < 4; i++) {
        PORTC->PCR[i] = 0x100;
    }
    PTC->PDDR |= 0x0F;
    PTC->PSOR  = 0x0F;

    for (i = 4; i < 8; i++) {
        PORTC->PCR[i] = 0x103;
    }
    PTC->PDDR &= ~0xF0;
}

char Keypad_scan(void)
{
    static const char keys[4][4] = {
        {'1','2','3','A'},
        {'4','5','6','B'},
        {'7','8','9','C'},
        {'*','0','#','D'}
    };
    int r, c;

    for (r = 0; r < 4; r++) {
        PTC->PCOR = (uint32_t)(1u << r);
        for (c = 0; c < 4; c++) {
            if (!(PTC->PDIR & (1u << (c + 4)))) {
                PTC->PSOR = (uint32_t)(1u << r);
                return keys[r][c];
            }
        }
        PTC->PSOR = (uint32_t)(1u << r);
    }
    return 0;
}
