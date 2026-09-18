#include <block/block.h>
#include <drivers/mmc/sd.h>

#define SD_CMD_GO_IDLE             0U
#define SD_CMD_SEND_IF_COND        8U
#define SD_CMD_ALL_SEND_CID        2U
#define SD_CMD_SEND_RELATIVE_ADDR  3U
#define SD_CMD_SEND_CSD            9U
#define SD_CMD_SELECT_CARD         7U
#define SD_CMD_SET_BLOCKLEN        16U
#define SD_CMD_READ_SINGLE         17U
#define SD_CMD_WRITE_SINGLE        24U
#define SD_CMD_APP                 55U
#define SD_ACMD_SEND_OP_COND       41U

#define SD_OCR_BUSY                (1U << 31)
#define SD_OCR_HIGH_CAPACITY       (1U << 30)
#define SD_OCR_VOLTAGE             0x00FF8000U
#define SD_INIT_RETRIES            100000U

static int sd_block_init(struct block_device *device);
static int sd_block_read(struct block_device *device, uint64_t lba,
                         uint32_t count, void *buffer);
static int sd_block_write(struct block_device *device, uint64_t lba,
                          uint32_t count, const void *buffer);

static const struct block_device_ops sd_block_ops = {
    .init = sd_block_init,
    .read = sd_block_read,
    .write = sd_block_write,
};

static struct sd_card qemu_card = {
    .host = { .base_address = SDHCI_QEMU_BASE },
};

static struct sd_card emmc2_card = {
    .host = { .base_address = SDHCI_EMMC2_BASE },
};

static struct block_device qemu_device = {
    .name = "QEMU raspi4b SDHCI",
    .block_size = BLOCK_SECTOR_SIZE,
    .ops = &sd_block_ops,
    .private_data = &qemu_card,
};

static struct block_device emmc2_device = {
    .name = "Raspberry Pi 4 eMMC2",
    .block_size = BLOCK_SECTOR_SIZE,
    .ops = &sd_block_ops,
    .private_data = &emmc2_card,
};

static void sd_prepare_command(struct sdhci_command *command, uint8_t index,
                               uint32_t argument,
                               enum sdhci_response_type response,
                               bool crc, bool index_check, bool data)
{
    unsigned int i;

    command->index = index;
    command->argument = argument;
    command->response_type = response;
    command->crc_check = crc;
    command->index_check = index_check;
    command->data_present = data;
    for (i = 0U; i < 4U; i++) {
        command->response[i] = 0U;
    }
}

static int sd_command(struct sd_card *card, uint8_t index, uint32_t argument,
                      enum sdhci_response_type response, bool crc,
                      bool index_check, uint32_t *response0)
{
    struct sdhci_command command;
    int result;

    sd_prepare_command(&command, index, argument, response, crc,
                       index_check, false);
    result = sdhci_send_command(&card->host, &command);
    if (result == 0 && response0 != NULL) {
        *response0 = command.response[0];
    }
    return result;
}

static int sd_app_command(struct sd_card *card, uint8_t index,
                          uint32_t argument, uint32_t *response0)
{
    if (sd_command(card, SD_CMD_APP, card->relative_address << 16,
                   SDHCI_RESPONSE_48, true, true, NULL) != 0) {
        return -1;
    }
    return sd_command(card, index, argument, SDHCI_RESPONSE_48,
                      false, false, response0);
}

/* SDHCIの136-bit応答はCRCを除いたため、CSD bit 8がresponse[0] bit 0。 */
static uint32_t sd_csd_bits(const uint32_t response[4],
                            unsigned int high, unsigned int low)
{
    uint32_t value = 0U;
    unsigned int bit;
    for (bit = low; bit <= high; bit++) {
        unsigned int shifted = bit - 8U;
        value |= ((response[shifted / 32U] >> (shifted % 32U)) & 1U)
                 << (bit - low);
    }
    return value;
}

static int sd_read_capacity(struct sd_card *card)
{
    struct sdhci_command command;
    uint32_t structure;

    sd_prepare_command(&command, SD_CMD_SEND_CSD,
                       card->relative_address << 16,
                       SDHCI_RESPONSE_136, true, false, false);
    if (sdhci_send_command(&card->host, &command) != 0) {
        return -1;
    }
    structure = sd_csd_bits(command.response, 127U, 126U);
    if (structure == 1U) {
        uint32_t size = sd_csd_bits(command.response, 69U, 48U);
        card->block_count = ((uint64_t)size + 1ULL) * 1024ULL;
    } else if (structure == 0U) {
        uint32_t read_length = sd_csd_bits(command.response, 83U, 80U);
        uint32_t size = sd_csd_bits(command.response, 73U, 62U);
        uint32_t multiplier = sd_csd_bits(command.response, 49U, 47U);
        uint64_t bytes;
        if (read_length > 31U || multiplier > 7U) {
            return -1;
        }
        bytes = ((uint64_t)size + 1ULL) *
                (1ULL << (multiplier + 2U)) * (1ULL << read_length);
        card->block_count = bytes / BLOCK_SECTOR_SIZE;
    } else {
        return -1;
    }
    return card->block_count == 0ULL ? -1 : 0;
}

