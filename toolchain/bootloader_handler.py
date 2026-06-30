import serial
import os
import random
import time

from enum import IntEnum

# workspace
WORKSPACE_FOLDER = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))

# folders
BOOTLOADER =  os.path.join(WORKSPACE_FOLDER, 'bootloader')

import re
def read_numbers_from_h_file(file_path):
    file = open(file_path, 'r')
    file_text = file.read()
    matches = re.findall(r'0x[0-9A-Fa-f]+', file_text)
    result = list(map(lambda x: int(x[2:], 16), matches))
    file.close()
    return result

PASSWORD_H_PATH = os.path.join(BOOTLOADER, 'src', 'Password.h')
# Chiave Speck 32/64: 4 parole da 16 bit, riletta dallo stesso header del firmware
# cosi' i due lati restano sempre allineati. Ordine: [k0, l0, l1, l2].
SPECK_KEY = read_numbers_from_h_file(PASSWORD_H_PATH)

POLYNOMIAL_H_PATH = os.path.join(BOOTLOADER, 'src', 'Polynomial.h')
POLYNOMIAL = read_numbers_from_h_file(POLYNOMIAL_H_PATH)

# ------------------------------------------------------------------------------

START_BYTE = 0x55

class Command(IntEnum):
    BOOTLOADER_VERSION = 0x00
    KEEP_ALIVE = 0x01
    WRITE_MEMORY_PAGE = 0x02
    APPLICATION_ENABLE = 0x03
    START_SKETCH = 0xF0

    def __repr__(self) -> str:
        return f"<Command.{self.name}: {self.value}>"

class Status(IntEnum):
    NONE = 0
    RECEIVED = 1
    PROCESSED = 2
    DELIVERED = 3
    APPLICATION_DETECTED = 4

    MISSING_MESSAGE_STRUCT = 0x20
    MISSING_RX_FUNCTION = 0x21
    MISSING_TX_FUNCTION = 0x22
    NO_PROCESS_FUNCTION = 0x23
    ANSWER_TOO_LONG = 0x24

    WRONG_START = 0x40
    WRONG_LENGTH = 0x41
    WRONG_CRC = 0x42
    UNKNOWN_COMMAND = 0x43

    FLASH_ADDRESS_NOT_ALIGNED = 0x60
    FLASH_ADDRESS_OUT_OF_BOUNDS = 0x61

    def __repr__(self) -> str:
        return f"<Status.{self.name}: {self.value}>"

class Request:
    identifier: int
    command: Command
    payload_length: int
    payload: list[int]

    def bufferize(self) -> bytearray:
        buffer = [START_BYTE, self.identifier, self.command, self.payload_length]
        if self.command == Command.WRITE_MEMORY_PAGE:
            buffer.extend(encrypt(self.payload))
        else:
            buffer.extend(self.payload)
        buffer.append(crc8(buffer[1:]))
        return bytearray(buffer)

    def __repr__(self) -> str:
        output = 'Request:\n'
        output += f'  identifier: {self.identifier}\n'
        output += f'  command: {repr(self.command)}\n'
        output += f'  payload_length: {self.payload_length}\n'
        output += f'  payload: {self.payload}\n'
        return output

class Response:
    identifier: int
    status: Status
    payload_length: int
    payload: list[int]
    crc: int

    def __repr__(self) -> str:
        output = 'Response:\n'
        output += f'  identifier: {self.identifier}\n'
        output += f'  status: {repr(self.status)}\n'
        output += f'  payload_length: {self.payload_length}\n'
        output += f'  payload: {self.payload}\n'
        return output
    
    def crc_wrong(self) -> bool:
        buffer = [self.identifier, self.status, self.payload_length]
        buffer.extend(self.payload)
        return crc8(buffer) != self.crc


def setup(selected_com):
    global serial_port
    serial_port = serial.Serial(selected_com, baudrate=115200, timeout=1.0)
    return serial_port

