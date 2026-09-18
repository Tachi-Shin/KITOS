// arch/arm64/rpi4/gicctl.c
#include <arch/arm64/armv8def.h>
#include <arch/arm64/armv8util.h>
#include <arch/arm64/rpi4/gicv2.h>
#include <dos/type.h>
#include <arch/arm64/rpi4/gicctl.h>

void init_SPI(void)
{
    uint32_t cpumask = ReadGICD32(GICD_ITARGETSR) & 0xFFU;
    uint32_t type = ReadGICD32(GICD_TYPER);
    uint32_t lines = 32U * ((type & GICD_TYPE_LINES) + 1U);
    uint32_t i;

    cpumask |= cpumask << 8;
    cpumask |= cpumask << 16;

    /* Default all global IRQs to level, active low */
    for (i = 32; i < lines; i += 16) {
        WriteGICD32(GICD_ICFGR + (i / 16) * 4, 0U);
    }

    /* Route all global IRQs to this CPU */
    for (i = 32; i < lines; i += 4) {
        WriteGICD32(GICD_ITARGETSR + (i / 4) * 4, cpumask);
    }

    /* Default priority for global interrupts */
    for (i = 32; i < lines; i += 4) {
        WriteGICD32(GICD_IPRIORITYR + (i / 4) * 4, GIC_PRI_IRQ_ALL);
    }

    /* Disable all global interrupts */
    for (i = 32; i < lines; i += 32) {
        WriteGICD32(GICD_ICENABLER + (i / 32) * 4, ~0x0U);
        WriteGICD32(GICD_ICACTIVER + (i / 32) * 4, ~0x0U);
    }
}


void SetupGIC(void)
{
    int i;

    /* Disable the distributor */
    WriteGICD32(GICD_CTLR, 0U);

    init_SPI();

    /* send all SGI and PPI interrupts to group 1 */
    WriteGICD32(GICD_IGROUPR, 0x0000FFFFU);

    /* Turn on the distributor */
    WriteGICD32(GICD_CTLR, GICD_CTL_ENABLE);


    /* initialize GIC for the current core */

    /* The first 32 interrupts (PPI and SGI) are banked per-cpu, so
     * even though they are controlled with GICD registers, they must
     * be set up here with the other per-cpu state. */
    WriteGICD32(GICD_ICACTIVER, 0xFFFFFFFFU); /* De-activate PPIs and SGIs */
    WriteGICD32(GICD_ICENABLER, 0xFFFF0000U); /* Disable all PPI */
    WriteGICD32(GICD_ISENABLER, 0x0000FFFFU); /* Enable all SGI */

    /* Set SGI priorities */
    for (i = 0; i < 16; i += 4) {
        WriteGICD32(GICD_IPRIORITYR + (i / 4) * 4, GIC_PRI_IPI_ALL);
    }

    /* Set PPI priorities */
    for (i = 16; i < 32; i += 4) {
        WriteGICD32(GICD_IPRIORITYR + (i / 4) * 4, GIC_PRI_IRQ_ALL);
    }

    /* Local settings: interface controller */
    WriteGICC32(GICC_CTLR, 0x0U);
    /* Set priority mask */
    WriteGICC32(GICC_PMR, 0xF0U);
    /* Enable group1 */
    WriteGICC32(GICC_BPR, 0x3U);
    WriteGICD32(GICD_CTLR, GICD_CTL_ENABLE);
    WriteGICC32(GICC_CTLR, GICC_CTL_ENABLE);
}

void RaiseSGI(uint32_t SGI, uint32_t core)
{
    /* just send to the current core */
    WriteGICD32(GICD_SGIR, SGI|GICD_SGI_GROUP1|(1U<<(GICD_SGI_TARGET_SHIFT+core)));
}

void ConfigInterrupt(uint32_t irq, uint32_t mode)
{
    uint32_t gicr_icfgr = GICD_ICFGR + irq/16*4;
    uint32_t icfg = ReadGICD32(gicr_icfgr);

    icfg &= ~(0x3U<<((irq%16)*2));
    icfg |= (mode<<((irq%16)*2));

    WriteGICD32(gicr_icfgr, icfg);
}

void EnableInterrupt(uint32_t irq)
{
    uint32_t slot = irq/32;
    uint32_t offset = irq%32;
    uint32_t gicd_isenabler = GICD_ISENABLER + slot*4;

    WriteGICD32(gicd_isenabler, ReadGICD32(gicd_isenabler) | (1U<<offset));
}

void DisableInterrupt(uint32_t irq)
{
    uint32_t slot  = irq / 32;
    uint32_t bit   = irq % 32;
    uint32_t reg   = GICD_ICENABLER + slot * 4;

    WriteGICD32(reg, 1U << bit);
}

void ActivateInterrupt(uint32_t irq, uint32_t ipri, int type)
{
    ConfigInterrupt(irq, type ? GICD_CFGR_EDGE_TRIGGERED : GICD_CFGR_LEVEL_SENSITIVE);
    WriteGICD8(GICD_IPRIORITYR + irq, INTPRI(ipri));
    EnableInterrupt(irq);
}

uint32_t AckInterrupt(void)
{
    return ReadGICC32(GICC_IAR);
}

void EndOfInterrupt(uint32_t intID)
{
    WriteGICC32(GICC_EOIR, intID);
}