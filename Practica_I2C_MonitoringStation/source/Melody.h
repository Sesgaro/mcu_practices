#ifndef _MELODY_H_
#define _MELODY_H_

#include <stdint.h>
#define MELODY_SHIFT   1

#define SIL     0
#define DS4     311
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

#define T_8      150    // corchea
#define T_4      300    // negra
#define T_4P     450    // negra con puntillo
#define T_2      600    // blanca
#define T_2P     900    // blanca con puntillo

typedef struct {
    uint16_t hz;
    uint16_t ms;
} Nota_t;

extern const Nota_t MELODIA_LAVADORA[];
extern const uint16_t MELODIA_LAVADORA_N;

void    Melody_start(const Nota_t *notas, uint16_t n, uint8_t repetir);
void    Melody_stop(void);
void    Melody_tick(void);
uint8_t Melody_playing(void);

#endif
