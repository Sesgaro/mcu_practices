#ifndef _I2C_H_
#define _I2C_H_

#include <stdint.h>

#define ERR_NONE      0
#define ERR_NO_ACK    1
#define ERR_ARB_LOST  2
#define ERR_BUS_BUSY  3
#define ERR_TIMEOUT   4

void I2C1_init(void);

int  I2C1_burstRead (uint8_t slaveAddr, uint8_t memAddr, int byteCount,
                     uint8_t *data, int *cnt);
int  I2C1_burstWrite(uint8_t slaveAddr, uint8_t memAddr, int byteCount,
                     const uint8_t *data);
int  I2C1_write_reg8 (uint8_t slaveAddr, uint8_t reg, uint8_t  val);
int  I2C1_write_reg16(uint8_t slaveAddr, uint8_t reg, uint16_t val);

int  I2C1_writeBytes(uint8_t slaveAddr, const uint8_t *data, int byteCount);

#endif
