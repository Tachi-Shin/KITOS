// include/dos/type.h

#ifndef DOS_TYPE_H
#define DOS_TYPE_H

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef unsigned long uintptr_t;
typedef unsigned long size_t;

typedef enum {
    false = 0,
    true = 1
} bool;

#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* DOS_TYPE_H */