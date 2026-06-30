/*
 * criptography.c - Cifrario Speck 32/64 in modalita' CTR.
 *
 * Speck 32/64: blocco da 32 bit (due parole da 16 bit), chiave da 64 bit
 * (quattro parole da 16 bit), 22 round. La variante "small" usa rotazioni
 * alpha=7 e beta=2 (le varianti grandi usano 8 e 3).
 *
 * Scelto perche' lavora su parole da 16 bit: su AVR8 (registri a 8 bit) genera
 * molto meno codice rispetto alle varianti a 32/64 bit, restando un vero
 * cifrario a blocchi (molto piu' robusto del precedente XOR a chiave ripetuta).
 *
 * Il file non dipende dall'hardware AVR (solo stdint): cosi' lo stesso sorgente
 * si compila anche sul PC per i test di cross-validazione contro la versione
 * Python del toolchain.
 */

#include "criptography.h"
#include "Password.h" // SPECK_KEY[4]: chiave segreta condivisa con il toolchain

// --- Parametri Speck 32/64 --- (rotazioni alpha=7, beta=2 cablate nelle helper)
#define SPECK_ROUNDS 22

// Rotazioni circolari su 16 bit, specializzate per le costanti di Speck32 e
// scritte per generare poco codice su AVR (niente barrel shifter):
//   - rotr di 7  ==  rotl di 9  ==  rotl1(swap_byte(x))   (swap = quasi gratis)
//   - rotl di 2  ==  due shift
static inline uint16_t rotr16_7(uint16_t x)
{
    uint16_t s = (uint16_t)((x << 8) | (x >> 8)); // swap dei due byte (rotl 8)
    return (uint16_t)((s << 1) | (s >> 15));      // + rotl 1
}
static inline uint16_t rotl16_2(uint16_t x)
{
    return (uint16_t)((x << 2) | (x >> 14));
}

/*
 * Cifra il blocco contatore (x = nonce, y = counter) generando il keystream.
 *
 * Il key schedule e' calcolato "al volo" dentro lo stesso loop di cifratura:
 * a ogni round si usa la round key corrente e si deriva subito la successiva.
 * Cosi' si evita l'array rk[22] e un secondo loop -> molto meno codice su AVR.
 * 'l' e' un buffer circolare di 3 parole (m-1, con m=4 parole di chiave).
 */
static void speck_keystream_block(uint16_t *out_x, uint16_t *out_y,
                                  uint16_t nonce, uint16_t counter)
{
    uint16_t k = SPECK_KEY[0];
    // 'l' come shift-register di profondita' 3 (FIFO): evita l'indicizzazione
    // l[i % 3] e quindi il costoso modulo su AVR. l0 e' sempre la parola corrente.
    uint16_t l0 = SPECK_KEY[1];
    uint16_t l1 = SPECK_KEY[2];
    uint16_t l2 = SPECK_KEY[3];

    uint16_t x = nonce;
    uint16_t y = counter;

    for (uint8_t i = 0; i < SPECK_ROUNDS; i++)
    {
        // Round di cifratura con la round key corrente (k == rk[i]).
        x = (uint16_t)((rotr16_7(x) + y) ^ k);
        y = (uint16_t)(rotl16_2(y) ^ x);

        // Deriva la round key successiva (rk[i+1]) per il giro dopo.
        uint16_t li = (uint16_t)((rotr16_7(l0) + k) ^ i);
        k = (uint16_t)(rotl16_2(k) ^ li);
        // scorri il FIFO: l0 <- l1 <- l2 <- li
        l0 = l1;
        l1 = l2;
        l2 = li;
    }

    *out_x = x;
    *out_y = y;
}

/*
 * CTR: per ogni blocco da 4 byte cifra il "blocco contatore" (x=nonce,
 * y=contatore incrementale) e mette il risultato in XOR con i dati.
 * - nonce diverso per pagina  -> keystream diverso tra pagine
 * - contatore diverso per blocco -> keystream diverso entro la stessa pagina
 * Cosi' non c'e' mai riuso di keystream.
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
