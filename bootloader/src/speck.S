/*
 * speck.S - Blocco keystream Speck 32/64 in assembly AVR (per minimizzare la flash).
 *
 * Equivalente 1:1 della funzione C `speck_keystream_block` (vedi criptography.md):
 * cifra il blocco contatore (x = nonce, y = counter) di Speck 32/64 con il key
 * schedule calcolato "al volo" (22 round, rotazioni alpha=7 / beta=2).
 *
 * Firma C:
 *   void speck_keystream_block(uint16_t *out_x, uint16_t *out_y,
 *                              uint16_t nonce, uint16_t counter);
 *
 * ABI avr-gcc (argomenti a 16 bit):
 *   out_x  -> r25:r24     out_y -> r23:r22
 *   nonce  -> r21:r20     counter -> r19:r18
 *
 * La chiave e' letta dalla variabile globale SPECK_KEY[4] (in .data / RAM).
 *
 * Trucco rotazioni (poco codice, niente barrel shifter):
 *   ror16 di 7  ==  rotl1(byteswap(v))     ror16 di 8 = swap byte
 *   rol16 di 2  ==  due rotl1
 */

	.global speck_keystream_block
	.section .text

speck_keystream_block:
	; --- salva i registri callee-saved usati ---
	push r2
	push r3
	push r4
	push r5
	push r6
	push r7
	push r16

	; --- sposta i puntatori di uscita (altrimenti collidono con x/y) ---
	movw r30, r24          ; Z = out_x
	movw r26, r22          ; X = out_y

	; --- stato iniziale: x = nonce ; y = counter ---
	movw r24, r20          ; x = nonce   (r25:r24)
	movw r22, r18          ; y = counter (r23:r22)

	; --- carica la chiave: k=[0] l0=[1] l1=[2] l2=[3] ---
	lds r20, SPECK_KEY+0
	lds r21, SPECK_KEY+1   ; k  = r21:r20
	lds r18, SPECK_KEY+2
	lds r19, SPECK_KEY+3   ; l0 = r19:r18
	lds r2,  SPECK_KEY+4
	lds r3,  SPECK_KEY+5   ; l1 = r3:r2
	lds r4,  SPECK_KEY+6
	lds r5,  SPECK_KEY+7   ; l2 = r5:r4

	clr r16                ; i = 0
1:
	; ---- round di cifratura ----
	; x = ror7(x) + y ; x ^= k        (ror7 in-place, scratch r0)
	mov r0, r24
	mov r24, r25
	mov r25, r0            ; byteswap(x)
	lsl r24
	rol r25
	adc r24, r1            ; rotl1  -> ror7(x)
	add r24, r22
	adc r25, r23          ; + y
	eor r24, r20
	eor r25, r21          ; ^ k
	; y = rol2(y) ^ x
	lsl r22
	rol r23
	adc r22, r1
	lsl r22
	rol r23
	adc r22, r1           ; rol2(y)
	eor r22, r24
	eor r23, r25         ; ^ x

	; ---- key schedule "al volo" ----
	; li = ror7(l0) + k ; li ^= i      -> r7:r6
	mov r7, r18
	mov r6, r19           ; byteswap(l0)
	lsl r6
	rol r7
	adc r6, r1           ; rotl1 -> ror7(l0)
	add r6, r20
	adc r7, r21         ; + k
	eor r6, r16         ; ^ i (solo byte basso, i < 256)
	; k = rol2(k) ^ li
	lsl r20
	rol r21
	adc r20, r1
	lsl r20
	rol r21
	adc r20, r1         ; rol2(k)
	eor r20, r6
	eor r21, r7        ; ^ li
	; shift register: l0 = l1 ; l1 = l2 ; l2 = li
	movw r18, r2        ; l0 = l1
	movw r2, r4         ; l1 = l2
	movw r4, r6         ; l2 = li (r7:r6)

	; ---- controllo del ciclo (22 round, i = 0..21) ----
	inc r16
	cpi r16, 22
	brlo 1b

	; --- scrittura risultati: *out_x = x ; *out_y = y (little-endian) ---
	st Z, r24
	std Z+1, r25
	st X+, r22            ; X non supporta il displacement: uso il post-incremento
	st X, r23

	; --- ripristino registri ---
	pop r16
	pop r7
	pop r6
	pop r5
	pop r4
	pop r3
	pop r2
	ret
