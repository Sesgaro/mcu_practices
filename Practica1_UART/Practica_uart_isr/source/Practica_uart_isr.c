#include <MKL25Z4.h>
#include <stdint.h>

#define ADC_CANAL   8u    /* PTB0 = ADC0_SE8 */

#define SW1  0x04u    /* PTE2 */
#define SW2  0x08u    /* PTE3 */

const unsigned int CLK_HZ = 20970000;

#define C2_TIE   0x80u
#define C2_RIE   0x20u
#define C2_TE    0x08u
#define C2_RE    0x04u

#define S1_TDRE  0x80u
#define S1_RDRF  0x20u
#define S1_ERR   0x0Fu

#define RX_SIZE  64u
#define RX_MASK  (RX_SIZE - 1u)
#define TX_SIZE  512u
#define TX_MASK  (TX_SIZE - 1u)

static volatile uint8_t  rx_buf[RX_SIZE];
static volatile uint16_t rx_head = 0, rx_tail = 0;

static volatile uint8_t  tx_buf[TX_SIZE];
static volatile uint16_t tx_head = 0, tx_tail = 0;

static volatile uint32_t g_ms = 0;

static volatile uint8_t  kp_row   = 0;
static volatile uint8_t  kp_scan  = 0;
static volatile uint8_t  kp_prev  = 0;
static volatile uint8_t  kp_rep   = 0;
static volatile uint8_t  kp_event = 0;

#define BTN_DEBOUNCE  20u
static volatile unsigned int btn_state = 0, btn_raw = 0;
static volatile uint8_t      btn_count = 0;
static volatile uint8_t      btn_event = 0;

static volatile uint8_t  adc_enable = 0;
static volatile uint16_t adc_div    = 0;
static volatile uint16_t adc_value  = 0;
static volatile uint8_t  adc_ready  = 0;

static const char keymap[16] = {
    '1', '2', '3', 'A',
    '4', '5', '6', 'B',
    '7', '8', '9', 'C',
    '*', '0', '#', 'D'
};

static const uint8_t row_select[4] = { 0x01u, 0x02u, 0x04u, 0x08u };

typedef enum {
    ST_MENU = 0,
    ST_LED,
    ST_ADC,
    ST_KEYPAD,
    ST_BUTTONS
} app_state_t;

static app_state_t estado = ST_MENU;

void UART0_init(void);
void UART0_flush(void);
void UART0_putc(char c);
int  UART0_kbhit(void);
int  UART0_getc(void);
void UART0_puts(const char *str);
void UART0_putn(unsigned int n);

void tick_init(void);
void LED_init(void);
void LED_set(int value);
int  seleccion(int numero);
void keypad_init(void);
void ADC_init(void);
void buttons_init(void);
unsigned int buttons_read(void);

void showMenu(void);


void UART0_init(void)
{
    SIM->SCGC4 |= 0x0400;
    SIM->SOPT2  = (SIM->SOPT2 & ~0x0C000000u) | 0x04000000u;

    UART0->C2 = 0x00;

    UART0->C4  = 0x0D;
    UART0->BDH = 0x00;
    UART0->BDL = 0x0D;

    UART0->C1 = 0x00;

    SIM->SCGC5   |= 0x0200;
    PORTA->PCR[2] = 0x0200;
    PORTA->PCR[1] = 0x0200;

    UART0->S1 = 0x1F;
    (void)UART0->D;

    UART0->C2 = C2_TE | C2_RE | C2_RIE;

    NVIC_SetPriority(UART0_IRQn, 0);
    NVIC_ClearPendingIRQ(UART0_IRQn);
    NVIC_EnableIRQ(UART0_IRQn);
}

