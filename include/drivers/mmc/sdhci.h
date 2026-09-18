#ifndef DOS_DRIVERS_MMC_SDHCI_H
#define DOS_DRIVERS_MMC_SDHCI_H

#include <dos/type.h>

#define SDHCI_QEMU_BASE  0xFE300000UL
#define SDHCI_EMMC2_BASE 0xFE340000UL

enum sdhci_response_type {
    SDHCI_RESPONSE_NONE,
    SDHCI_RESPONSE_136,
    SDHCI_RESPONSE_48,
    SDHCI_RESPONSE_48_BUSY,
};

struct sdhci_command {
    uint8_t index;
    uint32_t argument;
    enum sdhci_response_type response_type;
    bool crc_check;
    bool index_check;
    bool data_present;
    uint32_t response[4];
};

struct sdhci_host {
    uintptr_t base_address;
    uint32_t input_clock_hz;
    uint32_t current_clock_hz;
    uint16_t version;
};

void sdhci_setup(struct sdhci_host *host, uintptr_t base_address);
int sdhci_init(struct sdhci_host *host);
int sdhci_set_clock(struct sdhci_host *host, uint32_t frequency_hz);
int sdhci_send_command(struct sdhci_host *host,
                       struct sdhci_command *command);
int sdhci_transfer(struct sdhci_host *host, struct sdhci_command *command,
                   void *buffer, uint16_t block_size, uint16_t block_count,
                   bool write);

#endif
