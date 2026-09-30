#include <MKL25Z4.h>
#include "LCD.h"
#include "I2C.h"

#define LCD_CPU_HZ   20970000u

#define PIN_RS       0x01u
#define PIN_RW       0x02u
#define PIN_EN       0x04u
#define PIN_BL       0x08u
#define CMD_CLEAR    0x01u
#define CMD_HOME     0x02u
#define CMD_ENTRY    0x06u
#define CMD_DISPON   0x0Cu
#define CMD_FUNCSET  0x28u
#define CMD_DDRAM    0x80u

static uint8_t lcd_addr = LCD_ADDR_A;
static uint8_t lcd_bl   = PIN_BL;

/* Base de tiempos en TPM0, no en SysTick: la aplicacion necesita el SysTick
 * libre para su tick de 1 ms. Prescaler /16 -> 1 cuenta = 0.763 us. */
static void lcd_timer_init(void)
{
    SIM->SCGC6 |= SIM_SCGC6_TPM0_MASK;
    SIM->SOPT2  = (SIM->SOPT2 & ~0x03000000u) | 0x01000000u;

    TPM0->SC  = 0;
    TPM0->CNT = 0;
    TPM0->MOD = 0xFFFFu;
    TPM0->SC  = 0x08u | 0x04u;
}

static void lcd_delay_us(uint32_t us)
{
    uint32_t ticks = ((us * 131u) / 100u) + 1u;

    while (ticks > 0u) {
        uint16_t chunk = (ticks > 0x8000u) ? 0x8000u : (uint16_t)ticks;
        uint16_t t0    = (uint16_t)TPM0->CNT;

        while ((uint16_t)((uint16_t)TPM0->CNT - t0) < chunk) { }
        ticks -= chunk;
    }
}

static void lcd_write4(uint8_t nibble, uint8_t rs)
{
    uint8_t base = (uint8_t)((nibble << 4) | rs | lcd_bl);
    uint8_t seq[3];

    seq[0] = base;
    seq[1] = (uint8_t)(base | PIN_EN);
    seq[2] = base;

    I2C1_writeBytes(lcd_addr, seq, 3);
}

static void lcd_write8(uint8_t value, uint8_t rs)
{
    lcd_write4((uint8_t)(value >> 4), rs);
    lcd_write4((uint8_t)(value & 0x0Fu), rs);
}

static void lcd_cmd(uint8_t c)
{
    lcd_write8(c, 0);
    lcd_delay_us(50);
}

uint8_t LCD_check(void)
{
    uint8_t probe = lcd_bl;

    lcd_addr = LCD_ADDR_A;
    if (I2C1_writeBytes(LCD_ADDR_A, &probe, 1) == ERR_NONE) return 1u;

    lcd_addr = LCD_ADDR_B;
    if (I2C1_writeBytes(LCD_ADDR_B, &probe, 1) == ERR_NONE) return 1u;

    lcd_addr = LCD_ADDR_A;
    return 0u;
}

uint8_t LCD_address(void)
{
    return lcd_addr;
}

void LCD_init(void)
{
    lcd_timer_init();
    lcd_delay_us(50000);
    lcd_write4(0x03u, 0);  lcd_delay_us(4500);
    lcd_write4(0x03u, 0);  lcd_delay_us(4500);
    lcd_write4(0x03u, 0);  lcd_delay_us(150);
    lcd_write4(0x02u, 0);  lcd_delay_us(150);

    lcd_cmd(CMD_FUNCSET);
    lcd_cmd(CMD_DISPON);
    lcd_cmd(CMD_ENTRY);
    LCD_clear();
}

void LCD_clear(void)
{
    lcd_write8(CMD_CLEAR, 0);
    lcd_delay_us(2000);             /* clear tarda ~1.6 ms */
}

void LCD_setCursor(uint8_t col, uint8_t row)
{
    static const uint8_t base[LCD_ROWS] = { 0x00u, 0x40u };

    if (row >= LCD_ROWS) row = 0u;
    if (col >= LCD_COLS) col = 0u;

    lcd_cmd((uint8_t)(CMD_DDRAM | (base[row] + col)));
}

void LCD_putc(char c)
{
    lcd_write8((uint8_t)c, PIN_RS);
    lcd_delay_us(50);
}

void LCD_puts(const char *s)
{
    while (*s != '\0') {
        LCD_putc(*s++);
    }
}

void LCD_backlight(uint8_t on)
{
    uint8_t b;

    lcd_bl = on ? PIN_BL : 0u;
    b = lcd_bl;
    I2C1_writeBytes(lcd_addr, &b, 1);
}
