# 📉 Bootloader memory analysis (ATmega32U4)

> Analysis behind the size/security rework. All figures were measured by compiling
> the `Caterina` target with the local toolchain (`avr-gcc 7.3.0`).

---

## 1. 🧱 The hardware wall

- **MCU:** ATmega32U4 — 32 KB flash, 2.5 KB SRAM.
- **Boot section:** `BOOT_SECTION_SIZE_KB = 4` → starts at `0x7000`.
  - **4096 bytes is the absolute maximum** boot section on the 32U4 (BOOTSZ fuses,
    2048 words). **It cannot be enlarged** — either you fit in 4 KB, or nothing.

## 2. 📊 Starting point (baseline)

```
Program:  4076 B  (of 4096 → 99.5%)   ← 20 bytes free
Data:      308 B  (of 2560 → 12%)     ← RAM is comfortable
```

➡️ **The bottleneck is FLASH, not RAM.** To add anything you must first free flash.

## 3. 🔍 Where the flash goes (symbols actually linked)

| Bytes | Symbol | Origin | Note |
|------:|--------|--------|------|
| 592 | `CMDP_main` | command_processor.c | inlines `receive/execute/give_response` |
| 502 | `USB_Device_ProcessControlRequest` | LUFA core | USB enumeration |
| 354 | `__vector_10` (USB_GEN) | LUFA core | USB event/reset handling |
| 208 | `PF_write_memory_page` | process_functions.c | flash page write (required) |
| 178 | `Endpoint_Write_Control_Stream_LE` | LUFA core | used by GetLineEncoding |
| 134 | `crc8` | command_processor.c | bitwise CRC |
| 128 | `PASSWORD` | Password.h | **128-byte XOR key in `.data`** |
| 114 | `Endpoint_Read_Control_Stream_LE` | LUFA core | used by SetLineEncoding |

> The LUFA USB/CDC stack weighs **~1.5 KB** overall — the part that "eats the
> memory". `HIDParser` and most of `EndpointStream` are already dropped by `--gc-sections`.

## 4. 🛠️ Reduction levers (estimates, safest first)

| # | Action | Est. saving | Risk |
|---|--------|------------:|------|
| A | XOR key 128 → small / computed | ~110 B flash + ~110 B RAM | none |
| B | Build flags (`-mcall-prologues`, LTO) | 150–400 B | low/medium |
| C | Trim CDC control-requests (Get/Set LineEncoding) | ~250–300 B | medium (USB) |
| D | Refactor `CMDP_main` | 50–150 B | low |
| E | Shorter descriptor strings | 30–60 B | low (changes device name) |
| F | CRC8 — keep bitwise (a table costs 256 B) | — | — |

### 4.1 📐 Measured results — optimization probing

| Attempt | Program | Free of 4096 | Outcome |
|---------|--------:|-------------:|---------|
| Baseline | 4076 B | 20 B | starting point |
| `-mcall-prologues` | overflow +16 B | — | **worse** on small code → dropped |
| Micro-refactor C (NULL-checks, `feedback_bit`) | 4076 B | 20 B | **0 bytes** (already done by `-Os`) |
| **`-flto` (LTO)** | **3944 B** | **152 B** | **−132 B**, clean build |

**Takeaway:** there are no "free" wins. The only cheap lever that works is **LTO**,
but it is an aggressive link-time transform → **validate on hardware** (there is
inline asm for the interrupt-vector move, and ISRs).

### 4.2 🏁 Final result — robust Speck 32/64-CTR cipher

Decision taken: **no LEDs**, robust **Speck 32/64-CTR** cipher, byte recovery via
host-validatable C optimizations (no assembly, no USB changes).

| Step | Program | Note |
|------|--------:|------|
| Baseline (weak XOR) | 4076 B | 20 free |
| + LTO | 3944 B | 152 free |
| + Speck 32/64-CTR (key 128 → 8 B) | overflow +198 | cipher = 488 B |
| + byte-swap rotations | overflow +198 | no effect (already optimized) |
| + on-the-fly key schedule (no `rk[22]`) | overflow +158 | cipher 488 → 444 B |
| + shift-register instead of `l[i%3]` | overflow +144 | cipher 444 → 408 B |
| **+ timer 32 → 16 bit** | **4088 B** | **8 free, BUILD OK** ✅ |

**Result:** where only a very weak XOR barely fit, a real block cipher (Speck) now
fits inside 4 KB. RAM `.data + .bss` dropped **308 → 174 B**. Margin is tight (8 B);
for more headroom there remain low-risk options (shorter descriptor strings,
~40–60 B) or — carefully — an **assembly** rewrite of the cipher (high gain, but not
runtime-validatable without hardware/simulator).

> All figures: build with LTO enabled. **LTO and the 16-bit timer must be validated
> on hardware** (timing and USB enumeration are expected unchanged, but untested).

## 5. 💡 LED reintegration (evaluated, not done)

`leds.c`/`leds.h` exist but are excluded from the build (commented out in the
Makefile) and every `leds(...)` call is commented out. Reintegrating them would cost
~100–200 B of flash. **Decision: not reintegrated** — the available budget was spent
on the robust cipher instead.

## 6. 🔐 Encryption: before vs after

**Before (weak):** repeating-key XOR with key `0x00,0x01,…,0x7F` — key effectively
not secret, `key[0]=0x00` leaves the first byte in clear, no diffusion, no
authenticity (CRC8 is not cryptographic), identical plaintext → identical ciphertext.

**After:** **Speck 32/64-CTR** with a 64-bit secret key and a per-page nonce.
See [`../src/criptography.md`](../src/criptography.md).

> 🛡️ Security note: a truly *secure* update would also need **authenticity**
> (signature or MAC), not just confidentiality — out of immediate scope.

## 7. ✅ How to verify

- **Flash:** `make clean && make all` → the `Program: NNNN bytes` line must stay
  `< 4096`.
- **Functional (needs the physical board):** USB enumerates, the updater opens the
  port, a write-page + read-back round-trip matches, the app starts at timeout. These
  must be run by the user on hardware.
- **Crypto:** cross-checked against the official Speck test vector, C ↔ Python, and
  the AVR build in the `avr-gdb` simulator — all identical.
