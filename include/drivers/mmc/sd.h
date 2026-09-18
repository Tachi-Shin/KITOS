#ifndef DOS_DRIVERS_MMC_SD_H
#define DOS_DRIVERS_MMC_SD_H

#include <dos/type.h>
#include <drivers/mmc/sdhci.h>

struct block_device;

struct sd_card {
    struct sdhci_host host;
    uint32_t relative_address;
    uint32_t ocr;
    uint64_t block_count;
    bool high_capacity;
    bool initialized;
};

int sd_card_init(struct sd_card *card);
int sd_read_blocks(struct sd_card *card, uint64_t lba,
                   uint32_t count, void *buffer);
int sd_write_blocks(struct sd_card *card, uint64_t lba,
                    uint32_t count, const void *buffer);

struct block_device *sd_get_qemu_block_device(void);
struct block_device *sd_get_emmc2_block_device(void);

#endif
