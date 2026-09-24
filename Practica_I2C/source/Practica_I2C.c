#include <MKL25Z4.h>
#include <stdint.h>

const unsigned int CLK_HZ = 20970000;
const unsigned int BUS_HZ = 10485760;

#define ERR_NONE      0
#define ERR_NO_ACK    1
#define ERR_ARB_LOST  2
#define ERR_BUS_BUSY  3
#define ERR_TIMEOUT   4

#define I2C_TIMEOUT_LOOPS  200000u

#define INA3221_ADDR                0x40u
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

#define R_SHUNT_mOHM                100

typedef struct {
    int32_t bus_mV;
    int32_t shunt_uV;
    int32_t current_mA;
} INA_Data_t;

void UART0_init(void);
void UART0_putc(char c);
void UART0_puts(const char *str);
void UART0_putn(unsigned int n);
void UART0_putd(int32_t v);
void UART0_puthex16(uint16_t v);
void UART0_pad(int ancho, int usados);

void delayMs(int n);

void I2C1_init(void);
int  I2C1_burstRead(uint8_t slaveAddr, uint8_t memAddr, int byteCount, uint8_t *data, int *cnt);
int  I2C1_write_reg16(uint8_t slaveAddr, uint8_t reg, uint16_t val);

uint16_t INA3221_readRegister(uint8_t reg);
uint8_t  INA3221_check(void);
void     INA3221_init(void);
void     INA3221_read_channel(uint8_t channel, INA_Data_t *data);


void UART0_init(void)
{
    SIM->SCGC4 |= 0x0400;
    SIM->SOPT2  = (SIM->SOPT2 & ~0x0C000000u) | 0x04000000u;

    UART0->C2 = 0x00;

    UART0->C4  = 0x0D;
    UART0->BDH = 0x00;
    UART0->BDL = 0x0D;

    UART0->C1 = 0x00;
    UART0->C2 = 0x0C;

    SIM->SCGC5   |= 0x0200;
    PORTA->PCR[2] = 0x0200;
    PORTA->PCR[1] = 0x0200;

    UART0->S1 = 0x1F;
    (void)UART0->D;
}

void UART0_putc(char c)
{
    while (!(UART0->S1 & 0x80)) { }
    UART0->D = (uint8_t)c;
}

void UART0_puts(const char *str)
{
    while (*str != '\0') {
        if (*str == '\n') {
            UART0_putc('\r');
        }
        UART0_putc(*str);
        str++;
    }
}

void UART0_putn(unsigned int n)
{
    char buf[11];
    int i = 0;

    if (n == 0) {
        UART0_putc('0');
        return;
    }
    while (n > 0) {
        buf[i++] = (char)('0' + (n % 10));
        n /= 10;
    }
    while (i > 0) {
        UART0_putc(buf[--i]);
    }
}

void UART0_putd(int32_t v)
{
    if (v < 0) {
        UART0_putc('-');
        v = -v;
    }
    UART0_putn((unsigned int)v);
}

void UART0_puthex16(uint16_t v)
{
    static const char hex[16] = "0123456789ABCDEF";
    int i;

    UART0_puts("0x");
    for (i = 12; i >= 0; i -= 4) {
        UART0_putc(hex[(v >> i) & 0x0Fu]);
    }
}

void UART0_pad(int ancho, int usados)
{
    while (usados < ancho) {
        UART0_putc(' ');
        usados++;
    }
}

static int anchoDe(int32_t v)
{
    int n = (v < 0) ? 1 : 0;

    if (v < 0) v = -v;
    do {
        n++;
        v /= 10;
    } while (v > 0);
    return n;
}

void delayMs(int n)
{
    int i;

    SysTick->LOAD = (CLK_HZ / 1000u) - 1u;
    SysTick->CTRL = 0x5;
    for (i = 0; i < n; i++) {
        while ((SysTick->CTRL & 0x10000) == 0) { }
    }
    SysTick->CTRL = 0;
}


