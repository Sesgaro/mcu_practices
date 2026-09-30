#ifndef _SPI_H_
#define _SPI_H_

#include <stdint.h>

void    SPI0_init(void);
uint8_t SPI0_transfer(uint8_t b);
void    SPI0_cs_low(void);
void    SPI0_cs_high(void);

#endif /* _SPI_H_ */
