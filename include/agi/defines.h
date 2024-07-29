#pragma once

#include <stdint.h>
#include <stddef.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float f32;
typedef double f64;

typedef u8 b8;
typedef u16 b16;
typedef u32 b32;
typedef u64 b64;


#define true 1
#define false 0

#define null ((void*)0)

#define MAX_PACKET_SIZE 1024

// Error handling
typedef enum {
    AGI_SUCCESS = 0,
    AGI_ERROR_INVALID_ARGUMENT,
    AGI_ERROR_OUT_OF_MEMORY,
    AGI_ERROR_NETWORK,
    AGI_ERROR_PROTOCOL,
    AGI_ERROR_IO,
    // Add more error codes as needed
} agi_result_t;

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
#define AGI_PLATFORM_WINDOWS
#elif defined(__APPLE__) || defined(__MACH__)
#define AGI_PLATFORM_APPLE
#elif defined(__linux__)
#define AGI_PLATFORM_LINUX
#else
#error "Unsupported platform"
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))
