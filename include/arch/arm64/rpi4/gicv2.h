// include/arch/arm64/rpi4/gicv2.h
#ifndef __GICV2_H__
#define __GICV2_H__

#include <arch/arm64/rpi4/gicv2def.h>
#include <dos/type.h>

/* GICD */
static inline void WriteGICD8(uint32_t offset, uint8_t data) {
    *((volatile uint8_t *)(GICD_BASE + offset)) = data;
}

static inline uint32_t ReadGICD32(uint32_t offset) {
    return *((volatile uint32_t *)(GICD_BASE + offset));
}

static inline void WriteGICD32(uint32_t offset, uint32_t data) {
    *((volatile uint32_t *)(GICD_BASE + offset)) = data;
}

static inline uint64_t ReadGICD64(uint32_t offset) {
    return *((volatile uint64_t *)(GICD_BASE + offset));
}

static inline void WriteGICD64(uint32_t offset, uint64_t data) {
    *((volatile uint64_t *)(GICD_BASE + offset)) = data;
}

/* GICC */
static inline void WriteGICC8(uint32_t offset, uint8_t data) {
    *((volatile uint8_t *)(GICC_BASE + offset)) = data;
}

static inline uint32_t ReadGICC32(uint32_t offset) {
    return *((volatile uint32_t *)(GICC_BASE + offset));
}

static inline void WriteGICC32(uint32_t offset, uint32_t data) {
    *((volatile uint32_t *)(GICC_BASE + offset)) = data;
}

static inline uint64_t ReadGICC64(uint32_t offset) {
    return *((volatile uint64_t *)(GICC_BASE + offset));
}

static inline void WriteGICC64(uint32_t offset, uint64_t data) {
    *((volatile uint64_t *)(GICC_BASE + offset)) = data;
}

static inline uint32_t GetIRQId(uint32_t intID) {
    return intID & GICC_IA_IRQ;
}

#endif /* __GICV2_H__ */