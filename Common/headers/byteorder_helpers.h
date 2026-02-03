/*
 * byteorder_helpers.h - Consolidated host-to-disk and disk-to-host conversion helpers
 *
 * These inline functions provide type-safe byte order conversion for serialization.
 * All disk formats use big-endian byte order; these helpers convert to/from
 * host byte order as needed.
 *
 * 2026-02-02 Codex: Consolidated from duplicated definitions in menupack.c, langhash.c, memory.c
 *
 * Usage:
 *   uint32_t disk_value = host_to_disk_uint32(host_value);  // For writing to disk
 *   uint32_t host_value = disk_to_host_uint32(disk_value);  // For reading from disk
 *
 * Dependencies:
 *   - byteorder.h: Provides SWAP_BYTE_ORDER macro and doshortswap/dolongswap/dolonglongswap
 *   - db_format.h: Provides db_format_read_be64/db_format_write_be64 for 64-bit values
 */

#ifndef byteorder_helpers_include
#define byteorder_helpers_include

#include "byteorder.h"
#include "db_format.h"
#include <stdint.h>
#include <string.h>  /* for memcpy in double conversion */

/*
 * Unsigned integer conversions
 *
 * These use the swap macros from byteorder.h which are defined based on
 * the SWAP_BYTE_ORDER preprocessor symbol.
 */

static inline uint16_t host_to_disk_uint16(uint16_t value) {
#ifdef SWAP_BYTE_ORDER
    return (uint16_t)doshortswap((short)value);
#else
    return value;
#endif
}

static inline uint16_t disk_to_host_uint16(uint16_t value) {
#ifdef SWAP_BYTE_ORDER
    return (uint16_t)doshortswap((short)value);
#else
    return value;
#endif
}

static inline uint32_t host_to_disk_uint32(uint32_t value) {
#ifdef SWAP_BYTE_ORDER
    return (uint32_t)dolongswap((long)value);
#else
    return value;
#endif
}

static inline uint32_t disk_to_host_uint32(uint32_t value) {
#ifdef SWAP_BYTE_ORDER
    return (uint32_t)dolongswap((long)value);
#else
    return value;
#endif
}

static inline uint64_t host_to_disk_uint64(uint64_t value) {
#ifdef SWAP_BYTE_ORDER
    return (uint64_t)dolonglongswap((long long)value);
#else
    return value;
#endif
}

static inline uint64_t disk_to_host_uint64(uint64_t value) {
#ifdef SWAP_BYTE_ORDER
    return (uint64_t)dolonglongswap((long long)value);
#else
    return value;
#endif
}

/*
 * Signed integer conversions
 *
 * These mirror the unsigned versions but with signed types for cleaner
 * code when working with signed values.
 */

static inline int16_t host_to_disk_int16(int16_t value) {
#ifdef SWAP_BYTE_ORDER
    short temp = (short)value;
    memtodiskshort(temp);
    return (int16_t)temp;
#else
    return value;
#endif
}

static inline int16_t disk_to_host_int16(int16_t value) {
#ifdef SWAP_BYTE_ORDER
    short temp = (short)value;
    disktomemshort(temp);
    return (int16_t)temp;
#else
    return value;
#endif
}

static inline int32_t host_to_disk_int32(int32_t value) {
#ifdef SWAP_BYTE_ORDER
    long temp = (long)value;
    db_format_write_be32(&temp, (uint32_t)temp);
    return (int32_t)temp;
#else
    return value;
#endif
}

static inline int32_t disk_to_host_int32(int32_t value) {
#ifdef SWAP_BYTE_ORDER
    long temp = (long)value;
    disktomemlong(temp);
    return (int32_t)temp;
#else
    return value;
#endif
}

static inline int64_t host_to_disk_int64(int64_t value) {
#ifdef SWAP_BYTE_ORDER
    uint64_t temp = (uint64_t)value;
    db_format_write_be64(&temp, temp);
    return (int64_t)temp;
#else
    return value;
#endif
}

static inline int64_t disk_to_host_int64(int64_t value) {
#ifdef SWAP_BYTE_ORDER
    uint64_t temp = (uint64_t)value;
    temp = db_format_read_be64((const unsigned char *)&temp);
    return (int64_t)temp;
#else
    return value;
#endif
}

/*
 * Floating-point conversion (double)
 *
 * Converts IEEE 754 double to/from big-endian bit representation.
 * The bits are byte-swapped, not the semantic value.
 */

static inline uint64_t host_to_disk_double_bits(double value) {
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    db_format_write_be64(&bits, bits);
    return bits;
}

static inline double disk_to_host_double_bits(uint64_t bits) {
    uint64_t host = db_format_read_be64((const unsigned char *)&bits);
    double value = 0.0;
    memcpy(&value, &host, sizeof(value));
    return value;
}

/*
 * Database address conversion (dbaddress)
 *
 * dbaddress is either 32-bit or 64-bit depending on configuration.
 * This helper handles both cases correctly.
 */

static inline dbaddress host_to_disk_dbaddress(dbaddress value) {
#ifdef SWAP_BYTE_ORDER
    if (sizeof(dbaddress) == 8) {
        unsigned long long temp = (unsigned long long)value;
#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
        temp = __builtin_bswap64(temp);
#else
        temp = ((temp & 0x00000000000000FFULL) << 56) |
               ((temp & 0x000000000000FF00ULL) << 40) |
               ((temp & 0x0000000000FF0000ULL) << 24) |
               ((temp & 0x00000000FF000000ULL) << 8)  |
               ((temp & 0x000000FF00000000ULL) >> 8)  |
               ((temp & 0x0000FF0000000000ULL) >> 24) |
               ((temp & 0x00FF000000000000ULL) >> 40) |
               ((temp & 0xFF00000000000000ULL) >> 56);
#endif
        return (dbaddress)temp;
    }
    return (dbaddress)host_to_disk_int32((int32_t)value);
#else
    return value;
#endif
}

static inline dbaddress disk_to_host_dbaddress(dbaddress value) {
#ifdef SWAP_BYTE_ORDER
    if (sizeof(dbaddress) == 8) {
        unsigned long long temp = (unsigned long long)value;
#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
        temp = __builtin_bswap64(temp);
#else
        temp = ((temp & 0x00000000000000FFULL) << 56) |
               ((temp & 0x000000000000FF00ULL) << 40) |
               ((temp & 0x0000000000FF0000ULL) << 24) |
               ((temp & 0x00000000FF000000ULL) << 8)  |
               ((temp & 0x000000FF00000000ULL) >> 8)  |
               ((temp & 0x0000FF0000000000ULL) >> 24) |
               ((temp & 0x00FF000000000000ULL) >> 40) |
               ((temp & 0xFF00000000000000ULL) >> 56);
#endif
        return (dbaddress)temp;
    }
    return (dbaddress)disk_to_host_int32((int32_t)value);
#else
    return value;
#endif
}

#endif /* byteorder_helpers_include */
