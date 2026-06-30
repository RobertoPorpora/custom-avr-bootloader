# 📨 Protocol — quick byte reference

> Cheat-sheet for the on-wire frames. Full description: [`../bootloader/README.md`](../bootloader/README.md).

### ➡️ Request

```
1  start          = 0x55
1  identifier
1  command
1  payload_length = N
N  payload
1  crc            over (identifier, command, payload_length, payload)
```

### ⬅️ Response

```
1  start          = 0x55
1  identifier
1  status
1  payload_length = N
N  payload
1  crc            over (identifier, status, payload_length, payload)
```

> 🔐 For `WRITE_MEMORY_PAGE` the first 2 payload bytes (page address) are sent in
> clear and used as the CTR nonce; the remaining data bytes are encrypted with
> Speck 32/64-CTR. See [`../bootloader/src/criptography.md`](../bootloader/src/criptography.md).
