#include "RTC.h"
#include "I2C.h"

static uint8_t bcd2bin(uint8_t v)
{
    return (uint8_t)(((v >> 4) * 10u) + (v & 0x0Fu));
}

static uint8_t bin2bcd(uint8_t v)
{
    return (uint8_t)(((v / 10u) << 4) | (v % 10u));
}

/* El DS3231 no tiene registro de identificacion: la deteccion consiste en
 * comprobar que el chip reconoce su propia direccion con un ACK. */
uint8_t DS3231_check(void)
{
    uint8_t b;
    int cnt = 0;

    return (I2C1_burstRead(DS3231_ADDR, DS3231_REG_CONTROL, 1, &b, &cnt) == ERR_NONE) ? 1u : 0u;
}

int DS3231_init(void)
{
    uint8_t h;
    int cnt = 0;
    int err;

    err = I2C1_write_reg8(DS3231_ADDR, DS3231_REG_CONTROL, DS3231_CONTROL_DEFAULT);
    if (err != ERR_NONE) return err;

    /* Fuerza modo 24 h sin tocar el resto del registro de horas */
    err = I2C1_burstRead(DS3231_ADDR, DS3231_REG_HOURS, 1, &h, &cnt);
    if (err != ERR_NONE) return err;

    if (h & DS3231_HOURS_12H) {
        uint8_t hh = bcd2bin((uint8_t)(h & 0x1Fu));

        if (h & DS3231_HOURS_PM) {
            if (hh != 12u) hh = (uint8_t)(hh + 12u);
        } else {
            if (hh == 12u) hh = 0u;
        }
        err = I2C1_write_reg8(DS3231_ADDR, DS3231_REG_HOURS, bin2bcd(hh));
        if (err != ERR_NONE) return err;
    }

    return ERR_NONE;
}

int DS3231_get_time(RTC_Time_t *t)
{
    uint8_t r[7];
    int cnt = 0;
    int err;
    uint8_t h;

    /* Los 7 registros en una sola transaccion: si se leyeran de uno en uno el
     * reloj podria avanzar entre lecturas y devolver una hora incoherente. */
    err = I2C1_burstRead(DS3231_ADDR, DS3231_REG_SECONDS, 7, r, &cnt);
    if (err != ERR_NONE) return err;

    t->seconds = bcd2bin((uint8_t)(r[0] & 0x7Fu));
    t->minutes = bcd2bin((uint8_t)(r[1] & 0x7Fu));

    h = r[2];
    if (h & DS3231_HOURS_12H) {
        uint8_t hh = bcd2bin((uint8_t)(h & 0x1Fu));

        if (h & DS3231_HOURS_PM) {
            if (hh != 12u) hh = (uint8_t)(hh + 12u);
        } else {
            if (hh == 12u) hh = 0u;
        }
        t->hours = hh;
    } else {
        t->hours = bcd2bin((uint8_t)(h & 0x3Fu));
    }

    t->weekday = (uint8_t)(r[3] & 0x07u);
    t->day     = bcd2bin((uint8_t)(r[4] & 0x3Fu));
    t->month   = bcd2bin((uint8_t)(r[5] & 0x1Fu));   /* bit 7 = siglo */
    t->year    = (uint16_t)(2000u + bcd2bin(r[6]));

    return ERR_NONE;
}

int DS3231_set_time(const RTC_Time_t *t)
{
    uint8_t r[7];

    r[0] = bin2bcd(t->seconds);
    r[1] = bin2bcd(t->minutes);
    r[2] = bin2bcd(t->hours);            /* bit 6 a 0 -> modo 24 h */
    r[3] = (uint8_t)(t->weekday & 0x07u);
    r[4] = bin2bcd(t->day);
    r[5] = bin2bcd(t->month);
    r[6] = bin2bcd((uint8_t)(t->year % 100u));

    return I2C1_burstWrite(DS3231_ADDR, DS3231_REG_SECONDS, 7, r);
}

uint8_t DS3231_lost_power(void)
{
    uint8_t s;
    int cnt = 0;

    if (I2C1_burstRead(DS3231_ADDR, DS3231_REG_STATUS, 1, &s, &cnt) != ERR_NONE) {
        return 1u;
    }
    return (s & DS3231_STATUS_OSF) ? 1u : 0u;
}

int DS3231_clear_osf(void)
{
    uint8_t s;
    int cnt = 0;
    int err;

    err = I2C1_burstRead(DS3231_ADDR, DS3231_REG_STATUS, 1, &s, &cnt);
    if (err != ERR_NONE) return err;

    return I2C1_write_reg8(DS3231_ADDR, DS3231_REG_STATUS,
                           (uint8_t)(s & (uint8_t)~DS3231_STATUS_OSF));
}

int DS3231_get_temp_c100(int16_t *c100)
{
    uint8_t r[2];
    int cnt = 0;
    int err;

    err = I2C1_burstRead(DS3231_ADDR, DS3231_REG_TEMP_MSB, 2, r, &cnt);
    if (err != ERR_NONE) return err;

    /* 10 bits en complemento a 2: parte entera en MSB, fraccion en LSB[7:6] a 0.25 C */
    *c100 = (int16_t)(((int16_t)(int8_t)r[0] * 100) + (int16_t)((r[1] >> 6) * 25u));
    return ERR_NONE;
}