void UART0_IRQHandler(void)
{
    uint8_t s1 = UART0->S1;

    if (s1 & S1_ERR) {
        UART0->S1 = 0x1F;
        (void)UART0->D;
        return;
    }

    if (s1 & S1_RDRF) {
        uint8_t  d    = UART0->D;
        uint16_t next = (uint16_t)((rx_head + 1u) & RX_MASK);

        if (next != rx_tail) {
            rx_buf[rx_head] = d;
            rx_head = next;
        }
    }

    if ((UART0->C2 & C2_TIE) && (s1 & S1_TDRE)) {
        if (tx_tail != tx_head) {
            UART0->D = tx_buf[tx_tail];
            tx_tail  = (uint16_t)((tx_tail + 1u) & TX_MASK);
        } else {
            UART0->C2 &= (uint8_t)~C2_TIE;
        }
    }
}

void UART0_flush(void)
{
    rx_tail = rx_head;
}

void UART0_putc(char c)
{
    uint16_t next = (uint16_t)((tx_head + 1u) & TX_MASK);

    while (next == tx_tail) { }

    tx_buf[tx_head] = (uint8_t)c;
    tx_head = next;

    UART0->C2 |= C2_TIE;
}

int UART0_kbhit(void)
{
    return (rx_head != rx_tail);
}

int UART0_getc(void)
{
    uint8_t d;

    if (rx_head == rx_tail) {
        return -1;
    }
    d = rx_buf[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1u) & RX_MASK);
    return (int)d;
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


void tick_init(void)
{
    SIM->SCGC6 |= 0x01000000;
    SIM->SOPT2  = (SIM->SOPT2 & ~0x03000000u) | 0x01000000u;

    TPM0->SC  = 0;
    TPM0->CNT = 0;
    TPM0->MOD = (uint16_t)((CLK_HZ / 1000u) - 1u);

    TPM0->SC = 0x80;
    TPM0->SC = 0x48;

    NVIC_SetPriority(TPM0_IRQn, 2);
    NVIC_ClearPendingIRQ(TPM0_IRQn);
    NVIC_EnableIRQ(TPM0_IRQn);
}

static void keypad_tick(void)
{
    uint8_t col = (uint8_t)(PTC->PDIR & 0xF0u);

    if (col != 0xF0u) {
        if      (col == 0xE0u) kp_scan = (uint8_t)(kp_row * 4u + 1u);
        else if (col == 0xD0u) kp_scan = (uint8_t)(kp_row * 4u + 2u);
        else if (col == 0xB0u) kp_scan = (uint8_t)(kp_row * 4u + 3u);
        else if (col == 0x70u) kp_scan = (uint8_t)(kp_row * 4u + 4u);
    }

    kp_row++;
    if (kp_row >= 4u) {
        if (kp_scan == kp_prev) {
            if (kp_scan != kp_rep) {
                if (kp_scan != 0u) {
                    kp_event = kp_scan;
                }
                kp_rep = kp_scan;
            }
        }
        kp_prev = kp_scan;
        kp_scan = 0u;
        kp_row  = 0u;
    }

    PTC->PDDR = row_select[kp_row];
    PTC->PCOR = row_select[kp_row];
}

static void buttons_tick(void)
{
    unsigned int raw = buttons_read();

    if (raw != btn_raw) {
        btn_raw   = raw;
        btn_count = 0;
    } else if (btn_count < BTN_DEBOUNCE) {
        btn_count++;
        if (btn_count == BTN_DEBOUNCE && raw != btn_state) {
            btn_state = raw;
            btn_event = 1;
        }
    }
}

void TPM0_IRQHandler(void)
{
    TPM0->SC |= 0x80;

    g_ms++;

    keypad_tick();
    buttons_tick();

    if (adc_enable) {
        adc_div++;
        if (adc_div >= 500u) {
            adc_div = 0;
            ADC0->SC1[0] = 0x40u | (ADC_CANAL & 0x1Fu);
        }
    }
}


