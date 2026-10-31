
#include <msp430.h>
#include <string.h>

// =====================================================
// MSP430FR6989 Spotify Controller
// Automatic LPM0 + Manual LPM3
//
// S1: Play/Pause
// S2: Next Track
// Hold S1 + S2 for 2 seconds: Manual LPM3
// Press S1 or S2 in LPM3: Wake
//
// Mac -> MSP430:
// A: Spotify opened
// X: Spotify closed
// T:SONG TITLE\n: Update title
//
// MSP430 -> Mac:
// P: Play/Pause
// S: Next Track
// Z: Entering manual LPM3
// W: Woke from manual LPM3
// =====================================================

#define TITLE_MAX        64
#define LCD_DIGITS        6
#define BUTTONS (BIT1 | BIT2)

#define DEBOUNCE_MS      20
#define LED_FLASH_MS    100
#define SCROLL_MS       600
#define SLEEP_HOLD_MS  2000
#define OFF_DELAY_MS   2000

// =====================================================
// LCD Character Positions
// =====================================================

const unsigned char lcd_positions[6] = {
    9, 5, 3, 18, 14, 7
};

// LCD alphabet
const unsigned char alphabet[26][2] = {
    {0xEF,0x00}, // A
    {0xF1,0x50}, // B
    {0x9C,0x00}, // C
    {0xF0,0x50}, // D
    {0x9F,0x00}, // E
    {0x8F,0x00}, // F
    {0xBD,0x00}, // G
    {0x6F,0x00}, // H
    {0x90,0x50}, // I
    {0x78,0x00}, // J
    {0x0E,0x22}, // K
    {0x1C,0x00}, // L
    {0x6C,0xA0}, // M
    {0x6C,0x82}, // N
    {0xFC,0x00}, // O
    {0xCF,0x00}, // P
    {0xFC,0x02}, // Q
    {0xCF,0x02}, // R
    {0xB7,0x00}, // S
    {0x80,0x50}, // T
    {0x7C,0x00}, // U
    {0x0C,0x28}, // V
    {0x6C,0x0A}, // W
    {0x00,0xAA}, // X
    {0x00,0xB0}, // Y
    {0x90,0x28}  // Z
};

// LCD numbers
const unsigned char numbers[10][2] = {
    {0xFC,0x28}, // 0
    {0x60,0x20}, // 1
    {0xDB,0x00}, // 2
    {0xF3,0x00}, // 3
    {0x67,0x00}, // 4
    {0xB7,0x00}, // 5
    {0xBF,0x00}, // 6
    {0xE4,0x00}, // 7
    {0xFF,0x00}, // 8
    {0xF7,0x00}  // 9
};

// =====================================================
// Global Variables
// =====================================================

// Spotify events received by UART ISR
volatile unsigned char active_requested = 0;
volatile unsigned char off_requested = 0;
volatile unsigned char title_ready = 0;

// UART title buffer
volatile char rx_buffer[TITLE_MAX];
volatile char pending_title[TITLE_MAX];

volatile unsigned int rx_index = 0;
volatile unsigned char rx_state = 0;

// Spotify status
unsigned char spotify_online = 0;

// Power management
volatile unsigned char deep_sleep = 0;
volatile unsigned char offline_button_wake = 0;

unsigned char manual_sleep_requested = 0;
unsigned char lcd_enabled = 1;

// LCD song title
char song_title[TITLE_MAX] = "OFF";
unsigned int title_length = 3;

unsigned int scroll_position = 0;
unsigned int scroll_timer = 0;
unsigned int off_timer = 0;

// Button handling
unsigned int s1_timer = 0;
unsigned int s2_timer = 0;
unsigned int combo_timer = 0;

unsigned char s1_pending = 0;
unsigned char s2_pending = 0;
unsigned char combo_active = 0;

// LED feedback
unsigned int red_timer = 0;
unsigned int green_timer = 0;

