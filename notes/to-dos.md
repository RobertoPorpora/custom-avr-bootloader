# ✅ Progress & to-dos

> Legend: `[x]` done · `[~]` partly done / to be tested · `[ ]` to do

---

## 🎯 Goals

- [x] Compile the bootloader with `make`
- [x] Compile the application with `arduino-cli`
- [x] Select the COM port
- [x] Install the programmer on the Nano
- [x] Install the bootloader on the Olimex board through the Nano
- [x] Upload the application firmware through the bootloader
- [x] Verify that downloading (reading back) the firmware is not possible

## 🔧 Bootloader

- [x] USB communication
- [x] Command parser
- [x] Content decryption
- [x] Writing to application memory
- [x] Application startup

## 🧱 Application

- [ ] Bootloader access request handling

## 🐍 Toolchain

- [x] Bootloader compilation
- [x] Application compilation
- [x] Generation of encrypted application firmware (`.fw`)
- [x] Device reset
- [x] Bootloader installation
- [x] Memory-read blocking
- [x] Transfer of `.fw` to the board

## 🖥️ Client software

- [x] User GUI
- [x] Bootloader request
- [x] Transfer of `.fw` to the board
