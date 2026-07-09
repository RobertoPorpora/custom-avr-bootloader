/*
 * criptography.c - Cifrario Speck 32/64 in modalita' CTR.
 *
 * Il cuore del cifrario (key schedule + cifratura del blocco) e' implementato in
 * assembly AVR in `speck.S` per minimizzare l'occupazione di flash. Qui resta solo
 * la parte CTR (framing del keystream). La correttezza dell'asm e' verificata
 * eseguendolo nel simulatore avr-gdb e confrontando l'output byte-per-byte con la
 * versione Python del toolchain e con il test vector ufficiale Speck32/64.
 *
 * Speck 32/64: blocco 32 bit (due parole da 16 bit), chiave 64 bit, 22 round,
 * rotazioni alpha=7 / beta=2.
 */

#include "criptography.h"
#include "Password.h" // definisce SPECK_KEY[4], letta dall'assembly

/*
 * Cifra il blocco contatore (x = nonce, y = counter) generando un blocco di
 * keystream. Implementata in speck.S (AVR). La chiave e' presa da SPECK_KEY.
 */
extern void speck_keystream_block(uint16_t *out_x, uint16_t *out_y,
                                  uint16_t nonce, uint16_t counter);

/*
 * CTR: per ogni blocco da 4 byte cifra il blocco contatore (x=nonce,
 * y=contatore incrementale) e mette il risultato in XOR con i dati.
 * - nonce diverso per pagina    -> keystream diverso tra pagine
 * - contatore diverso per blocco -> keystream diverso entro la stessa pagina
 */
void CRI_crypt(uint8_t *data, uint16_t len, uint16_t nonce)
{
    uint16_t counter = 0;
    uint16_t i = 0;
    while (i < len)
    {
        uint16_t x;
        uint16_t y;
        speck_keystream_block(&x, &y, nonce, counter++);

        // Ordine dei byte del keystream: x_hi, x_lo, y_hi, y_lo
        // (deve combaciare ESATTAMENTE con la versione Python).
        uint8_t ks[4] = {
            (uint8_t)(x >> 8), (uint8_t)x,
            (uint8_t)(y >> 8), (uint8_t)y};

        for (uint8_t b = 0; b < 4 && i < len; b++)
        {
            data[i++] ^= ks[b];
        }
    }
}
