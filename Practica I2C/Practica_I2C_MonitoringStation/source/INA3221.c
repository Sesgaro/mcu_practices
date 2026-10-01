#include "INA3221.h"
#include "I2C.h"

uint16_t INA3221_readRegister(uint8_t reg)
{
    uint8_t raw[2];
    int cnt = 0;

    if (I2C1_burstRead(INA3221_ADDR, reg, 2, raw, &cnt) == ERR_NONE) {
        return (uint16_t)((raw[0] << 8) | raw[1]);
    }
    return 0xFFFFu;
}

uint8_t INA3221_check(void)
{
    return (INA3221_readRegister(INA3221_REG_MANUFACTURER_ID) == INA3221_MANUF_ID) ? 1u : 0u;
}

void INA3221_init(void)
{
    I2C1_write_reg16(INA3221_ADDR, INA3221_REG_CONFIG, INA3221_CONFIG_DEFAULT);
}

void INA3221_read_channel(uint8_t channel, INA_Data_t *data)
{
    uint8_t reg_shunt, reg_bus;
    uint16_t busRaw, shuntRaw;
    int16_t bus_s, shunt_s;

    switch (channel) {
    case 1: reg_shunt = INA3221_REG_SHUNTVOLTAGE_1; reg_bus = INA3221_REG_BUSVOLTAGE_1; break;
    case 2: reg_shunt = INA3221_REG_SHUNTVOLTAGE_2; reg_bus = INA3221_REG_BUSVOLTAGE_2; break;
    case 3: reg_shunt = INA3221_REG_SHUNTVOLTAGE_3; reg_bus = INA3221_REG_BUSVOLTAGE_3; break;
    default: return;
    }

    busRaw   = INA3221_readRegister(reg_bus);
    shuntRaw = INA3221_readRegister(reg_shunt);

    bus_s   = (int16_t)((int16_t)busRaw   >> 3);   /* bits [2:0] no se usa */
    shunt_s = (int16_t)((int16_t)shuntRaw >> 3);

    data->bus_mV     = (int32_t)bus_s   * 8;       /* LSB = 8 mV  */
    data->shunt_uV   = (int32_t)shunt_s * 40;      /* LSB = 40 uV */
    data->current_mA = data->shunt_uV / R_SHUNT_mOHM;    /* I(mA) = V(uV) / R(mOhm) */
}
