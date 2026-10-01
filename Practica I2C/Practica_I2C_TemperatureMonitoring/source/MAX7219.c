#include "MAX7219.h"
#include "SPI.h"

void MAX7219_write(uint8_t command, uint8_t data)
{
    SPI0_cs_low();
    (void)SPI0_transfer(command);
    (void)SPI0_transfer(data);
    SPI0_cs_high();
}

void MAX7219_init(void)
{
    MAX7219_write(MAX7219_DECODE,    0x0F);
    MAX7219_write(MAX7219_SCANLIMIT, 3);
    MAX7219_write(MAX7219_INTENSITY, 4);
    MAX7219_write(MAX7219_TEST,      0);
    MAX7219_write(MAX7219_SHUTDOWN,  1);
}

void MAX7219_showTime(uint8_t hours, uint8_t minutes)
{
    MAX7219_write(0x01, (uint8_t)(hours / 10u));
    /* Se utiliza un OR bit a bit con 0x80 para encender el punto decimal en este display */
    MAX7219_write(0x02, (uint8_t)((hours % 10u) | 0x80u));
    MAX7219_write(0x03, (uint8_t)(minutes / 10u));
    MAX7219_write(0x04, (uint8_t)(minutes % 10u));
}
