#ifndef _MELODY_H_
#define _MELODY_H_

#include <stdint.h>

/* Notas en Hz. La melodia esta en La mayor (fa#, do#, sol#).
 * MELODY_SHIFT sube la melodia en octavas: un piezo suena mucho mas
 * fuerte cerca de su resonancia (2-4 kHz) que en la octava escrita. */
#define MELODY_SHIFT   1

#define SIL     0
#define E4      330
#define FS4     370
#define GS4     415
#define A4      440
#define B4      494
#define CS5     554
#define D5      587
#define DS5     622
#define E5      659
#define FS5     740
#define GS5     831
#define A5      880
#define B5      988
#define CS6    1109
#define D6     1175
#define E6     1319

/* Duraciones a negra = 200 bpm -> negra = 300 ms */
#define T_8      150    /* corchea            */
#define T_4      300    /* negra              */
#define T_4P     450    /* negra con puntillo */
#define T_2      600    /* blanca             */
#define T_2P     900    /* blanca con puntillo*/

typedef struct {
    uint16_t hz;    /* 0 = silencio */
    uint16_t ms;
} Nota_t;

extern const Nota_t MELODIA_LAVADORA[];
extern const uint16_t MELODIA_LAVADORA_N;

void    Melody_start(const Nota_t *notas, uint16_t n, uint8_t repetir);
void    Melody_stop(void);
void    Melody_tick(void);          /* llamar cada 1 ms */
uint8_t Melody_playing(void);

#endif /* _MELODY_H_ */
