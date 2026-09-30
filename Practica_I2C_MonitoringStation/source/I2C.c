#include <MKL25Z4.h>
#include "I2C.h"

#define I2C_TIMEOUT_LOOPS  200000u

void I2C1_init(void)
{
    SIM->SCGC4 |= SIM_SCGC4_I2C1_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;

    PORTC->PCR[10] = PORT_PCR_MUX(2);   /* SCL */
    PORTC->PCR[11] = PORT_PCR_MUX(2);   /* SDA */

    I2C1->C1 = 0;
    I2C1->S  = I2C_S_IICIF_MASK;
    I2C1->F  = 0x1A;
    I2C1->C1 = I2C_C1_IICEN_MASK;
}

static void i2c1_stop(void)
{
    I2C1->C1 &= (uint8_t)~(I2C_C1_MST_MASK | I2C_C1_TX_MASK);
}

static int i2c1_wait_iicif(void)
{
    uint32_t t = I2C_TIMEOUT_LOOPS;

    while (!(I2C1->S & I2C_S_IICIF_MASK)) {
        if (--t == 0u) {
            i2c1_stop();
            return ERR_TIMEOUT;
        }
    }
    I2C1->S = I2C_S_IICIF_MASK;
    return ERR_NONE;
}

static int i2c1_wait_idle(void)
{
    int retry = 100;

    while (I2C1->S & I2C_S_BUSY_MASK) {
        if (--retry <= 0) return ERR_BUS_BUSY;
    }
    return ERR_NONE;
}

static int i2c1_start(uint8_t slaveAddr, uint8_t read)
{
    I2C1->C1 |= I2C_C1_TX_MASK;
    I2C1->C1 |= I2C_C1_MST_MASK;
    I2C1->D   = (uint8_t)((slaveAddr << 1) | (read ? 1u : 0u));
    if (i2c1_wait_iicif() != ERR_NONE)  return ERR_TIMEOUT;
    if (I2C1->S & I2C_S_ARBL_MASK)    { i2c1_stop(); return ERR_ARB_LOST; }
    if (I2C1->S & I2C_S_RXAK_MASK)    { i2c1_stop(); return ERR_NO_ACK;   }
    return ERR_NONE;
}

static int i2c1_send(uint8_t b)
{
    I2C1->D = b;
    if (i2c1_wait_iicif() != ERR_NONE)  return ERR_TIMEOUT;
    if (I2C1->S & I2C_S_RXAK_MASK)    { i2c1_stop(); return ERR_NO_ACK; }
    return ERR_NONE;
}

int I2C1_burstRead(uint8_t slaveAddr, uint8_t memAddr, int byteCount,
                   uint8_t *data, int *cnt)
{
    int err;

    *cnt = 0;

    err = i2c1_wait_idle();          if (err != ERR_NONE) return err;
    err = i2c1_start(slaveAddr, 0);  if (err != ERR_NONE) return err;
    err = i2c1_send(memAddr);        if (err != ERR_NONE) return err;

    I2C1->C1 |= I2C_C1_RSTA_MASK;
    I2C1->D   = (uint8_t)((slaveAddr << 1) | 1u);
    if (i2c1_wait_iicif() != ERR_NONE)  return ERR_TIMEOUT;
    if (I2C1->S & I2C_S_RXAK_MASK)    { i2c1_stop(); return ERR_NO_ACK; }

    I2C1->C1 &= (uint8_t)~(I2C_C1_TX_MASK | I2C_C1_TXAK_MASK);
    if (byteCount == 1) I2C1->C1 |= I2C_C1_TXAK_MASK;

    (void)I2C1->D;

    while (byteCount > 0) {
        if (byteCount == 1) I2C1->C1 |= I2C_C1_TXAK_MASK;
        if (i2c1_wait_iicif() != ERR_NONE) return ERR_TIMEOUT;
        if (byteCount == 1) I2C1->C1 &= (uint8_t)~I2C_C1_MST_MASK;

        *data++ = I2C1->D;
        byteCount--;
        (*cnt)++;
    }
    return ERR_NONE;
}

int I2C1_burstWrite(uint8_t slaveAddr, uint8_t memAddr, int byteCount,
                    const uint8_t *data)
{
    int err;

    err = i2c1_wait_idle();          if (err != ERR_NONE) return err;
    err = i2c1_start(slaveAddr, 0);  if (err != ERR_NONE) return err;
    err = i2c1_send(memAddr);        if (err != ERR_NONE) return err;

    while (byteCount > 0) {
        err = i2c1_send(*data++);
        if (err != ERR_NONE) return err;
        byteCount--;
    }

    i2c1_stop();
    return ERR_NONE;
}

int I2C1_writeBytes(uint8_t slaveAddr, const uint8_t *data, int byteCount)
{
    int err;

    err = i2c1_wait_idle();          if (err != ERR_NONE) return err;
    err = i2c1_start(slaveAddr, 0);  if (err != ERR_NONE) return err;

    while (byteCount > 0) {
        err = i2c1_send(*data++);
        if (err != ERR_NONE) return err;
        byteCount--;
    }

    i2c1_stop();
    return ERR_NONE;
}

int I2C1_write_reg8(uint8_t slaveAddr, uint8_t reg, uint8_t val)
{
    return I2C1_burstWrite(slaveAddr, reg, 1, &val);
}

int I2C1_write_reg16(uint8_t slaveAddr, uint8_t reg, uint16_t val)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFFu);
    return I2C1_burstWrite(slaveAddr, reg, 2, buf);
}
