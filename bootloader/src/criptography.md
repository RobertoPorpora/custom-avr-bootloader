# 🔐 `criptography` module — Speck 32/64 in CTR mode

> Encrypts/decrypts the page data exchanged between the PC toolchain and the
> bootloader. Replaces the previous weak **repeating-key XOR** with a real
> lightweight block cipher, while staying within the ATmega32U4's 4 KB boot section.

---

## 🎯 Purpose

Provide confidentiality for firmware page writes with a genuine block cipher, at a
flash cost compatible with the (hard) 4 KB boot-section limit.

## 🗃️ Files involved

| File | Role |
|------|------|
| `criptography.h` / `criptography.c` | C implementation (bootloader) |
| `Password.h` | secret key `SPECK_KEY[4]` (64-bit), **shared** with the PC |
| `toolchain/bootloader_handler.py` | Python twin (`encrypt` / `decrypt`) |
| `process_functions.c` | the only firmware caller (`PF_write_memory_page`) |

---

## 🔣 The cipher

- **Speck 32/64**: 32-bit block (two 16-bit words), 64-bit key (four 16-bit words),
  **22 rounds**, rotations `alpha = 7`, `beta = 2`.
- Chosen because it works on **16-bit words** → on the 8-bit AVR it generates much
  less code than the 32/64-bit variants, while remaining a real cipher.

## 🔁 CTR mode

A "counter block" is encrypted and **XOR-ed** with the data:

- counter block = `(x = nonce, y = block_counter)`;
- 🔄 encrypt and decrypt are the **same** operation → a single `CRI_crypt()`.

### 🎯 Nonce = page address

The nonce is the **flash page address**:

- different for each page → different keystream per page (kills the XOR weakness
  "identical pages → identical ciphertext");
- the block counter varies within a page → no keystream reuse;
- the nonce is **public** by CTR's definition: that is why the address travels
  **in clear** (see below).

---

## 🧩 Main functions (`criptography.c`)

- `rotr16_7(x)` / `rotl16_2(x)` — rotations specialized for Speck's constants and
  written to emit little AVR code (`rotr 7 = rotl1(byte_swap(x))`).
- `speck_keystream_block(out_x, out_y, nonce, counter)` — produces one keystream
  block. The **key schedule is computed on the fly** inside the same encryption
  loop (no `rk[22]` array, no second loop), using a **3-word shift-register**
  instead of `l[i % 3]` (modulo is expensive on AVR).
- `CRI_crypt(data, len, nonce)` — applies the keystream to `data[0..len)` in place.
  Keystream byte order: `x_hi, x_lo, y_hi, y_lo` (must match the Python side).

---

## 🔀 Protocol: what changed

- **Before:** the whole payload `[addr_hi, addr_lo, *data(128)]` was XOR-encrypted.
- **After:** the address (first 2 bytes) travels **in clear** (it is the nonce) and
  only the 128 data bytes are encrypted. The on-wire length is unchanged (130 bytes).

On the Python side the convention is wrapped inside `encrypt` / `decrypt`:

- payload ≤ 2 bytes (e.g. the address echo in responses): **passthrough** (not encrypted);
- payload > 2 bytes: first 2 bytes in clear, CTR on the rest with `nonce = (b0 << 8) | b1`.

> ℹ️ Thanks to this convention, **`operations.py` was not touched**: the round-trip
> `decrypt(packet)` → `encrypt(payload)` still produces the same on-wire bytes.

---

## 🔑 Key (`Password.h`)

- `SPECK_KEY[4]` = `{ k0, l0, l1, l2 }` (order expected by the Speck key schedule).
- Re-read by the PC with `read_numbers_from_h_file` → both sides stay in sync.
- ⚠️ **It is a secret:** replace it with your own random values and do not publish it.

---

## ✅ Verification (done)

Automated cross-validation (host gcc + Python + AVR simulator):

1. **Official Speck32/64 test vector** — key `1918 1110 0908 0100`, pt `6574 694c`
   → ct `a868 42f2`: ✅ confirmed by Python.
2. **C ↔ Python, byte-for-byte** on the keystream/ciphertext with the real key:
   ✅ identical (`5aa2e744…541c`).
3. **AVR-compiled cipher in the `avr-gdb` simulator** ↔ host ↔ Python: ✅ identical.
4. **Round-trip** `decrypt(encrypt(x)) == x`, with address in clear and data encrypted: ✅.

---

## 📌 Constraints / notes

- 🔄 **Compatibility:** the format changed. Update **bootloader and toolchain
  together** and **re-generate `application.fw`** (old files are incompatible).
- 🛡️ **Robustness:** Speck provides confidentiality, **not authenticity**. A secure
  update would also need a signature/MAC (out of current scope).
- 🧪 The file has no AVR dependency: it also compiles on a PC for the tests.
