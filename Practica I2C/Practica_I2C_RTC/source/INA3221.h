#ifndef _INA3221_H_
#define _INA3221_H_

#include <stdint.h>

#define INA3221_ADDR                0x40u   /* A0 a GND */

#define INA3221_REG_CONFIG          0x00u
#define INA3221_REG_SHUNTVOLTAGE_1  0x01u
#define INA3221_REG_BUSVOLTAGE_1    0x02u
#define INA3221_REG_SHUNTVOLTAGE_2  0x03u
#define INA3221_REG_BUSVOLTAGE_2    0x04u
#define INA3221_REG_SHUNTVOLTAGE_3  0x05u
#define INA3221_REG_BUSVOLTAGE_3    0x06u
#define INA3221_REG_MANUFACTURER_ID 0xFEu
#define INA3221_REG_DIE_ID          0xFFu

#define INA3221_CONFIG_DEFAULT      0x7127u
#define INA3221_MANUF_ID            0x5449u

#define R_SHUNT_mOHM                100     /* shunt R100 = 0.100 ohm */

typedef struct {
    int32_t bus_mV;
    int32_t shunt_uV;
    int32_t current_mA;
} INA_Data_t;

uint16_t INA3221_readRegister(uint8_t reg);
uint8_t  INA3221_check(void);
void     INA3221_init(void);
void     INA3221_read_channel(uint8_t channel, INA_Data_t *data);

#endif /* _INA3221_H_ */
