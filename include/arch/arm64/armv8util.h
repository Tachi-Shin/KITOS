// arch/arm64/armv8util.h
#ifndef __ARMV8UTIL_H__
#define  __ARMV8UTIL_H__

#include <dos/type.h>

#define TOSTRING(x)  #x

#define WriteSysReg(sysreg, val) do {                    \
     uint64_t _r = val;                                    \
     asm volatile("msr " TOSTRING(sysreg) ", %0" : : "r" (_r));       \
     } while (0)

#define ReadSysReg(sysreg) ({                          \
      uint64_t _r;                                        \
      asm volatile("mrs  %0, " TOSTRING(sysreg) : "=r" (_r));         \
     _r; })


#endif  /* __ARMV8UTIL_H__ */