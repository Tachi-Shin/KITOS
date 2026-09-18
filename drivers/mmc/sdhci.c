#include <arch/arm64/rpi4/mmio.h>
#include <drivers/mmc/sdhci.h>

#define SDHCI_BLOCK_SIZE_COUNT       0x04U
#define SDHCI_ARGUMENT               0x08U
#define SDHCI_TRANSFER_MODE          0x0CU
#define SDHCI_COMMAND                0x0EU
#define SDHCI_RESPONSE               0x10U
#define SDHCI_BUFFER                 0x20U
#define SDHCI_PRESENT_STATE          0x24U
#define SDHCI_HOST_CONTROL           0x28U
#define SDHCI_POWER_CONTROL          0x29U
#define SDHCI_CLOCK_CONTROL          0x2CU
#define SDHCI_TIMEOUT_CONTROL        0x2EU
#define SDHCI_SOFTWARE_RESET         0x2FU
#define SDHCI_INT_STATUS             0x30U
#define SDHCI_INT_STATUS_ENABLE      0x34U
#define SDHCI_INT_SIGNAL_ENABLE      0x38U
#define SDHCI_CAPABILITIES           0x40U
#define SDHCI_HOST_VERSION           0xFEU

#define SDHCI_STATE_CMD_INHIBIT      (1U << 0)
#define SDHCI_STATE_DAT_INHIBIT      (1U << 1)
#define SDHCI_STATE_CARD_INSERTED    (1U << 16)

#define SDHCI_CLOCK_INT_ENABLE       (1U << 0)
#define SDHCI_CLOCK_INT_STABLE       (1U << 1)
#define SDHCI_CLOCK_CARD_ENABLE      (1U << 2)

#define SDHCI_RESET_ALL              (1U << 0)
#define SDHCI_RESET_CMD              (1U << 1)
#define SDHCI_RESET_DATA             (1U << 2)

#define SDHCI_INT_COMMAND_COMPLETE   (1U << 0)
#define SDHCI_INT_TRANSFER_COMPLETE  (1U << 1)
#define SDHCI_INT_BUFFER_WRITE_READY (1U << 4)
#define SDHCI_INT_BUFFER_READ_READY  (1U << 5)
#define SDHCI_INT_ERROR_MASK         0xFFFF0000U

#define SDHCI_TRNS_BLOCK_COUNT_EN    (1U << 1)
#define SDHCI_TRNS_READ              (1U << 4)
#define SDHCI_TRNS_MULTI             (1U << 5)

#define SDHCI_CMD_RESP_NONE          0U
#define SDHCI_CMD_RESP_LONG          1U
#define SDHCI_CMD_RESP_SHORT         2U
#define SDHCI_CMD_RESP_SHORT_BUSY    3U
#define SDHCI_CMD_CRC                (1U << 3)
#define SDHCI_CMD_INDEX              (1U << 4)
#define SDHCI_CMD_DATA               (1U << 5)

#define SDHCI_TIMEOUT_LOOPS          10000000U

static int sdhci_wait_clear(struct sdhci_host *host, uint32_t mask)
{
    unsigned int timeout;

    for (timeout = 0U; timeout < SDHCI_TIMEOUT_LOOPS; timeout++) {
        if ((mmio_read32(host->base_address + SDHCI_PRESENT_STATE) & mask) == 0U) {
            return 0;
        }
    }
    return -1;
}

static int sdhci_wait_interrupt(struct sdhci_host *host, uint32_t wanted)
{
    unsigned int timeout;
    uint32_t status;

    for (timeout = 0U; timeout < SDHCI_TIMEOUT_LOOPS; timeout++) {
        status = mmio_read32(host->base_address + SDHCI_INT_STATUS);
        if ((status & SDHCI_INT_ERROR_MASK) != 0U) {
            mmio_write32(host->base_address + SDHCI_INT_STATUS, status);
            return -1;
        }
        if ((status & wanted) != 0U) {
            mmio_write32(host->base_address + SDHCI_INT_STATUS, wanted);
            return 0;
        }
    }
    return -1;
}

static int sdhci_wait_data_done(struct sdhci_host *host)
{
    unsigned int timeout;
    uint32_t status;

    for (timeout = 0U; timeout < SDHCI_TIMEOUT_LOOPS; timeout++) {
        status = mmio_read32(host->base_address + SDHCI_INT_STATUS);
        if ((status & SDHCI_INT_ERROR_MASK) != 0U) {
            mmio_write32(host->base_address + SDHCI_INT_STATUS, status);
            return -1;
        }
        if ((status & SDHCI_INT_TRANSFER_COMPLETE) != 0U ||
            (mmio_read32(host->base_address + SDHCI_PRESENT_STATE) &
             SDHCI_STATE_DAT_INHIBIT) == 0U) {
            mmio_write32(host->base_address + SDHCI_INT_STATUS,
                         SDHCI_INT_TRANSFER_COMPLETE);
            return 0;
        }
    }
    return -1;
}