void I2C1_init(void)
{
    SIM->SCGC4 |= SIM_SCGC4_I2C1_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;

    PORTC->PCR[10] = PORT_PCR_MUX(2);   /* SCL */
    PORTC->PCR[11] = PORT_PCR_MUX(2);   /* SDA */

    I2C1->C1 = 0;
    I2C1->S  = I2C_S_IICIF_MASK;
    I2C1->F  = 0x1A;                    /* MULT=00 (x1), ICR=0x1A -> /112 -> ~94 kHz con el bus a 10.49 MHz */
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

int I2C1_burstRead(uint8_t slaveAddr, uint8_t memAddr, int byteCount, uint8_t *data, int *cnt)
{
    int retry = 100;

    *cnt = 0;

    while (I2C1->S & I2C_S_BUSY_MASK) {
        if (--retry <= 0) return ERR_BUS_BUSY;
    }

    I2C1->C1 |= I2C_C1_TX_MASK;
    I2C1->C1 |= I2C_C1_MST_MASK;
    I2C1->D   = (uint8_t)(slaveAddr << 1);
    if (i2c1_wait_iicif() != ERR_NONE)  return ERR_TIMEOUT;
    if (I2C1->S & I2C_S_ARBL_MASK)    { i2c1_stop(); return ERR_ARB_LOST; }
    if (I2C1->S & I2C_S_RXAK_MASK)    { i2c1_stop(); return ERR_NO_ACK;   }

    I2C1->D = memAddr;
    if (i2c1_wait_iicif() != ERR_NONE)  return ERR_TIMEOUT;
    if (I2C1->S & I2C_S_RXAK_MASK)    { i2c1_stop(); return ERR_NO_ACK;   }

    I2C1->C1 |= I2C_C1_RSTA_MASK;
    I2C1->D   = (uint8_t)((slaveAddr << 1) | 1u);
    if (i2c1_wait_iicif() != ERR_NONE)  return ERR_TIMEOUT;
    if (I2C1->S & I2C_S_RXAK_MASK)    { i2c1_stop(); return ERR_NO_ACK;   }

    I2C1->C1 &= (uint8_t)~(I2C_C1_TX_MASK | I2C_C1_TXAK_MASK);
    if (byteCount == 1) I2C1->C1 |= I2C_C1_TXAK_MASK;

    (void)I2C1->D;                      /* lectura ficticia: arranca el reloj del primer byte */

    while (byteCount > 0) {
        if (byteCount == 1) I2C1->C1 |= I2C_C1_TXAK_MASK;
        if (i2c1_wait_iicif() != ERR_NONE) return ERR_TIMEOUT;
        if (byteCount == 1) I2C1->C1 &= (uint8_t)~I2C_C1_MST_MASK;  /* STOP antes de leer el ultimo byte */

        *data++ = I2C1->D;
        byteCount--;
        (*cnt)++;
    }
    return ERR_NONE;
}

int I2C1_write_reg16(uint8_t slaveAddr, uint8_t reg, uint16_t val)
{
    int retry = 100;

    while (I2C1->S & I2C_S_BUSY_MASK) {
        if (--retry <= 0) return ERR_BUS_BUSY;
    }

    I2C1->C1 |= I2C_C1_TX_MASK;
    I2C1->C1 |= I2C_C1_MST_MASK;
    I2C1->D   = (uint8_t)(slaveAddr << 1);
    if (i2c1_wait_iicif() != ERR_NONE)  return ERR_TIMEOUT;
    if (I2C1->S & I2C_S_RXAK_MASK)    { i2c1_stop(); return ERR_NO_ACK; }

    I2C1->D = reg;
    if (i2c1_wait_iicif() != ERR_NONE)  return ERR_TIMEOUT;
    if (I2C1->S & I2C_S_RXAK_MASK)    { i2c1_stop(); return ERR_NO_ACK; }

    I2C1->D = (uint8_t)(val >> 8);
    if (i2c1_wait_iicif() != ERR_NONE)  return ERR_TIMEOUT;
    if (I2C1->S & I2C_S_RXAK_MASK)    { i2c1_stop(); return ERR_NO_ACK; }

    I2C1->D = (uint8_t)(val & 0xFFu);
    if (i2c1_wait_iicif() != ERR_NONE)  return ERR_TIMEOUT;
    if (I2C1->S & I2C_S_RXAK_MASK)    { i2c1_stop(); return ERR_NO_ACK; }

    i2c1_stop();
    return ERR_NONE;
}


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

    bus_s   = (int16_t)((int16_t)busRaw   >> 3);   /* bits [2:0] no se usan */
    shunt_s = (int16_t)((int16_t)shuntRaw >> 3);

    data->bus_mV     = (int32_t)bus_s   * 8;       /* LSB = 8 mV  */
    data->shunt_uV   = (int32_t)shunt_s * 40;      /* LSB = 40 uV */
    data->current_mA = data->shunt_uV / R_SHUNT_mOHM;    /* I(mA) = V(uV) / R(mOhm) */
}


static void imprimirCabecera(void)
{
    UART0_puts("\n");
    UART0_puts("CH    Vbus(mV)   Vshunt(uV)    I(mA)\n");
    UART0_puts("-------------------------------------\n");
}

static void imprimirCanal(uint8_t ch, const INA_Data_t *d)
{
    UART0_putn(ch);

    UART0_pad(12, anchoDe(d->bus_mV));
    UART0_putd(d->bus_mV);

    UART0_pad(13, anchoDe(d->shunt_uV));
    UART0_putd(d->shunt_uV);

    UART0_pad(9, anchoDe(d->current_mA));
    UART0_putd(d->current_mA);

    UART0_puts("\n");
}

int main(void)
{
    INA_Data_t canal;
    uint8_t ch;

    UART0_init();
    I2C1_init();

    delayMs(100);

    UART0_puts("\n\n=== Practica I2C: INA3221 ===\n");

    while (!INA3221_check()) {
        UART0_puts("INA3221 no responde en 0x40. Revisa SDA/SCL y pull-ups.\n");
        delayMs(1000);
    }

    UART0_puts("INA3221 detectado. MANUF_ID = ");
    UART0_puthex16(INA3221_readRegister(INA3221_REG_MANUFACTURER_ID));
    UART0_puts("   DIE_ID = ");
    UART0_puthex16(INA3221_readRegister(INA3221_REG_DIE_ID));
    UART0_puts("\n");

    INA3221_init();

    UART0_puts("CONFIG = ");
    UART0_puthex16(INA3221_readRegister(INA3221_REG_CONFIG));
    UART0_puts("   Rshunt = ");
    UART0_putn(R_SHUNT_mOHM);
    UART0_puts(" mOhm\n");

    for (;;) {
        imprimirCabecera();

        for (ch = 1; ch <= 3; ch++) {
            INA3221_read_channel(ch, &canal);
            imprimirCanal(ch, &canal);
        }

        delayMs(1000);
    }
}
