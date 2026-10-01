#include "Melody.h"
#include "Buzzer.h"

/* Washing Machine Song - La mayor, 4/4, negra = 200.
 * Una linea por compas para poder corregir notas sueltas. */
const Nota_t MELODIA_LAVADORA[] = {
    {A4,T_4},{B4,T_4},{CS5,T_4},{CS5,T_4},                                  /* 1  */
    {A4,T_2},{FS4,T_4},{FS4,T_4},                                           /* 2  */
    {FS4,T_4P},{GS4,T_8},{A4,T_8},{B4,T_8},{A4,T_8},{GS4,T_8},              /* 3  */
    {FS4,T_2P},{A4,T_4},                                                    /* 4  */
    {B4,T_4},{B4,T_4},{CS5,T_4},{CS5,T_4},                                  /* 5  */
    {A4,T_2},{FS4,T_4},{A4,T_4},                                            /* 6  */
    {B4,T_4},{A4,T_8},{GS4,T_8},{FS4,T_4},{DS5,T_4},                        /* 7  */
    {E5,T_2P},{E5,T_4},                                                     /* 8  */
    {FS5,T_4},{E5,T_4},{D5,T_8},{CS5,T_8},{B4,T_8},{A4,T_8},                /* 9  */
    {B4,T_2},{A4,T_4},{B4,T_4},                                             /* 10 */
    {CS5,T_4},{B4,T_4},{E5,T_8},{D5,T_8},{CS5,T_8},{B4,T_8},                /* 11 */
    {A4,T_2P},{A4,T_4},                                                     /* 12 */
    {A4,T_4},{B4,T_4},{CS5,T_4},{CS5,T_4},                                  /* 13 */
    {A4,T_2},{FS4,T_4},{GS4,T_4},                                           /* 14 */
    {A4,T_4P},{B4,T_8},{CS5,T_4},{A4,T_4},                                  /* 15 */
    {A4,T_2P},{CS5,T_4},                                                    /* 16 */
    {B4,T_8},{A4,T_8},{GS4,T_4},{A4,T_8},{B4,T_8},{CS5,T_8},{D5,T_8},       /* 17 */
    {E5,T_2},{A4,T_4},{GS4,T_4},                                            /* 18 */
    {FS4,T_4P},{A4,T_8},{B4,T_4},{A4,T_4},                                  /* 19 */
    {A4,T_2P},{SIL,T_4}                                                     /* 20 */
};

const uint16_t MELODIA_LAVADORA_N =
    (uint16_t)(sizeof(MELODIA_LAVADORA) / sizeof(MELODIA_LAVADORA[0]));

static const Nota_t *mel;
static uint16_t mel_n;
static uint16_t mel_i;
static uint16_t mel_ms;
static uint16_t mel_gap;
static uint8_t  mel_rep;

static void mel_arrancarNota(void)
{
    uint16_t hz = mel[mel_i].hz;
    uint16_t ms = mel[mel_i].ms;

    if (hz == 0u) {
        BUZZER_off();
    } else {
        BUZZER_tone((uint16_t)(hz << MELODY_SHIFT));
        BUZZER_on();
    }

    mel_ms  = ms;
    mel_gap = (ms > 60u) ? 30u : (uint16_t)(ms / 4u);   /* silencio final: separa notas repetidas */
}

void Melody_start(const Nota_t *notas, uint16_t n, uint8_t repetir)
{
    if (notas == 0 || n == 0u) return;

    mel     = notas;
    mel_n   = n;
    mel_i   = 0;
    mel_rep = repetir;
    mel_arrancarNota();
}

void Melody_stop(void)
{
    mel = 0;
    BUZZER_off();
}

uint8_t Melody_playing(void)
{
    return (mel != 0) ? 1u : 0u;
}

void Melody_tick(void)
{
    if (mel == 0) return;

    if (mel_ms > 0u) {
        mel_ms--;
        if (mel_ms == mel_gap) {
            BUZZER_off();
        }
        return;
    }

    mel_i++;
    if (mel_i >= mel_n) {
        if (!mel_rep) {
            Melody_stop();
            return;
        }
        mel_i = 0;
    }
    mel_arrancarNota();
}