void ADC_init(void)
{
    SIM->SCGC6 |= 0x08000000;
    SIM->SCGC5 |= 0x400;

    PORTB->PCR[0] = 0x000;

    ADC0->CFG1 = 0x24;
    ADC0->CFG2 = 0x00;
    ADC0->SC2  = 0x00;
    ADC0->SC3  = 0x00;

    ADC0->SC1[0] = 0x1Fu;

    NVIC_SetPriority(ADC0_IRQn, 1);
    NVIC_ClearPendingIRQ(ADC0_IRQn);
    NVIC_EnableIRQ(ADC0_IRQn);
}

void ADC0_IRQHandler(void)
{
    adc_value = (uint16_t)ADC0->R[0];
    adc_ready = 1;
}


void LED_init(void)
{
    SIM->SCGC5 |= 0x400;
    SIM->SCGC5 |= 0x1000;

    PORTB->PCR[18] = 0x100;
    PTB->PDDR |= 0x40000;
    PTB->PSOR  = 0x40000;

    PORTB->PCR[19] = 0x100;
    PTB->PDDR |= 0x80000;
    PTB->PSOR  = 0x80000;

    PORTD->PCR[1] = 0x100;
    PTD->PDDR |= 0x02;
    PTD->PSOR  = 0x02;
}

void LED_set(int value)
{
    if (value & 1) PTB->PCOR = 0x40000; else PTB->PSOR = 0x40000;
    if (value & 2) PTB->PCOR = 0x80000; else PTB->PSOR = 0x80000;
    if (value & 4) PTD->PCOR = 0x02;    else PTD->PSOR = 0x02;
}

int seleccion(int numero)
{
    static const int colores[8] = {
        1, /* 1: Rojo     */
        2, /* 2: Verde    */
        3, /* 3: Amarillo */
        4, /* 4: Azul     */
        7, /* 5: Blanco   */
        5, /* 6: Magenta  */
        6, /* 7: Cian     */
        0  /* 8: Apagado  */
    };

    if (numero >= 1 && numero <= 8) {
        int color_elegido = colores[numero - 1];
        LED_set(color_elegido);
        return color_elegido;
    }

    return -1;   /* indice fuera de rango */
}

void keypad_init(void)
{
    int i;

    SIM->SCGC5 |= 0x0800;
    for (i = 0; i < 8; i++) {
        PORTC->PCR[i] = 0x103;
    }

    kp_row    = 0;
    PTC->PDDR = row_select[0];
    PTC->PCOR = row_select[0];
}

void buttons_init(void)
{
    SIM->SCGC5 |= 0x2000;
    PORTE->PCR[2] = 0x103;
    PORTE->PCR[3] = 0x103;
    PTE->PDDR &= ~(SW1 | SW2);

    btn_raw   = buttons_read();
    btn_state = btn_raw;
    btn_count = BTN_DEBOUNCE;
}

unsigned int buttons_read(void)
{
    uint32_t entrada = PTE->PDIR;
    unsigned int est = 0;

    if ((entrada & SW1) == 0) est |= 0x01;
    if ((entrada & SW2) == 0) est |= 0x02;
    return est;
}


void showMenu(void)
{
    UART0_puts("\n");
    UART0_puts("========================================\n");
    UART0_puts("        KL25Z UART SYSTEM (ISR)\n");
    UART0_puts("========================================\n");
    UART0_puts("  L) LED RGB\n");
    UART0_puts("  A) Lectura de ADC\n");
    UART0_puts("  K) Teclado\n");
    UART0_puts("  B) Botones\n");
    UART0_puts("----------------------------------------\n");
    UART0_puts("Opcion> ");
}

static void entrarLED(void)
{
    UART0_puts("\n--- LED RGB ---\n");
    UART0_puts("  1 Rojo    2 Verde   3 Amarillo  4 Azul\n");
    UART0_puts("  5 Blanco  6 Magenta 7 Cian      8 Apagado\n");
    UART0_puts("  Q para volver\n");
    UART0_puts("LED> ");
    estado = ST_LED;
}

