#ifndef PASSWORD_H
#define PASSWORD_H

#include <stdint.h>

/*
 * Chiave segreta del cifrario Speck 32/64 (64 bit = 4 parole da 16 bit).
 *
 * IMPORTANTE:
 * - Deve restare IDENTICA tra bootloader (questo file, lato C) e toolchain
 *   (lato Python: bootloader_handler.py rilegge i numeri da questo file).
 * - E' un segreto: cambiala con valori casuali tuoi e non pubblicarla.
 * - Ordine atteso dal key schedule Speck: { k0, l0, l1, l2 }.
 */
static const uint16_t SPECK_KEY[4] = {0x7CE7, 0x9BBC, 0x70B9, 0x3D45};

#endif // PASSWORD_H
