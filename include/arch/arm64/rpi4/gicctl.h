// include/arch/arm64/rpi4/gicctl.h
#ifndef GICCTL_H
#define GICCTL_H
#define INTPRI(ipri)    ((ipri)<<4)

#include <dos/type.h>

void init_SPI(void);
void SetupGIC(void);
void RaiseSGI(uint32_t SGI, uint32_t core);
void ConfigInterrupt(uint32_t irq, uint32_t mode);
void EnableInterrupt(uint32_t irq);
void DisableInterrupt(uint32_t irq);
void ActivateInterrupt(uint32_t irq, uint32_t ipri, int type);
uint32_t AckInterrupt(void);
void EndOfInterrupt(uint32_t intID);

#endif