def teardown():
    global serial_port
    serial_port.close()

def request_reset():
    serial_port.write(b'{RST}')

def send(request: Request) -> None:
    global serial_port
    print(f'sending {request}')
    serial_port.write(request.bufferize())

def receive_bytes(length: int) -> list[int]:
    data = []
    start = time.time()
    while time.time() - start < 5.0:
        data.extend(list(serial_port.read()))
        if len(data) >= length:
            return data
    return data

def receive(decrypt_payload: bool) -> Response:
    global serial_port

    start = receive_bytes(1)[0]
    if start != START_BYTE:
        serial_port.reset_input_buffer()
        raise Exception('start not detected')
    
    input = Response()
    input.identifier = receive_bytes(1)[0]
    input.status = Status(receive_bytes(1)[0])
    input.payload_length = receive_bytes(1)[0]
    
    input.payload = []
    if input.payload_length > 0:
        input.payload = list(receive_bytes(input.payload_length))

    input.crc = receive_bytes(1)[0]

    if input.crc_wrong():
        serial_port.reset_input_buffer()
        raise Exception('crc wrong')
    
    if decrypt_payload:
        input.payload = decrypt(input.payload)
    
    print(f'received {input}')
    return input

# ------------------------------------------------------------------------------

def crc8(buffer: list[int]) -> int:
    global POLYNOMIAL
    crc = 0
    for byte in buffer:
        crc ^= byte
        for _ in reversed(range(8)):
            feedback_bit = crc & 0x80
            crc <<= 1
            crc &= 0xFF
            if 0 != feedback_bit:
                crc ^= POLYNOMIAL[0]
    return crc

# ------------------------------------------------------------------------------
# Cifratura Speck 32/64 in modalita' CTR (gemella di bootloader/src/criptography.c).
# In CTR cifrare e decifrare sono la stessa operazione (XOR col keystream).
#
# Convenzione di payload condivisa col firmware:
#   - i primi 2 byte sono l'INDIRIZZO di pagina, viaggiano in CHIARO e fanno da
#     nonce; non vengono cifrati;
#   - dal 3o byte in poi ci sono i dati, cifrati in CTR con nonce = indirizzo.
# Payload di <= 2 byte (es. eco dell'indirizzo nelle risposte) non viene toccato.

SPECK_ROUNDS = 22
_ALPHA = 7
_BETA = 2

def _rotr16(x: int, r: int) -> int:
    return ((x >> r) | (x << (16 - r))) & 0xFFFF

def _rotl16(x: int, r: int) -> int:
    return ((x << r) | (x >> (16 - r))) & 0xFFFF

def _speck_key_schedule() -> list[int]:
    k = SPECK_KEY[0]
    l = [SPECK_KEY[1], SPECK_KEY[2], SPECK_KEY[3]]
    rk = [k]
    for i in range(SPECK_ROUNDS - 1):
        li = l[i % 3]
        li = ((_rotr16(li, _ALPHA) + k) & 0xFFFF) ^ i
        k = _rotl16(k, _BETA) ^ li
        l[i % 3] = li
        rk.append(k)
    return rk

def _speck_encrypt_block(rk: list[int], x: int, y: int) -> tuple[int, int]:
    for i in range(SPECK_ROUNDS):
        x = ((_rotr16(x, _ALPHA) + y) & 0xFFFF) ^ rk[i]
        y = _rotl16(y, _BETA) ^ x
    return x, y

def _ctr(data: list[int], nonce: int) -> list[int]:
    rk = _speck_key_schedule()
    out = []
    counter = 0
    i = 0
    while i < len(data):
        x, y = _speck_encrypt_block(rk, nonce, counter)
        counter += 1
        ks = [(x >> 8) & 0xFF, x & 0xFF, (y >> 8) & 0xFF, y & 0xFF]
        for b in range(4):
            if i >= len(data):
                break
            out.append(data[i] ^ ks[b])
            i += 1
    return out

