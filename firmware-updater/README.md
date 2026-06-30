# 🖥️ Firmware updater

> A small **PyWebView** desktop app that guides a user through updating a board running the custom AVR bootloader.

---

## 🎯 Requirements

- 👤 The user can update a board equipped with the custom AVR bootloader.
- 🔌 The user must have a USB connection (virtual serial port) to the board.
- 📦 The user must have an encrypted `.fw` file.
- 🔒 Neither the user nor the software can decrypt the contents of the `.fw` file.
- 🧭 The procedure is strict and guided, to limit variability and errors.
- 📝 Basic logging, to diagnose issues afterwards.
- 🔢 Ability to distinguish 2 or more connected boards.

---

## 🏗️ Detailed design

The software has a **frontend** and a **backend**, built with PyWebView.

**Frontend** — guides the user through the steps:
1. 📂 Load a `.fw` file.
2. 🔌 Select a serial port.
3. 📊 Run the firmware update (progress bar).
4. 🚀 Start the application, or report errors.

**Backend** — handles USB communication with the board.

### 🔁 Frontend ↔ Backend protocol

> 🟦 **F** = Frontend · 🟩 **B** = Backend

| Direction | Message |
|-----------|---------|
| F → B | Load file → *B replies with the file path* |
| F → B | Request COM port list → *B replies with the list* |
| F → B | Start update (file, port) → *B acknowledges* |
| B → F | Update status: in progress (% complete) / finished (possible errors) |

---

## 🔐 Note on encryption

The `.fw` file is encrypted with **Speck 32/64 in CTR mode** (the toolchain and the
bootloader share the same secret key). The updater only relays the already-encrypted
pages to the board, so neither the user nor this GUI needs — or is able — to decrypt
the firmware contents.
👉 See [`../bootloader/src/criptography.md`](../bootloader/src/criptography.md).
