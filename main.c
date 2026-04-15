#include <xc.h>

// CONFIG
#pragma config FOSC = INTRCIO
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config MCLRE = OFF
#pragma config BOREN = OFF
#pragma config CP = OFF
#pragma config CPD = OFF

// mapping of LEDs and buttons in accordance with U1 
#define LED1 PORTAbits.RA4
#define LED2 PORTCbits.RC5
#define LED3 PORTCbits.RC4
#define LED4 PORTBbits.RB7
#define LED5 PORTAbits.RA5
#define BTN_PWR   PORTCbits.RC2
#define BTN_FLTR  PORTCbits.RC3
#define BTN_PLUS  PORTAbits.RA0
#define BTN_MINUS PORTAbits.RA3

#define TRIAC PORTCbits.RC7

#define SYSTEM_STATE_LED PORTAbits.RA1  // LED-P controlling D3, D4 via transistor

#define LED_ON  0
#define LED_OFF 1
#define SLOW_BLINKING 500
#define FAST_BLINKING 100
#define SOFT_START_DELAY 8500

#define NO_PULSE_DELAY 9000   

#define _XTAL_FREQ 8000000   // 8Mhz cycles, but 4 cycle per tick/instruction

//#define MODE_TRUE_INTERRUPT  1
#define MODE_POLLING      1

uint8_t level = 3;
uint8_t prev_zc = 0;
uint16_t current_delay = SOFT_START_DELAY;
uint16_t target_delay = 6100;

typedef enum {
    STATE_OFF,
    STATE_STARTING,
    STATE_RUNNING,
    STATE_FAULT
} system_state_t;

volatile system_state_t system_state = STATE_OFF;

volatile uint32_t system_time_ms = 0;

// helpers
static inline uint8_t button_pressed(uint8_t prev, uint8_t curr)
{
    return (prev == 1 && curr == 0);
}
static inline uint8_t button_released(uint8_t prev, uint8_t curr)
{
    return (prev == 0 && curr == 1);
}
void turn_level_leds(uint8_t led_state)
{
    LED1 = led_state;
    LED2 = led_state;
    LED3 = led_state;
    LED4 = led_state;
    LED5 = led_state;
}
uint32_t get_time(void)
{
    uint32_t t;
    di();
    t = system_time_ms;
    ei();
    return t;
}
// end of helpers


void update_level_leds()
{
    LED1 = (level >= 1) ? 0 : 1;
    LED2 = (level >= 2) ? 0 : 1;
    LED3 = (level >= 3) ? 0 : 1;
    LED4 = (level >= 4) ? 0 : 1;
    LED5 = (level >= 5) ? 0 : 1;
}

void set_led(uint8_t index, uint8_t state)
{
    switch (index)
    {
        case 0: LED1 = state; break;
        case 1: LED2 = state; break;
        case 2: LED3 = state; break;
        case 3: LED4 = state; break;
        case 4: LED5 = state; break;
    }
}

void startup_animation(void)
{
    const uint8_t pattern[] = {0,1,2,3,4,3,2,1};
    
    for (uint8_t i = 0; i < sizeof(pattern); i++)
    {
        turn_level_leds(LED_OFF);
        set_led(pattern[i], LED_ON);
        __delay_ms(100);
    }
}

const uint16_t delay_table[5] = {
    7800, // (7.8 / 10) × 180 = ~140° ~15% LOW POWER sine area (level 1)
    6900, // ~124°  ~25%
    6100, // ~110°  ~40%
    5300, // ~95°   ~60%
    4500  // ~81°  ~75?80%  (level5)
};

void handle_buttons()
{
    static uint8_t prev_plus = 1;
    static uint8_t prev_minus = 1;

    uint8_t curr_plus = BTN_PLUS;
    uint8_t curr_minus = BTN_MINUS;
    
    if (button_released(prev_plus, curr_plus))
    {
        if (level < 5) level++;
    }
    
    if (button_released(prev_minus, curr_minus))
    {
        if (level > 1) level--;
    }

    prev_plus = curr_plus;
    prev_minus = curr_minus;
}