// =====================================================
// Function Prototypes
// =====================================================

void init_clocks(void);
void init_gpio(void);
void init_uart(void);
void init_lcd(void);

void uart_send(char command);

void lcd_write_char(unsigned char pos, char c);
void lcd_display_window(void);
void lcd_clear(void);
void lcd_on(void);
void lcd_off(void);

void set_display(const char *text);
void update_song_title(void);
void process_uart_events(void);

void handle_buttons(void);
void update_leds(void);
void reset_buttons(void);

void automatic_lpm0(void);
void enter_manual_lpm3(void);

// =====================================================
// Main
// =====================================================

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;

    init_clocks();
    init_gpio();
    init_uart();
    init_lcd();

    PM5CTL0 &= ~LOCKLPM5;

    __enable_interrupt();

    set_display("OFF");

    while (1)
    {
        process_uart_events();

        if (manual_sleep_requested)
        {
            enter_manual_lpm3();
            continue;
        }

        // ---------------------------------------------
        // Spotify closed: OFF -> LCD off -> LPM0
        // ---------------------------------------------

        if (!spotify_online)
        {
            if (off_timer < OFF_DELAY_MS)
            {
                off_timer++;

                // Allow manual dual-button sleep
                handle_buttons();
                update_leds();

                __delay_cycles(1000);
            }
            else
            {
                automatic_lpm0();
            }

            continue;
        }

        // ---------------------------------------------
        // Spotify active
        // ---------------------------------------------

        lcd_on();

        handle_buttons();
        update_leds();

        scroll_timer++;

        if (scroll_timer >= SCROLL_MS)
        {
            scroll_timer = 0;

            if (title_length > LCD_DIGITS)
            {
                scroll_position++;

                if (scroll_position >=
                    title_length + LCD_DIGITS)
                {
                    scroll_position = 0;
                }

                lcd_display_window();
            }
        }

        __delay_cycles(1000);
    }
}

// =====================================================
// Clock Configuration
// =====================================================

void init_clocks(void)
{
    FRCTL0 = FRCTLPW | NWAITS_0;

    CSCTL0_H = CSKEY_H;

    CSCTL1 = DCOFSEL_0;

    CSCTL2 = SELA__VLOCLK |
             SELS__DCOCLK |
             SELM__DCOCLK;

    CSCTL3 = DIVA__1 |
             DIVS__1 |
             DIVM__1;

    CSCTL0_H = 0;
}

// =====================================================
// GPIO
// =====================================================

void init_gpio(void)
{
    // LEDs
    P1DIR |= BIT0;
    P1OUT &= ~BIT0;

    P9DIR |= BIT7;
    P9OUT &= ~BIT7;

    // S1 and S2 with pull-ups
    P1DIR &= ~BUTTONS;
    P1REN |= BUTTONS;
    P1OUT |= BUTTONS;

    // Falling-edge GPIO interrupts
    P1IES |= BUTTONS;
    P1IFG &= ~BUTTONS;
    P1IE &= ~BUTTONS;
}

// =====================================================
// UART
// =====================================================

void init_uart(void)
{
    // Backchannel UART:
    // P3.4 = TXD
    // P3.5 = RXD
    P3SEL1 &= ~(BIT4 | BIT5);
    P3SEL0 |= BIT4 | BIT5;

    UCA1CTLW0 = UCSWRST;
    UCA1CTLW0 |= UCSSEL__SMCLK;

    UCA1BRW = 6;

    UCA1MCTLW = UCBRF_8 |
                 UCBRS5 |
                 UCOS16;

    UCA1CTLW0 &= ~UCSWRST;

    UCA1IFG &= ~UCRXIFG;
    UCA1IE |= UCRXIE;
}

void uart_send(char command)
{
    while (!(UCA1IFG & UCTXIFG));

    UCA1TXBUF = command;
}

// =====================================================
// LCD Initialization
// =====================================================