def encrypt(payload: list[int]) -> list[int]:
    payload = list(payload)
    if len(payload) <= 2:
        return payload
    nonce = (payload[0] << 8) | payload[1]
    return payload[:2] + _ctr(payload[2:], nonce)

def decrypt(payload: list[int]) -> list[int]:
    # CTR: identica alla cifratura.
    return encrypt(payload)

# ------------------------------------------------------------------------------

class HexLine:
    size: int
    address: int
    record_type: int
    data: list[int]
    checksum: int

    def checksum_is_wrong(self) -> bool:
        checksum = self.size
        checksum += sum(self.address.to_bytes(length=2, signed=False))
        checksum += self.record_type
        checksum += sum(self.data)
        checksum += self.checksum
        checksum = checksum & 0xFF
        return checksum != 0

    def __init__(self, raw_data: str, line_number: int):

        # strip raw data and check initial ":"
        raw_data = raw_data.strip()
        if not raw_data.startswith(':'):
            raise Exception(f'line {line_number} doesn\'t start with \':\'')
        raw_data = raw_data[1:] # removes ':'
        
        # parse data bytes
        buffer = [int(raw_data[i:i+2], 16) for i in range(0, len(raw_data), 2)]
        
        if len(buffer) < 5:
            raise Exception(f'line {line_number} is too short')
        self.size = buffer[0]
        self.address = (buffer[1] << 8) + buffer[2]
        self.record_type = buffer[3]
        self.data = buffer[4:-1]
        self.checksum = buffer[-1]

        # validity checks
        if self.size != len(self.data):
            raise Exception(f'size is wrong on line {line_number}')
        if self.checksum_is_wrong():
            raise Exception(f'checksum is wrong on line {line_number}')
        

class MemoryPage:
    address: int
    size: int
    data: list[int]

    def __init__(self, address: int, size: int):
        self.address = address
        self.size = size
        self.data = [0xFF] * size


PAGE_SIZE = 64 * 2
ADDRESS_SIZE = 2

def extract_data_from_hex(hex_file_path: str) -> list[MemoryPage]:
    pages: list[MemoryPage] = []

    with open(hex_file_path, 'r') as file:
        line_count = 0
        for line in file.readlines():
            line_count += 1
            hex_line = HexLine(line, line_count)

            if hex_line.size > PAGE_SIZE:
                raise Exception(f'line {line_count} of hex file is > {PAGE_SIZE}')
            
            if hex_line.record_type != 0:
                continue
            
            if hex_line.size == 0:
                continue
            
            page_start = (hex_line.address // PAGE_SIZE) * PAGE_SIZE
            page_end = hex_line.address + hex_line.size - 1
            page_end = (page_end // PAGE_SIZE) * PAGE_SIZE

            if page_start != page_end:
                raise Exception(f'line {line_count} of hex file is spans 2 pages')

            page = None
            for element in pages:
                if element.address == page_start:
                    page = element
            
            if page is None:
                page = MemoryPage(
                    address=page_start,
                    size=PAGE_SIZE
                )
                pages.append(page)
            
            start = hex_line.address - page.address
            end = start + hex_line.size
            page.data[start:end] = hex_line.data
            
    return pages


def write_fw_file(output_path: str, pages: list[MemoryPage]):
    output_file = open(output_path, 'wb')
    random.shuffle(pages)
    for page in pages:
        data = [
            (page.address >> 8) & 0xFF,
            page.address & 0xFF,
            *page.data
        ]
        output_file.write(bytes(encrypt(data)))
    output_file.close()


def unpack_fw_file(fw_path: str) -> list[bytes]:
    with open(fw_path, 'rb') as fw_file:
        data = fw_file.read()
    
    packets = []
    packet_size = ADDRESS_SIZE + PAGE_SIZE
    i = 0
    while i < len(data):
        packets.append(data[i:i+packet_size])
        i += packet_size
    
    return packets