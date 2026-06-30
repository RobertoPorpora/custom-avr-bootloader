#ifndef BOOTLOADER_TIMER_H
#define BOOTLOADER_TIMER_H

#include <stdint.h>
#include <stdbool.h>

// 16 bit bastano: il timeout massimo (60000 ms) sta in uint16 e su AVR 'int' e'
// a 16 bit, quindi l'aritmetica resta 16-bit unsigned modulare (meta' codice
// rispetto a 32 bit). Il confronto in BT_timer_expired usa un cast esplicito
// per garantire il wraparound corretto a 65536 ms.
typedef uint16_t BT_time_ms_t;

extern volatile BT_time_ms_t bootloader_timer;

void BT_setup(void);
void BT_teardown(void);

void BT_timer_start(BT_time_ms_t *timer);
bool BT_timer_expired(BT_time_ms_t *timer, BT_time_ms_t timeout);

#endif // BOOTLOADER_TIMER_H