void init_lcd(void)
{
    LCDCPCTL0 = 0xFFFF;
    LCDCPCTL1 = 0xFC3F;
    LCDCPCTL2 = 0x0FFF;

    LCDCCTL0 = LCDDIV__1 |
               LCDPRE__16 |
               LCD4MUX |
               LCDLP;

    LCDCVCTL = VLCD_1 |
               VLCDREF_0 |
               LCDCPEN;

    LCDCCPCTL = LCDCPCLKSYNC;

    LCDCMEMCTL = LCDCLRM;

    LCDCCTL0 |= LCDON;
    lcd_enabled = 1;
}

// =====================================================
// LCD Character Output
// =====================================================

void lcd_write_char(unsigned char pos, char c)
{
    unsigned char low = 0;
    unsigned char high = 0;
    unsigned char index;

    if (c >= 'a' && c <= 'z')
    {
        c -= 32;
    }

    if (c >= 'A' && c <= 'Z')
    {
        index = c - 'A';

        low = alphabet[index][0];
        high = alphabet[index][1];
    }
    else if (c >= '0' && c <= '9')
    {
        index = c - '0';

        low = numbers[index][0];
        high = numbers[index][1];
    }

    LCDMEM[pos] = low;
    LCDMEM[pos + 1] = high;
}

void lcd_clear(void)
{
    unsigned int i;

    for (i = 0; i < LCD_DIGITS; i++)
    {
        LCDMEM[lcd_positions[i]] = 0;
        LCDMEM[lcd_positions[i] + 1] = 0;
    }
}

void lcd_on(void)
{
    if (!lcd_enabled)
    {
        LCDCCTL0 |= LCDON;
        lcd_enabled = 1;
        lcd_display_window();
    }
}

void lcd_off(void)
{
    if (lcd_enabled)
    {
        lcd_clear();
        LCDCCTL0 &= ~LCDON;
        lcd_enabled = 0;
    }
}

void lcd_display_window(void)
{
    unsigned int i;
    unsigned int index;
    char c;

    for (i = 0; i < LCD_DIGITS; i++)
    {
        index = scroll_position + i;
        c = ' ';

        if (title_length <= LCD_DIGITS)
        {
            if (i < title_length)
            {
                c = song_title[i];
            }
        }
        else
        {
            index %= title_length + LCD_DIGITS;

            if (index < title_length)
            {
                c = song_title[index];
            }
        }

        lcd_write_char(lcd_positions[i], c);
    }
}

void set_display(const char *text)
{
    strncpy(song_title, text, TITLE_MAX - 1);
    song_title[TITLE_MAX - 1] = '\0';

    title_length = strlen(song_title);

    scroll_position = 0;
    scroll_timer = 0;

    if (lcd_enabled)
    {
        lcd_display_window();
    }
}

// =====================================================
// UART Event Processing
// =====================================================

void update_song_title(void)
{
    char local_title[TITLE_MAX];
    unsigned int i;

    __disable_interrupt();

    for (i = 0; i < TITLE_MAX; i++)
    {
        local_title[i] = pending_title[i];

        if (pending_title[i] == '\0')
        {
            break;
        }
    }

    local_title[TITLE_MAX - 1] = '\0';

    title_ready = 0;

    __enable_interrupt();

    set_display(local_title);
}

void process_uart_events(void)
{
    unsigned char got_off;
    unsigned char got_active;

    __disable_interrupt();

    got_off = off_requested;
    got_active = active_requested;

    off_requested = 0;
    active_requested = 0;

    __enable_interrupt();

    if (got_off)
    {
        spotify_online = 0;
        off_timer = 0;

        P1OUT &= ~BIT0;
        P9OUT &= ~BIT7;

        red_timer = 0;
        green_timer = 0;

        reset_buttons();

        // Ensure OFF appears for 2 seconds
        lcd_on();
        set_display("OFF");
    }

    if (got_active)
    {
        spotify_online = 1;
        off_timer = 0;

        P1IE &= ~BUTTONS;
        offline_button_wake = 0;

        reset_buttons();

        // Restore the LCD immediately
        lcd_on();
        set_display("READY");
    }

    if (spotify_online && title_ready)
    {
        update_song_title();
    }
    else if (!spotify_online && title_ready)
    {
        __disable_interrupt();
        title_ready = 0;
        __enable_interrupt();
    }
}

