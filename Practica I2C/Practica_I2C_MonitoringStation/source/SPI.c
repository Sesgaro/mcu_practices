#include <MKL25Z4.h>
#include "SPI.h"

void SPI0_init(void)
{
    SIM->SCGC5 |= 0x1000;
    PORTD->PCR[1] = 0x200;   /* PTD1 como SPI SCK */
    PORTD->PCR[2] = 0x200;   /* PTD2 como SPI MOSI */
    PORTD->PCR[0] = 0x100;   /* PTD0 como GPIO (Chip Select) */

    PTD->PDDR |= 0x01;
    PTD->PSOR  = 0x01;

    SIM->SCGC4 |= 0x400000;
    SPI0->C1 = 0x1C;
    SPI0->BR = 0x60;
    SPI0->C1 |= 0x40;
}

uint8_t SPI0_transfer(uint8_t b)
{
    while (!(SPI0->S & 0x20)) { }
    SPI0->D = b;
    while (!(SPI0->S & 0x80)) { }
    return SPI0->D;
}

void SPI0_cs_low(void)
{
    PTD->PCOR = 0x01;
}

void SPI0_cs_high(void)
{
    PTD->PSOR = 0x01;
}
