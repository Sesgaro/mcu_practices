#ifndef _RTC_H_
#define _RTC_H_

#include <stdint.h>

#define DS3231_ADDR             0x68u   /* fija, el chip no tiene pines de direccion */

#define DS3231_REG_SECONDS      0x00u
#define DS3231_REG_MINUTES      0x01u
#define DS3231_REG_HOURS        0x02u
#define DS3231_REG_WEEKDAY      0x03u
#define DS3231_REG_DAY          0x04u
#define DS3231_REG_MONTH        0x05u
#define DS3231_REG_YEAR         0x06u
#define DS3231_REG_ALARM1_SEC   0x07u
#define DS3231_REG_ALARM1_MIN    0x08u
#define DS3231_REG_ALARM1_HOUR   0x09u
#define DS3231_REG_ALARM1_DAY    0x0Au
#define DS3231_REG_CONTROL      0x0Eu
#define DS3231_REG_STATUS       0x0Fu
#define DS3231_REG_TEMP_MSB     0x11u
#define DS3231_REG_TEMP_LSB     0x12u

/* Control: oscilador activo, salida cuadrada apagada, alarmas deshabilitadas */
#define DS3231_CONTROL_DEFAULT  0x1Cu

#define DS3231_STATUS_OSF       0x80u   /* Oscillator Stop Flag */
#define DS3231_STATUS_A1F       0x01u   /* bandera de la alarma 1 */

#define DS3231_CTRL_INTCN       0x04u   /* INT/SQW es salida de interrupcion */
#define DS3231_CTRL_A1IE        0x01u   /* habilita la interrupcion de alarma 1 */

#define DS3231_A1_MASK          0x80u   /* bit A1Mx de cada registro de alarma */

#define DS3231_HOURS_12H        0x40u   /* 1 = modo 12 h */
#define DS3231_HOURS_PM         0x20u   /* solo valido en modo 12 h */

typedef struct {
    uint8_t  seconds;   /* 0-59  */
    uint8_t  minutes;   /* 0-59  */
    uint8_t  hours;     /* 0-23  */
    uint8_t  weekday;   /* 1-7   */
    uint8_t  day;       /* 1-31  */
    uint8_t  month;     /* 1-12  */
    uint16_t year;      /* 2000-2099 */
} RTC_Time_t;

uint8_t DS3231_check(void);
int     DS3231_init(void);
int     DS3231_get_time(RTC_Time_t *t);
int     DS3231_set_time(const RTC_Time_t *t);
uint8_t DS3231_lost_power(void);
int     DS3231_clear_osf(void);
int     DS3231_get_temp_c100(int16_t *c100);

/* Alarma 1, coincidencia diaria en hora:minuto:segundo. El DS3231 baja
 * su pin INT/SQW al dispararse y lo mantiene hasta que se limpia A1F. */
int     DS3231_setAlarm1(uint8_t hours, uint8_t minutes, uint8_t seconds);
int     DS3231_getAlarm1(uint8_t *hours, uint8_t *minutes, uint8_t *seconds);
int     DS3231_alarmEnable(uint8_t on);
uint8_t DS3231_alarmEnabled(void);
uint8_t DS3231_alarmFired(void);
int     DS3231_clearAlarmFlag(void);

#endif /* _RTC_H_ */