static int sdhci_reset_lines(struct sdhci_host *host, uint8_t mask)
{
    unsigned int timeout;

    mmio_write8(host->base_address + SDHCI_SOFTWARE_RESET, mask);
    for (timeout = 0U; timeout < SDHCI_TIMEOUT_LOOPS; timeout++) {
        if ((mmio_read8(host->base_address + SDHCI_SOFTWARE_RESET) & mask) == 0U) {
            return 0;
        }
    }
    return -1;
}

void sdhci_setup(struct sdhci_host *host, uintptr_t base_address)
{
    if (host == NULL) {
        return;
    }
    host->base_address = base_address;
    host->input_clock_hz = 0U;
    host->current_clock_hz = 0U;
    host->version = 0U;
}

int sdhci_set_clock(struct sdhci_host *host, uint32_t frequency_hz)
{
    uint32_t divisor;
    uint32_t encoded_divisor;
    uint16_t clock;
    unsigned int timeout;

    if (host == NULL || frequency_hz == 0U || host->input_clock_hz == 0U) {
        return -1;
    }

    clock = mmio_read16(host->base_address + SDHCI_CLOCK_CONTROL);
    clock &= (uint16_t)~SDHCI_CLOCK_CARD_ENABLE;
    mmio_write16(host->base_address + SDHCI_CLOCK_CONTROL, clock);

    divisor = (host->input_clock_hz + frequency_hz - 1U) / frequency_hz;
    if (divisor > 1U) {
        if ((divisor & 1U) != 0U) {
            divisor++;
        }
        encoded_divisor = divisor / 2U;
    } else {
        encoded_divisor = 0U;
    }
    if (encoded_divisor > 0x3FFU) {
        encoded_divisor = 0x3FFU;
    }

    clock = (uint16_t)SDHCI_CLOCK_INT_ENABLE;
    clock |= (uint16_t)((encoded_divisor & 0xFFU) << 8);
    clock |= (uint16_t)((encoded_divisor & 0x300U) >> 2);
    mmio_write16(host->base_address + SDHCI_CLOCK_CONTROL, clock);

    for (timeout = 0U; timeout < SDHCI_TIMEOUT_LOOPS; timeout++) {
        clock = mmio_read16(host->base_address + SDHCI_CLOCK_CONTROL);
        if ((clock & SDHCI_CLOCK_INT_STABLE) != 0U) {
            clock |= (uint16_t)SDHCI_CLOCK_CARD_ENABLE;
            mmio_write16(host->base_address + SDHCI_CLOCK_CONTROL, clock);
            host->current_clock_hz = frequency_hz;
            return 0;
        }
    }
    return -1;
}

int sdhci_init(struct sdhci_host *host)
{
    uint32_t capabilities;
    uint32_t base_clock_mhz;
    unsigned int timeout;

    if (host == NULL || host->base_address == 0UL) {
        return -1;
    }
    if (sdhci_reset_lines(host, SDHCI_RESET_ALL) != 0) {
        return -1;
    }

    host->version = mmio_read16(host->base_address + SDHCI_HOST_VERSION);
    capabilities = mmio_read32(host->base_address + SDHCI_CAPABILITIES);
    base_clock_mhz = (capabilities >> 8) & 0xFFU;
    host->input_clock_hz = base_clock_mhz == 0U ? 100000000U
                                                : base_clock_mhz * 1000000U;

    mmio_write8(host->base_address + SDHCI_POWER_CONTROL, 0x0FU);
    mmio_write8(host->base_address + SDHCI_TIMEOUT_CONTROL, 0x0EU);
    mmio_write32(host->base_address + SDHCI_INT_STATUS, 0xFFFFFFFFU);
    mmio_write32(host->base_address + SDHCI_INT_STATUS_ENABLE, 0xFFFFFFFFU);
    mmio_write32(host->base_address + SDHCI_INT_SIGNAL_ENABLE, 0U);

    if (sdhci_set_clock(host, 400000U) != 0) {
        return -1;
    }
    for (timeout = 0U; timeout < SDHCI_TIMEOUT_LOOPS; timeout++) {
        if ((mmio_read32(host->base_address + SDHCI_PRESENT_STATE) &
             SDHCI_STATE_CARD_INSERTED) != 0U) {
            return 0;
        }
    }
    return -1;
}

static uint16_t sdhci_encode_command(const struct sdhci_command *command)
{
    uint16_t value = (uint16_t)command->index << 8;

    switch (command->response_type) {
    case SDHCI_RESPONSE_136:
        value |= SDHCI_CMD_RESP_LONG;
        break;
    case SDHCI_RESPONSE_48:
        value |= SDHCI_CMD_RESP_SHORT;
        break;
    case SDHCI_RESPONSE_48_BUSY:
        value |= SDHCI_CMD_RESP_SHORT_BUSY;
        break;
    default:
        value |= SDHCI_CMD_RESP_NONE;
        break;
    }
    if (command->crc_check) {
        value |= SDHCI_CMD_CRC;
    }
    if (command->index_check) {
        value |= SDHCI_CMD_INDEX;
    }
    if (command->data_present) {
        value |= SDHCI_CMD_DATA;
    }
    return value;
}

