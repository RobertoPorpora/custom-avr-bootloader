#include "bootloader_timer.h"

#include <avr/interrupt.h>

volatile BT_time_ms_t system_timer = 0;

volatile BT_time_ms_t bootloader_timer = 0;

ISR(TIMER1_COMPA_vect)
{
    TCNT1 = 0;
    system_timer++;
}

void BT_setup(void)
{
    /* Initialize TIMER1 to handle bootloader timeout and LED tasks.
     * With 16 MHz clock and 1/64 prescaler, timer 1 is clocked at 250 kHz
     * Our chosen compare match generates an interrupt every 1 ms.
     * This interrupt is disabled selectively when doing memory reading, erasing,
     * or writing since SPM has tight timing requirements.
     */
    OCR1AH = 0;
    OCR1AL = 250;
    TIMSK1 = (1 << OCIE1A);               // enable timer 1 output compare A match interrupt
    TCCR1B = ((1 << CS11) | (1 << CS10)); // 1/64 prescaler on timer 1 input
    TCNT1 = 0;
    system_timer = 0;
}

void BT_teardown(void)
{
    /* Undo TIMER1 setup and clear the count before running the sketch */
    TIMSK1 = 0;
    TCCR1B = 0;
    TCNT1 = 0;
}

void BT_timer_start(BT_time_ms_t *timer)
{
    *timer = system_timer;
}

bool BT_timer_expired(BT_time_ms_t *timer, BT_time_ms_t timeout)
{
    // Cast a 16-bit: forza la sottrazione modulare (wraparound a 65536 ms),
    // corretta per qualsiasi intervallo < 65536 ms (il timeout e' 60000 ms).
    return (BT_time_ms_t)(system_timer - *timer) >= timeout;
}