// =====================================================
// Button State
// =====================================================

void reset_buttons(void)
{
    s1_timer = 0;
    s2_timer = 0;
    combo_timer = 0;

    s1_pending = 0;
    s2_pending = 0;
    combo_active = 0;
}

// =====================================================
// Button Handling
// =====================================================

void handle_buttons(void)
{
    unsigned char s1_pressed;
    unsigned char s2_pressed;

    s1_pressed = ((P1IN & BIT1) == 0);
    s2_pressed = ((P1IN & BIT2) == 0);

    // Both buttons held for LPM3
    if (s1_pressed && s2_pressed)
    {
        combo_active = 1;

        s1_pending = 0;
        s2_pending = 0;

        if (combo_timer < SLEEP_HOLD_MS)
        {
            combo_timer++;
        }

        if (combo_timer >= SLEEP_HOLD_MS)
        {
            manual_sleep_requested = 1;
            combo_timer = 0;
        }

        return;
    }

    if (combo_active)
    {
        if (!s1_pressed && !s2_pressed)
        {
            reset_buttons();
        }

        return;
    }

    combo_timer = 0;

    // S1 = Play/Pause
    if (s1_pressed)
    {
        if (s1_timer < DEBOUNCE_MS)
        {
            s1_timer++;
        }
        else
        {
            s1_pending = 1;
        }
    }
    else
    {
        if (s1_pending && spotify_online)
        {
            uart_send('P');

            P1OUT |= BIT0;
            red_timer = LED_FLASH_MS;
        }

        s1_pending = 0;
        s1_timer = 0;
    }

    // S2 = Next Track
    if (s2_pressed)
    {
        if (s2_timer < DEBOUNCE_MS)
        {
            s2_timer++;
        }
        else
        {
            s2_pending = 1;
        }
    }
    else
    {
        if (s2_pending && spotify_online)
        {
            uart_send('S');

            P9OUT |= BIT7;
            green_timer = LED_FLASH_MS;
        }

        s2_pending = 0;
        s2_timer = 0;
    }
}

// =====================================================
// LED Feedback
// =====================================================

void update_leds(void)
{
    if (red_timer > 0)
    {
        red_timer--;

        if (red_timer == 0)
        {
            P1OUT &= ~BIT0;
        }
    }

    if (green_timer > 0)
    {
        green_timer--;

        if (green_timer == 0)
        {
            P9OUT &= ~BIT7;
        }
    }
}

// =====================================================
// Automatic LPM0
// =====================================================

void automatic_lpm0(void)
{
    // Turn off the LCD and LEDs
    lcd_off();

    P1OUT &= ~BIT0;
    P9OUT &= ~BIT7;

    // Arm GPIO interrupts to detect a manual
    // button hold while Spotify is closed.
    if (!offline_button_wake)
    {
        P1IES |= BUTTONS;
        P1IFG &= ~BUTTONS;
        P1IE |= BUTTONS;
    }

    // If a button woke us, poll its state.
    // This keeps manual LPM3 available even
    // while Spotify is closed.
    if (offline_button_wake)
    {
        handle_buttons();

        if (manual_sleep_requested)
        {
            return;
        }

        // Continue polling until release
        if ((P1IN & BUTTONS) == BUTTONS)
        {
            offline_button_wake = 0;
            reset_buttons();

            P1IE &= ~BUTTONS;
        }
        else
        {
            __delay_cycles(1000);
            return;
        }
    }

    // Atomically check wake requests before
    // entering LPM0 to avoid a lost wakeup.
    __disable_interrupt();

    if (!active_requested &&
        !off_requested &&
        !title_ready &&
        !offline_button_wake &&
        !spotify_online)
    {
        __bis_SR_register(LPM0_bits | GIE);
        __no_operation();
    }
    else
    {
        __enable_interrupt();
    }
}