void handle_system_state()
{
    static uint8_t prev_pwr = 1;
    static uint8_t prev_fltr = 1;

    uint8_t curr_pwr = BTN_PWR;
    uint8_t curr_fltr = BTN_FLTR;


    //FLTR is the HIGHEST PRIORITY
    if (curr_fltr == 0)
    {
        system_state = STATE_FAULT;
    }
    else if (button_released(prev_fltr, curr_fltr))
    {
        if (system_state == STATE_FAULT)
        {
            current_delay = SOFT_START_DELAY;
            system_state = STATE_STARTING;
        }
    }

    // If in FAULT don't override state just return
    if (system_state == STATE_FAULT)
    {
        prev_pwr = curr_pwr;
        prev_fltr = curr_fltr;
        return;
    }

    if (curr_pwr == 0)  
    {
        if (system_state == STATE_OFF)
        {
            current_delay = SOFT_START_DELAY;

            if (BTN_PLUS == 0)
                level = 5;
            else if (BTN_MINUS == 0)
                level = 1;
            else
                level = 3;

            system_state = STATE_STARTING;
        }
    }
    else 
    {
        system_state = STATE_OFF;
    }

    prev_pwr = curr_pwr;
    prev_fltr = curr_fltr;
}



void init()
{
    // Disable analog otherwise buttons will not work
    ANSEL = 0;
    ANSELH = 0;

    // ===== TIMER0 setup for 1 ms tick =====
    // Fosc = 8 MHz ? instruction clock = Fosc/4 = 2 MHz
    // 1 tick = 0.5 µs

    OPTION_REGbits.T0CS = 0; // clock = internal (Fosc/4)
    OPTION_REGbits.PSA  = 0; // prescaler assigned to Timer0
    OPTION_REGbits.PS   = 0b101; // prescaler 1:64

    // Timer tick:
    // 0.5 µs * 64 = 32 µs per increment
    // Need ~1 ms ? 1000 / 32 ? 31 counts

    TMR0 = 256 - 31;  // preload for ~1 ms

    INTCONbits.T0IF = 0; // clear flag
    INTCONbits.T0IE = 1; // enable Timer0 interrupt
    
    /*
    RA0 = input  (BTN_PLUS)
    RA3 = input  (BTN_MINUS)
    RA4 = output (LED1)
    RA5 = output (LED5)
    */
    TRISA = 0b00001001;


#ifdef MODE_TRUE_INTERRUPT
    //TRISBbits.TRISB0 = 1;   // TRISB0 is not defined, no full register access
    TRISB |= 0b00000001;   // RB0 input
    OPTION_REGbits.INTEDG = 1; // rising edge
    INTCONbits.INTF = 0; // explicit clear, preventing undefined startup state
    INTCONbits.INTE = 1; // enable RB0 interrupt 
    // NOTE: PIC16F690 external interrupt is only on RB0
    // So real interrupt solution requires IOC (interrupt-on-change),
    // not INTE. This is placeholder for future.
    // IOCBbits.IOCB5 = 1;  // enable RB5 change interrupt
    // INTCONbits.RBIE = 1;
#else
    /*
    RB5 = input  (zero-cross)
    RB7 = output (LED4)
    */
    TRISB = 0b10100000;

#endif
    /*
    RC2 = input  (PWR)
    RC3 = input  (FLTR)
    RC4 = output (LED3)
    RC5 = output (LED2)
    RC6 = input  (if unused keep input)
    RC7 = output (TRIAC)
    */
    TRISC = 0b01101100;
    T1CON = 0b00000001; // Timer1 ON
    INTCONbits.PEIE = 1;  // peripheral
    INTCONbits.GIE = 1; // global interrupt master switch
    // clean up everything, imagine we have Easter
    PORTA = 0x00;
    PORTB = 0x00;
    PORTC = 0x00;
}


void start_timer_delay(uint16_t delay_us)
{
    uint16_t ticks = delay_us * 2; // convert to timer ticks
    uint16_t preload = (uint16_t)(0x10000UL - ticks);

    TMR1H = (preload >> 8);
    TMR1L = (preload & 0xFF);

    PIR1bits.TMR1IF = 0;  // clear flag
    PIE1bits.TMR1IE = 1;  // enable Timer1 interrupt
}


