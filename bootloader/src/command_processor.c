#include <stddef.h>
#include "command_processor.h"
#include "process_functions.h"

#include "bootloader_timer.h"

static tx_function_t *transmit = NULL;
static rx_function_t *receive = NULL;

#define MESSAGE_START 0x55

// -----------------------------------------------------------------------------

static CP_status_t receive_message(message_s *message);
static CP_status_t execute_command(message_s *message);
static CP_status_t give_response(message_s *message);

typedef enum
{
    CRC8_MODE_REQUEST = 0,
    CRC8_MODE_ANSWER = 1,
} crc8_mode_t;
static uint8_t crc8(message_s *message, crc8_mode_t crc8_mode);

// -----------------------------------------------------------------------------

void CMDP_tx_point_set(tx_function_t *function_pointer)
{
    transmit = function_pointer;
}

void CMDP_rx_point_set(rx_function_t *function_pointer)
{
    receive = function_pointer;
}

CP_status_t CMDP_main(void)
{
    if (NULL == receive)
    {
        return CP_MISSING_RX_FUNCTION;
    }

    if (NULL == transmit)
    {
        return CP_MISSING_TX_FUNCTION;
    }

    message_s message;

    receive_message(&message);

    if (CP_APPLICATION_DETECTED == message.status)
    {
        PF_start_sketch(&message);
        return message.status = CP_APPLICATION_DETECTED;
    }

    if (CP_RECEIVED != message.status)
    {
        return message.status;
    }

    execute_command(&message);
    give_response(&message);

    return message.status;
}

// -----------------------------------------------------------------------------

static CP_status_t receive_message(message_s *message)
{
    // message e' sempre &message (mai NULL): nessun controllo necessario.
    message->start = receive();

    if ('[' == message->start || '{' == message->start)
    {
        uint8_t _ = receive();
        _ = receive();
        _ = receive();
        uint8_t end = receive();
        if ('[' == message->start && ']' == end)
        {
            return message->status = CP_APPLICATION_DETECTED;
        }
    }

    if (MESSAGE_START != message->start)
    {
        return message->status = CP_WRONG_START;
    }
    message->identifier = receive();
    message->command = receive();
    message->payload_length = receive();
    if (message->payload_length > MAX_PAYLOAD_LENGTH)
    {
        return message->status = CP_WRONG_LENGTH;
    }
    for (uint8_t i = 0; i < message->payload_length; i++)
    {
        message->payload[i] = receive();
    }
    message->crc = receive();
    if (crc8(message, CRC8_MODE_REQUEST) != message->crc)
    {
        return message->status = CP_WRONG_CRC;
    }
    // success
    return message->status = CP_RECEIVED;
}

static CP_status_t execute_command(message_s *message)
{
    // message e' sempre &message (mai NULL): nessun controllo necessario.
    switch (message->command)
    {
    case CP_BOOTLOADER_VERSION:
        return PF_bootloader_version(message);
    case CP_KEEP_ALIVE:
        return PF_keep_alive(message);
    case CP_WRITE_MEMORY_PAGE:
        return PF_write_memory_page(message);
    case CP_APPLICATION_ENABLE:
        return PF_application_enable(message);
    case CP_START_SKETCH:
        return PF_start_sketch(message);
    default:
        return message->status = CP_UNKNOWN_COMMAND;
    }
    return message->status = CP_UNKNOWN_COMMAND;
}

static CP_status_t give_response(message_s *message)
{
    // message e' sempre &message (mai NULL): nessun controllo necessario.
    if (message->answer_payload_length > MAX_PAYLOAD_LENGTH)
    {
        return CP_ANSWER_TOO_LONG;
    }

    transmit(MESSAGE_START);
    transmit(message->identifier);
    transmit(message->status);
    transmit(message->answer_payload_length);
    for (uint8_t i = 0; i < message->answer_payload_length; i++)
    {
        transmit(message->answer_payload[i]);
    }
    transmit(crc8(message, CRC8_MODE_ANSWER));

    return message->status = CP_DELIVERED;
}

// -----------------------------------------------------------------------------

#include "Polynomial.h"

static void crc8_push(uint8_t *crc, uint8_t byte)
{
    // variabile locale (non static): evita un accesso in RAM inutile.
    uint8_t feedback_bit;

    *crc ^= byte;
    for (uint8_t bit = 8; bit > 0; --bit)
    {
        feedback_bit = *crc & 0x80;
        *crc <<= 1;
        if (0 != feedback_bit)
        {
            *crc ^= POLYNOMIAL[0];
        }
    }
}

static uint8_t crc8(message_s *message, crc8_mode_t crc8_mode)
{
    // initialize crc to zero

    uint8_t crc = 0;

    // use request data by default

    uint8_t byte_1 = message->identifier;
    uint8_t byte_2 = message->command;
    uint8_t payload_length = message->payload_length;
    uint8_t *payload = message->payload;

    // eventually switch to answer data

    if (CRC8_MODE_ANSWER == crc8_mode)
    {
        byte_2 = message->status;
        payload_length = message->answer_payload_length;
        payload = message->answer_payload;
    }

    // push bytes into crc-machine

    crc8_push(&crc, byte_1);
    crc8_push(&crc, byte_2);
    crc8_push(&crc, payload_length);
    for (uint8_t i = 0; i < payload_length; i++)
    {
        crc8_push(&crc, payload[i]);
    }

    // return resulting crc

    return crc;
}