int sd_card_init(struct sd_card *card)
{
    uint32_t response;
    unsigned int retries;

    if (card == NULL) {
        return -1;
    }
    card->relative_address = 0U;
    card->ocr = 0U;
    card->block_count = 0ULL;
    card->high_capacity = false;
    card->initialized = false;

    if (sdhci_init(&card->host) != 0) {
        return -1;
    }
    if (sd_command(card, SD_CMD_GO_IDLE, 0U, SDHCI_RESPONSE_NONE,
                   false, false, NULL) != 0) {
        return -1;
    }
    if (sd_command(card, SD_CMD_SEND_IF_COND, 0x1AAU, SDHCI_RESPONSE_48,
                   true, true, &response) != 0 ||
        (response & 0xFFFU) != 0x1AAU) {
        return -1;
    }

    for (retries = 0U; retries < SD_INIT_RETRIES; retries++) {
        if (sd_app_command(card, SD_ACMD_SEND_OP_COND,
                           SD_OCR_HIGH_CAPACITY | SD_OCR_VOLTAGE,
                           &response) != 0) {
            return -1;
        }
        if ((response & SD_OCR_BUSY) != 0U) {
            card->ocr = response;
            card->high_capacity = (response & SD_OCR_HIGH_CAPACITY) != 0U;
            break;
        }
    }
    if (retries == SD_INIT_RETRIES) {
        return -1;
    }
    if (sd_command(card, SD_CMD_ALL_SEND_CID, 0U, SDHCI_RESPONSE_136,
                   true, false, NULL) != 0 ||
        sd_command(card, SD_CMD_SEND_RELATIVE_ADDR, 0U, SDHCI_RESPONSE_48,
                   true, true, &response) != 0) {
        return -1;
    }
    card->relative_address = response >> 16;
    if (card->relative_address == 0U) {
        return -1;
    }
    if (sd_read_capacity(card) != 0 ||
        sd_command(card, SD_CMD_SELECT_CARD, card->relative_address << 16,
                   SDHCI_RESPONSE_48_BUSY, true, true, NULL) != 0) {
        return -1;
    }
    if (!card->high_capacity &&
        sd_command(card, SD_CMD_SET_BLOCKLEN, BLOCK_SECTOR_SIZE,
                   SDHCI_RESPONSE_48, true, true, NULL) != 0) {
        return -1;
    }
    if (sdhci_set_clock(&card->host, 25000000U) != 0) {
        return -1;
    }
    card->initialized = true;
    return 0;
}

static int sd_transfer_one(struct sd_card *card, uint64_t lba,
                           void *buffer, bool write)
{
    struct sdhci_command command;
    uint64_t address;

    if (card == NULL || !card->initialized || buffer == NULL) {
        return -1;
    }
    address = card->high_capacity ? lba : lba * BLOCK_SECTOR_SIZE;
    if (address > 0xFFFFFFFFULL) {
        return -1;
    }
    sd_prepare_command(&command,
                       write ? SD_CMD_WRITE_SINGLE : SD_CMD_READ_SINGLE,
                       (uint32_t)address, SDHCI_RESPONSE_48,
                       true, true, true);
    return sdhci_transfer(&card->host, &command, buffer,
                          BLOCK_SECTOR_SIZE, 1U, write);
}

int sd_read_blocks(struct sd_card *card, uint64_t lba,
                   uint32_t count, void *buffer)
{
    uint32_t index;
    uint8_t *bytes = (uint8_t *)buffer;

    if (count == 0U) {
        return -1;
    }
    for (index = 0U; index < count; index++) {
        if (sd_transfer_one(card, lba + index,
                            bytes + index * BLOCK_SECTOR_SIZE, false) != 0) {
            return -1;
        }
    }
    return 0;
}

int sd_write_blocks(struct sd_card *card, uint64_t lba,
                    uint32_t count, const void *buffer)
{
    uint32_t index;
    const uint8_t *bytes = (const uint8_t *)buffer;

    if (count == 0U) {
        return -1;
    }
    for (index = 0U; index < count; index++) {
        if (sd_transfer_one(card, lba + index,
                            (void *)(bytes + index * BLOCK_SECTOR_SIZE),
                            true) != 0) {
            return -1;
        }
    }
    return 0;
}

static int sd_block_init(struct block_device *device)
{
    struct sd_card *card;

    if (device == NULL || device->private_data == NULL) {
        return -1;
    }
    card = (struct sd_card *)device->private_data;
    if (sd_card_init(card) != 0) {
        return -1;
    }
    device->block_size = BLOCK_SECTOR_SIZE;
    device->block_count = card->block_count;
    return 0;
}

static int sd_block_read(struct block_device *device, uint64_t lba,
                         uint32_t count, void *buffer)
{
    return sd_read_blocks((struct sd_card *)device->private_data,
                          lba, count, buffer);
}

static int sd_block_write(struct block_device *device, uint64_t lba,
                          uint32_t count, const void *buffer)
{
    return sd_write_blocks((struct sd_card *)device->private_data,
                           lba, count, buffer);
}

struct block_device *sd_get_qemu_block_device(void)
{
    return &qemu_device;
}

struct block_device *sd_get_emmc2_block_device(void)
{
    return &emmc2_device;
}
