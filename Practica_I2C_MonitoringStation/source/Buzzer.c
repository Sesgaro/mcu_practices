#include <MKL25Z4.h>
#include "Buzzer.h"

#define TPM_SRC_HZ     20971520u
#define TPM_PRESCALE   8u          /* PS = 3 */

static void buzzer_setMod(uint16_t hz)
{
    uint32_t mod;

    if (hz == 0u) hz = 1u;
    mod = (TPM_SRC_HZ / TPM_PRESCALE) / (uint32_t)hz;

    if (mod < 2u)       mod = 2u;
    if (mod > 0x10000u) mod = 0x10000u;

    TPM2->MOD = (uint16_t)(mod - 1u);
}

void BUZZER_init(void)
{
    SIM->SCGC6 |= SIM_SCGC6_TPM2_MASK;
    SIM->SOPT2  = (SIM->SOPT2 & ~0x03000000u) | 0x01000000u;   /* TPMSRC = MCGFLLCLK */

    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    PORTB->PCR[2] = PORT_PCR_MUX(3);        /* PTB2 = TPM2_CH0 */

    TPM2->SC  = 0;
    TPM2->CNT = 0;
    buzzer_setMod(BUZZER_TONE_HZ);

    TPM2->CONTROLS[0].CnSC = 0x28;          /* PWM alineado a flanco, pulso alto */
    TPM2->CONTROLS[0].CnV  = 0;             /* arranca en silencio */

    TPM2->SC = 0x08u | 0x03u;               /* CMOD = reloj interno, PS = /8 */
}

void BUZZER_on(void)
{
    TPM2->CONTROLS[0].CnV = (uint16_t)((TPM2->MOD + 1u) / 2u);   /* 50 % */
}

void BUZZER_off(void)
{
    TPM2->CONTROLS[0].CnV = 0;
}

void BUZZER_tone(uint16_t hz)
{
    uint8_t sonando = (TPM2->CONTROLS[0].CnV != 0u) ? 1u : 0u;

    buzzer_setMod(hz);
    if (sonando) BUZZER_on();
}