// =====================================================
// Manual LPM3
// =====================================================

void enter_manual_lpm3(void)
{
    // Notify Python before disabling UART
    uart_send('Z');

    while (UCA1STATW & UCBUSY);

    UCA1IE &= ~UCRXIE;
    UCA1CTLW0 |= UCSWRST;

    rx_state = 0;
    rx_index = 0;
    title_ready = 0;

    P1OUT &= ~BIT0;
    P9OUT &= ~BIT7;

    red_timer = 0;
    green_timer = 0;

    lcd_off();

    // Release the original hold
    while ((P1IN & BUTTONS) != BUTTONS);

    __delay_cycles(20000);

    deep_sleep = 1;

    P1IES |= BUTTONS;
    P1IFG &= ~BUTTONS;
    P1IE |= BUTTONS;

    // Enter manual LPM3
    __bis_SR_register(LPM3_bits | GIE);
    __no_operation();

    // Wake after a physical button interrupt
    P1IE &= ~BUTTONS;
    P1IFG &= ~BUTTONS;

    deep_sleep = 0;

    init_uart();

    // Wait until wake button is released
    while ((P1IN & BUTTONS) != BUTTONS);

    __delay_cycles(20000);

    reset_buttons();

    offline_button_wake = 0;
    manual_sleep_requested = 0;

    if (spotify_online)
    {
        lcd_on();
        set_display("READY");
    }
    else
    {
        set_display("OFF");

        // Return to automatic LPM0
        // if Spotify is closed
        off_timer = OFF_DELAY_MS;
    }

    // Tell Python that manual LPM3 ended
    uart_send('W');
}

// =====================================================
// Port 1 Interrupt
// =====================================================

#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
    if (P1IFG & BUTTONS)
    {
        P1IE &= ~BUTTONS;
        P1IFG &= ~BUTTONS;

        if (!deep_sleep)
        {
            offline_button_wake = 1;
            __bic_SR_register_on_exit(LPM0_bits);
        }
        else
        {
            __bic_SR_register_on_exit(LPM3_bits);
        }
    }
}

// =====================================================
// UART Receive Interrupt
// =====================================================

#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
{
    unsigned char received;
    unsigned int i;

    switch (__even_in_range(
        UCA1IV,
        USCI_UART_UCTXCPTIFG))
    {
        case USCI_UART_UCRXIFG:

            received = UCA1RXBUF;

            if (rx_state == 0)
            {
                if (received == 'A')
                {
                    active_requested = 1;
                }
                else if (received == 'X')
                {
                    off_requested = 1;
                }
                else if (received == 'T')
                {
                    rx_state = 1;
                }
            }
            else if (rx_state == 1)
            {
                if (received == ':')
                {
                    rx_state = 2;
                    rx_index = 0;
                }
                else
                {
                    rx_state = 0;
                }
            }
            else
            {
                if (received == '\n')
                {
                    rx_buffer[rx_index] = '\0';

                    for (i = 0; i <= rx_index; i++)
                    {
                        pending_title[i] =
                            rx_buffer[i];
                    }

                    title_ready = 1;
                    rx_index = 0;
                    rx_state = 0;
                }
                else if (received == '\r')
                {
                    // Ignore carriage return
                }
                else if (rx_index < TITLE_MAX - 1)
                {
                    rx_buffer[rx_index++] =
                        (char)received;
                }
            }

            // UART interrupt wakes automatic LPM0
            __bic_SR_register_on_exit(LPM0_bits);

            break;

        default:
            break;
    }
}