static void entrarADC(void)
{
    UART0_puts("\n--- ADC ---\n");
    UART0_puts("Pulsa cualquier tecla del terminal para salir.\n");
    adc_div    = 0;
    adc_ready  = 0;
    adc_enable = 1;
    estado     = ST_ADC;
}

static void entrarKeypad(void)
{
    UART0_puts("\n--- Teclado matricial ---\n");
    UART0_puts("Q en el terminal para salir.\n");
    kp_event = 0;
    estado   = ST_KEYPAD;
}

static void entrarButtons(void)
{
    UART0_puts("\n--- Botones ---\n");
    UART0_puts("manda un caracter para salir\n");
    btn_event = 1;
    estado    = ST_BUTTONS;
}

static void volverAlMenu(void)
{
    adc_enable = 0;
    estado     = ST_MENU;
    showMenu();
}

static void imprimirADC(unsigned int valor)
{
    unsigned int mv = (valor * 3300u) / 4095u;

    UART0_puts("Cuentas: ");
    UART0_putn(valor);
    UART0_puts("   Voltaje: ");
    UART0_putn(mv / 1000u);
    UART0_putc('.');
    UART0_putn((mv / 100u) % 10u);
    UART0_putn((mv / 10u) % 10u);
    UART0_putn(mv % 10u);
    UART0_puts(" V\n");
}

static void procesarSerie(char c)
{
    if (c == '\r' || c == '\n') {
        return;
    }

    switch (estado) {

    case ST_MENU:
        UART0_putc(c);
        UART0_puts("\n");
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 32);
        }
        switch (c) {
        case 'L': entrarLED();     break;
        case 'A': entrarADC();     break;
        case 'K': entrarKeypad();  break;
        case 'B': entrarButtons(); break;
        default:
            UART0_puts("\nOpcion no valida.\n");
            showMenu();
            break;
        }
        break;

    case ST_LED:
        UART0_putc(c);
        UART0_puts("\n");
        if (c == 'q' || c == 'Q') {
            volverAlMenu();
        } else if (c >= '1' && c <= '8') {
            seleccion(c - '0');
            UART0_puts("Color aplicado\n");
            UART0_puts("LED> ");
        } else {
            UART0_puts("Opcion no valida\n");
            UART0_puts("LED> ");
        }
        break;

    case ST_KEYPAD:
        if (c == 'q' || c == 'Q') {
            volverAlMenu();
        }
        break;

    case ST_ADC:
    case ST_BUTTONS:
        LED_set(0);
        volverAlMenu();
        break;

    default:
        break;
    }
}


int main(void)
{
    __disable_irq();

    tick_init();
    UART0_init();
    LED_init();
    LED_set(0);
    keypad_init();
    ADC_init();
    buttons_init();

    __enable_irq();

    while (g_ms < 100u) { }
    UART0_flush();

    showMenu();

    for (;;) {

        int c = UART0_getc();
        if (c >= 0) {
            procesarSerie((char)c);
            continue;
        }

        if (adc_ready) {
            adc_ready = 0;
            if (estado == ST_ADC) {
                imprimirADC(adc_value);
            }
            continue;
        }

        if (kp_event) {
            uint8_t code = kp_event;
            kp_event = 0;
            if (estado == ST_KEYPAD) {
                UART0_puts("Tecla: ");
                UART0_putc(keymap[code - 1u]);
                UART0_puts("   (codigo ");
                UART0_putn((unsigned int)code);
                UART0_puts(")\n");
            }
            continue;
        }

        if (btn_event) {
            unsigned int est = btn_state;
            btn_event = 0;
            if (estado == ST_BUTTONS) {
                UART0_puts("Boton1: ");
                UART0_puts((est & 0x01) ? "pulsado" : "libre  ");
                UART0_puts("   Boton2: ");
                UART0_puts((est & 0x02) ? "pulsado" : "libre  ");
                UART0_puts("\n");
                LED_set((int)est);
            }
            continue;
        }

        __WFI();
    }
}