volatile uint8_t triac_state = 0;

void __interrupt() isr(void)
{
    
    // 1 ms tick TIMER0 interrupt 
    if (INTCONbits.T0IF)
    {
        INTCONbits.T0IF = 0;
        TMR0 = 256 - 31;
        system_time_ms++;
    }
    
#ifdef MODE_TRUE_INTERRUPT
    // ZERO-CROSS via RB0 interrupt
    if (INTCONbits.INTF)  // Interrupt Control Register
    {
        INTCONbits.INTF = 0;

        target_delay = delay_table[level - 1];

        if (current_delay > target_delay)
            current_delay -= 100;
        else
            current_delay = target_delay;
        
        triac_state = 0;  // start phase
        start_timer_delay(current_delay);
    }
#endif

    // TIMER1 interrupt (common for both modes)
    if (PIR1bits.TMR1IF) // Peripheral Interrupt Request Register 1 Timer1.Flag
    {
        PIR1bits.TMR1IF = 0;
        
        if (triac_state == 0)
        {
            // FIRE TRIAC (start pulse)
            TRIAC = 1;
            triac_state = 1;
            // This is PRO-LEVEL: schedule pulse end (20 µs later)
            // This is better because with __delay_us CPU is BLOCKED inside ISR  
            // No other interrupts can run + timing jitter risk
            start_timer_delay(20);
        }
        else
        {
            TRIAC = 0;
            //Peripheral Interrupt Enable Register 1 Timer1.Enable
            PIE1bits.TMR1IE = 0; 
        }   
    }
}

void handle_status_led(void)
{
    static uint32_t last_toggle = 0;
    static uint8_t led_state = LED_OFF;

    uint16_t interval = 0;

    if (system_state == STATE_OFF)
        interval = SLOW_BLINKING;
    else if (system_state == STATE_FAULT)
        interval = FAST_BLINKING;
    else
    {
        SYSTEM_STATE_LED = LED_ON;
        return;
    }
    uint32_t now = get_time();
    
    if ((now  - last_toggle) >= interval)
    {
        last_toggle = now;
        led_state = !led_state;
        SYSTEM_STATE_LED = led_state;
    }
}

void handle_level_blink(void)
{
    static uint32_t last_toggle = 0;
    static uint8_t led_on = 0;
    uint32_t now = get_time();
 
    if ((now - last_toggle) >= FAST_BLINKING)
    {
        last_toggle = now;
        led_on = !led_on;

        if (led_on)
            update_level_leds();   // show level
        else
            turn_level_leds(LED_OFF);
    }
}


void main()
{
    init();
    startup_animation();

    while (1)
    {
        handle_system_state();
#ifdef MODE_POLLING
        uint8_t curr_zc = PORTBbits.RB5;
        // Detect zero-cross (rising edge)
        // real systems use interrupts
        // Better approach:
        // Zero-cross ? hardware interrupt ? instant response
        // instead of  polling
        // Zero-cross ? detected in loop (small delay possible)
        if (prev_zc == 0 && curr_zc == 1)
        {
            if (system_state == STATE_STARTING || system_state == STATE_RUNNING)
            {
                target_delay = delay_table[level - 1];

                if (current_delay > target_delay)
                {
                    current_delay -= 100; // Decrease of ~1%
                    system_state = STATE_STARTING;
                }
                else
                {
                    current_delay = target_delay;
                    system_state = STATE_RUNNING;
                }
                
                // uint16_t debug_delay = current_delay;
                if (level == 1)  // or current_delay >= NO_PULSE_DELAY
                {
                    // NO TRIAC pulse because minimal power
                }
                else
                {
                    start_timer_delay(current_delay);
                }
            }
        }

        prev_zc = curr_zc;
#elif MODE_TRUE_INTERRUPT
        // Related behavior is handled in the Interrupt Service Routine
#endif
        handle_buttons();
        
        if (system_state == STATE_OFF )
        {
            TRIAC = 0;
            turn_level_leds(LED_OFF);
        }
        else if (system_state == STATE_FAULT)
        {
            TRIAC = 0;
            handle_level_blink();
        }
        else
        {
            update_level_leds();
        }
        
        handle_status_led();  
    }
}