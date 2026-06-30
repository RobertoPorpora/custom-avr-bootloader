# 📡 Bootloader Communication Protocol

> Simple, framed, CRC-checked request/response protocol between the PC and the bootloader over the USB virtual serial port.

---

## 🧱 Message structure

Every message (request and response) shares the same frame:

| Field | Size | Description |
|-------|:----:|-------------|
| 🚩 **Start byte** | 1 B | Fixed value `0x55`, marks the start of a message. |
| 🏷️ **Identifier** | 1 B | Transaction id, chosen by the PC and echoed by the bootloader in the response. |
| ⌨️ **Command / Status** | 1 B | Request: the command code. Response: the operation status. |
| 📏 **Payload length** | 1 B | Number of payload bytes (`N`). |
| 📦 **Payload** | N B | Command-specific data. |
| 🔒 **CRC** | 1 B | CRC-8 over `identifier · command/status · length · payload`. |

---

## 🔄 Protocol phases

1. **📥 Reception** — the bootloader reads a framed message and checks the CRC.
2. **⚙️ Execution** — if valid, the requested command is executed.
3. **🧾 Response** — a reply is built with status, optional data, and CRC.
4. **📤 Transmission** — the reply is sent using the same frame structure.

---

## ⌨️ Commands

| Code | Command | Description | Response payload |
|:----:|---------|-------------|------------------|
| `0x00` | **BOOTLOADER_VERSION** | Request bootloader + hardware version. | `[bl_major, bl_minor, hw_major, hw_minor]` (4 B) |
| `0x01` | **KEEP_ALIVE** | Reset the bootloader timeout so it stays active. | *(none)* |
| `0x02` | **WRITE_MEMORY_PAGE** | Write one flash page (see [Encryption](#-encryption)). | `[page_address]` (2 B, **in clear**) |
| `0x03` | **APPLICATION_ENABLE** | Enable the RWW section so the freshly written application becomes readable/executable. | *(none)* |
| `0xF0` | **START_SKETCH** | Make the bootloader hand over to the application (jump to `0x0000`). | *(none)* |

### Status codes (responses)

| Group | Codes |
|-------|-------|
| ✅ Success | `CP_PROCESSED` |
| 🧷 Flash errors | `CP_FLASH_ADDRESS_NOT_ALIGNED`, `CP_FLASH_ADDRESS_OUT_OF_BOUNDS` |
| 🛑 Protocol errors | `CP_WRONG_START`, `CP_WRONG_LENGTH`, `CP_WRONG_CRC`, `CP_UNKNOWN_COMMAND` |

> ℹ️ `WRITE_MEMORY_PAGE` rejects addresses that are not page-aligned or that fall inside the bootloader region (`>= 0x7000`), returning the corresponding flash error.

---

## 💬 Example flow

**1 · Get version**
```
PC  → BL :  0x55 | id1 | 0x00 | 0 |               | CRC
BL  → PC :  0x55 | id1 | CP_PROCESSED | 4 | [1,0,1,0] | CRC
```

**2 · Write a page**
```
PC  → BL :  0x55 | id2 | 0x02 | 130 | [addr_hi, addr_lo, <128 enc. bytes>] | CRC
BL  → PC :  0x55 | id2 | CP_PROCESSED | 2 | [addr_hi, addr_lo] | CRC
```

---

## 🔐 Encryption

Memory-write payloads are protected with **Speck 32/64 in CTR mode** (a lightweight
block cipher), replacing the previous repeating-key XOR.
👉 Full description: [`src/criptography.md`](src/criptography.md).

`WRITE_MEMORY_PAGE` payload layout (130 bytes):

```
[ addr_hi | addr_lo |            128 data bytes            ]
  \___ in clear ___/  \___ encrypted: Speck 32/64-CTR ___/
       (nonce)          (keystream nonce = page address)
```

- 🎯 The page address is the public CTR **nonce**, so it travels in clear; a per-page nonce means identical pages no longer produce identical ciphertext.
- 🔁 Encrypt and decrypt are the **same** operation (XOR with the keystream).
- 🔑 The secret key lives in [`src/Password.h`](src/Password.h) (`SPECK_KEY`, 64-bit) and is shared, **byte-identical**, with the Python toolchain.
- ⚠️ The `CRC` is an **integrity** check over the on-wire bytes, **not** cryptographic authentication. A signature/MAC would be needed for a fully secure (authenticated) update path.