static void sdhci_read_response(struct sdhci_host *host,
                                struct sdhci_command *command)
{
    unsigned int words = command->response_type == SDHCI_RESPONSE_136 ? 4U : 1U;
    unsigned int index;

    for (index = 0U; index < words; index++) {
        command->response[index] =
            mmio_read32(host->base_address + SDHCI_RESPONSE + index * 4U);
    }
}

static int sdhci_issue_command(struct sdhci_host *host,
                               struct sdhci_command *command,
                               uint16_t transfer_mode)
{
    uint32_t inhibit = SDHCI_STATE_CMD_INHIBIT;

    if (command->data_present || command->response_type == SDHCI_RESPONSE_48_BUSY) {
        inhibit |= SDHCI_STATE_DAT_INHIBIT;
    }
    if (sdhci_wait_clear(host, inhibit) != 0) {
        return -1;
    }

    mmio_write32(host->base_address + SDHCI_INT_STATUS, 0xFFFFFFFFU);
    mmio_write32(host->base_address + SDHCI_ARGUMENT, command->argument);
    mmio_write16(host->base_address + SDHCI_TRANSFER_MODE, transfer_mode);
    mmio_dmb();
    mmio_write16(host->base_address + SDHCI_COMMAND,
                 sdhci_encode_command(command));

    if (sdhci_wait_interrupt(host, SDHCI_INT_COMMAND_COMPLETE) != 0) {
        (void)sdhci_reset_lines(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA);
        return -1;
    }
    sdhci_read_response(host, command);
    return 0;
}

int sdhci_send_command(struct sdhci_host *host,
                       struct sdhci_command *command)
{
    if (host == NULL || command == NULL || command->data_present) {
        return -1;
    }
    if (sdhci_issue_command(host, command, 0U) != 0) {
        return -1;
    }
    if (command->response_type == SDHCI_RESPONSE_48_BUSY &&
        sdhci_wait_interrupt(host, SDHCI_INT_TRANSFER_COMPLETE) != 0) {
        (void)sdhci_reset_lines(host, SDHCI_RESET_DATA);
        return -1;
    }
    return 0;
}

int sdhci_transfer(struct sdhci_host *host, struct sdhci_command *command,
                   void *buffer, uint16_t block_size, uint16_t block_count,
                   bool write)
{
    uint16_t mode = SDHCI_TRNS_BLOCK_COUNT_EN;
    uint32_t ready = write ? SDHCI_INT_BUFFER_WRITE_READY
                           : SDHCI_INT_BUFFER_READ_READY;
    uint8_t *bytes = (uint8_t *)buffer;
    uint32_t word_count;
    uint32_t block;
    uint32_t word;

    if (host == NULL || command == NULL || buffer == NULL ||
        block_size == 0U || block_count == 0U ||
        (block_size & 3U) != 0U || !command->data_present) {
        return -1;
    }
    if (!write) {
        mode |= SDHCI_TRNS_READ;
    }
    if (block_count > 1U) {
        mode |= SDHCI_TRNS_MULTI;
    }

    mmio_write32(host->base_address + SDHCI_BLOCK_SIZE_COUNT,
                 (uint32_t)block_size | ((uint32_t)block_count << 16));
    if (sdhci_issue_command(host, command, mode) != 0) {
        return -1;
    }

    word_count = (uint32_t)block_size / 4U;
    for (block = 0U; block < block_count; block++) {
        if (sdhci_wait_interrupt(host, ready) != 0) {
            (void)sdhci_reset_lines(host, SDHCI_RESET_DATA);
            return -1;
        }
        for (word = 0U; word < word_count; word++) {
            uint32_t position =
                (block * word_count + word) * (uint32_t)sizeof(uint32_t);
            if (write) {
                uint32_t value = (uint32_t)bytes[position] |
                    ((uint32_t)bytes[position + 1U] << 8) |
                    ((uint32_t)bytes[position + 2U] << 16) |
                    ((uint32_t)bytes[position + 3U] << 24);
                mmio_write32(host->base_address + SDHCI_BUFFER,
                             value);
            } else {
                uint32_t value =
                    mmio_read32(host->base_address + SDHCI_BUFFER);
                bytes[position] = (uint8_t)value;
                bytes[position + 1U] = (uint8_t)(value >> 8);
                bytes[position + 2U] = (uint8_t)(value >> 16);
                bytes[position + 3U] = (uint8_t)(value >> 24);
            }
        }
    }
    if (sdhci_wait_data_done(host) != 0) {
        (void)sdhci_reset_lines(host, SDHCI_RESET_DATA);
        return -1;
    }
    mmio_dmb();
    return 0;
}
