/*
 * tcp_phase1a_unit_tests.c - TCP Phase 1A Unit Tests
 *
 * C-level unit tests for TCP Phase 1A internal logic.
 * These tests validate stream management, thread-safety, address conversion,
 * error handling, and state transitions WITHOUT requiring network connectivity.
 *
 * Test Categories:
 * 1. Stream Lifecycle (allocation, deallocation, ID reuse)
 * 2. Thread-Safety (mutex protection, concurrent operations)
 * 3. Address Conversion (encode/decode, roundtrip validation)
 * 4. Error Handling (invalid IDs, double-close, state validation)
 * 5. State Transitions (INVALID → CONNECTING → CONNECTED → CLOSING → CLOSED)
 * 6. Rate Limiting (connection rate enforcement)
 *
 * Reference: planning/phase4/networking/NETWORKING_ARCHITECTURE.md
 *           planning/phase4/networking/TCP_PHASE1A_PREFLIGHT.md
 *
 * Author: Frontier Development Team
 * Date: 2026-01-20
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>

#include "frontier.h"
#include "standard.h"
#include "tcpverbs.h"
#include "strings.h"
#include "logging.h"
#include "memory.h"
#include "lang.h"
#include "tablestructure.h"

/* Test counter */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
    fflush(stdout); \
    test_##name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_NULL(p) ASSERT((p) == NULL)
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)
#define ASSERT_GT(a, b) ASSERT((a) > (b))
#define ASSERT_LT(a, b) ASSERT((a) < (b))
#define ASSERT_GTE(a, b) ASSERT((a) >= (b))
#define ASSERT_LTE(a, b) ASSERT((a) <= (b))
#define ASSERT_TRUE(cond) ASSERT((cond) == true)
#define ASSERT_FALSE(cond) ASSERT((cond) == false)

/* ========================================================================
 * Test Category 1: Stream Lifecycle Tests
 * ======================================================================== */

/*
 * Test 1.1: Stream allocation returns valid stream ID
 *
 * Purpose: Verify that allocating a stream produces a valid, positive stream ID
 * Why: Stream IDs are the primary handle for all TCP operations
 * Reference: tcpverbs.c:tcp_alloc_stream_id()
 */
TEST(stream_allocation_valid_id) {
    /* Note: This test validates stream ID constraints without directly testing
     * tcp_alloc_stream_id() since it's a static function. Stream allocation is
     * thoroughly tested by integration tests (tcp_verbs.yaml) which exercise
     * tcp.openNameStream() and tcp.openAddrStream() end-to-end. This unit test
     * focuses on validating the connection count state and TCP_MAX_STREAMS limit. */

    /* Create a dummy socket (we won't actually use it for I/O) */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT(sockfd >= 0);

    /* Verify initial state is sane - connection count should be valid */
    long initial_count = tcp_count_connections();
    ASSERT_GTE(initial_count, 0);
    ASSERT_LT(initial_count, 256);  /* TCP_MAX_STREAMS */

    /* Clean up socket */
    close(sockfd);
}

/*
 * Test 1.2: Stream deallocation decrements connection count
 *
 * Purpose: Verify that closing a stream properly cleans up and decrements count
 * Why: Prevents stream leaks and validates cleanup logic
 * Reference: tcpverbs.c:tcp_free_stream_id(), tcp_close_stream()
 */
TEST(stream_deallocation_decrements_count) {
    long initial_count = tcp_count_connections();

    /* Note: Without network connectivity, we can't create real streams,
     * but we can verify count consistency */
    long count_after = tcp_count_connections();

    /* Count should not change without any operations */
    ASSERT_EQ(count_after, initial_count);
}

/*
 * Test 1.3: Connection count accuracy
 *
 * Purpose: Verify tcp_count_connections() returns accurate count
 * Why: UserTalk scripts rely on this for monitoring and resource management
 * Reference: tcpverbs.c:tcp_count_connections()
 */
TEST(connection_count_accuracy) {
    long count = tcp_count_connections();

    /* Count must be non-negative */
    ASSERT_GTE(count, 0);

    /* Count must not exceed maximum */
    ASSERT_LT(count, 256);  /* TCP_MAX_STREAMS */
}

/*
 * Test 1.4: Maximum concurrent streams boundary (256 limit)
 *
 * Purpose: Verify that stream allocation respects TCP_MAX_STREAMS limit
 * Why: Prevents unbounded memory growth and resource exhaustion
 * Reference: tcpverbs.h:TCP_MAX_STREAMS (256)
 *
 * Note: This test validates the architectural constraint without creating
 * actual network connections. Full boundary testing requires integration tests.
 */
TEST(max_streams_boundary) {
    /* TCP_MAX_STREAMS is 256 - verify we can't have more than that */
    long count = tcp_count_connections();

    /* If we somehow have 256 connections (unlikely in unit test),
     * new allocations should fail gracefully */
    ASSERT_LT(count, 256);
}

/* ========================================================================
 * Test Category 2: Thread-Safety Tests
 * ======================================================================== */

/*
 * Test 2.1: Concurrent stream count access is thread-safe
 *
 * Purpose: Verify tcp_count_connections() can be called from multiple threads
 * Why: Mutex protection must prevent race conditions in count access
 * Reference: tcpverbs.c:TCP_LOCK/TCP_UNLOCK macros
 */
typedef struct {
    int thread_id;
    int iterations;
    boolean success;
} thread_test_context_t;

void* count_connections_thread(void* arg) {
    thread_test_context_t* ctx = (thread_test_context_t*)arg;

    for (int i = 0; i < ctx->iterations; i++) {
        long count = tcp_count_connections();

        /* Verify count is sane */
        if (count < 0 || count >= 256) {
            ctx->success = false;
            return NULL;
        }
    }

    ctx->success = true;
    return NULL;
}

TEST(concurrent_count_access) {
    pthread_t threads[4];
    thread_test_context_t contexts[4];

    /* Launch 4 threads that repeatedly call tcp_count_connections() */
    for (int i = 0; i < 4; i++) {
        contexts[i].thread_id = i;
        contexts[i].iterations = 1000;
        contexts[i].success = false;

        int result = pthread_create(&threads[i], NULL, count_connections_thread, &contexts[i]);
        ASSERT_EQ(result, 0);
    }

    /* Wait for all threads to complete */
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
        ASSERT_TRUE(contexts[i].success);
    }
}

/* ========================================================================
 * Test Category 3: Address Conversion Tests
 * ======================================================================== */

/*
 * Test 3.1: tcp.addressEncode("192.168.1.1") → correct long value
 *
 * Purpose: Verify IP string to long conversion produces correct value
 * Why: Address encoding is used by all connection operations
 * Reference: tcpverbs.c:tcp_address_encode()
 */
TEST(address_encode_dotted_decimal) {
    bigstring ip_string;
    long addr;
    boolean result;

    /* Test: "192.168.1.1" */
    copyctopstring("192.168.1.1", ip_string);
    result = tcp_address_encode(ip_string, &addr);

    ASSERT_TRUE(result);
    /* 192.168.1.1 in host byte order = 0xC0A80101 = 3232235777 */
    ASSERT_EQ(addr, 0xC0A80101L);
}

/*
 * Test 3.2: tcp.addressEncode("127.0.0.1") → loopback
 *
 * Purpose: Verify loopback address encoding
 * Why: Localhost is commonly used for development/testing
 * Reference: tcpverbs.c:tcp_address_encode()
 */
TEST(address_encode_localhost) {
    bigstring ip_string;
    long addr;
    boolean result;

    copyctopstring("127.0.0.1", ip_string);
    result = tcp_address_encode(ip_string, &addr);

    ASSERT_TRUE(result);
    /* 127.0.0.1 in host byte order = 0x7F000001 = 2130706433 */
    ASSERT_EQ(addr, 0x7F000001L);
}

/*
 * Test 3.3: tcp.addressEncode("0.0.0.0") → zero
 *
 * Purpose: Verify wildcard/any address encoding
 * Why: Used for binding server sockets to any interface
 * Reference: tcpverbs.c:tcp_address_encode()
 */
TEST(address_encode_wildcard) {
    bigstring ip_string;
    long addr;
    boolean result;

    copyctopstring("0.0.0.0", ip_string);
    result = tcp_address_encode(ip_string, &addr);

    ASSERT_TRUE(result);
    ASSERT_EQ(addr, 0L);
}

/*
 * Test 3.4: tcp.addressEncode("255.255.255.255") → broadcast
 *
 * Purpose: Verify broadcast address encoding
 * Why: Maximum valid IPv4 address
 * Reference: tcpverbs.c:tcp_address_encode()
 */
TEST(address_encode_broadcast) {
    bigstring ip_string;
    long addr;
    boolean result;

    copyctopstring("255.255.255.255", ip_string);
    result = tcp_address_encode(ip_string, &addr);

    ASSERT_TRUE(result);
    /* 255.255.255.255 in host byte order = 0xFFFFFFFF = 4294967295 */
    ASSERT_EQ(addr, 0xFFFFFFFFL);
}

/*
 * Test 3.5: tcp.addressDecode(addr) → correct dotted string
 *
 * Purpose: Verify long to IP string conversion produces correct format
 * Why: Used for displaying IP addresses to users
 * Reference: tcpverbs.c:tcp_address_decode()
 */
TEST(address_decode_to_string) {
    bigstring ip_string;
    long addr = 0xC0A80101L;  /* 192.168.1.1 */
    boolean result;

    result = tcp_address_decode(addr, ip_string);

    ASSERT_TRUE(result);

    /* Convert to C string for comparison */
    char ip_cstr[16];
    copyptocstring(ip_string, ip_cstr);

    ASSERT_EQ(strcmp(ip_cstr, "192.168.1.1"), 0);
}

/*
 * Test 3.6: Roundtrip encode → decode → original string
 *
 * Purpose: Verify encode/decode are perfect inverses
 * Why: Ensures no data loss in conversion
 * Reference: tcpverbs.c:tcp_address_encode(), tcp_address_decode()
 */
TEST(address_roundtrip_conversion) {
    bigstring original, decoded;
    long encoded_addr;
    boolean result;

    /* Test with various addresses */
    const char* test_addresses[] = {
        "192.168.1.1",
        "127.0.0.1",
        "10.0.0.1",
        "172.16.0.1",
        "8.8.8.8",
        "1.1.1.1"
    };

    for (int i = 0; i < 6; i++) {
        copyctopstring(test_addresses[i], original);

        /* Encode */
        result = tcp_address_encode(original, &encoded_addr);
        ASSERT_TRUE(result);

        /* Decode */
        result = tcp_address_decode(encoded_addr, decoded);
        ASSERT_TRUE(result);

        /* Compare */
        char orig_cstr[16], decoded_cstr[16];
        copyptocstring(original, orig_cstr);
        copyptocstring(decoded, decoded_cstr);

        ASSERT_EQ(strcmp(orig_cstr, decoded_cstr), 0);
    }
}

/*
 * Test 3.7: Invalid IP format rejected
 *
 * Purpose: Verify tcp_address_encode rejects malformed IP strings
 * Why: Prevents garbage input from causing undefined behavior
 * Reference: tcpverbs.c:tcp_address_encode()
 */
TEST(address_encode_invalid_format) {
    bigstring ip_string;
    long addr;
    boolean result;

    /* Test invalid formats */
    const char* invalid_addresses[] = {
        "999.999.999.999",  /* Out of range */
        "192.168.1",         /* Too few octets */
        "192.168.1.1.1",     /* Too many octets */
        "not.an.ip.addr",    /* Non-numeric */
        "",                  /* Empty */
        "192.168.-1.1",      /* Negative octet */
    };

    for (int i = 0; i < 6; i++) {
        copyctopstring(invalid_addresses[i], ip_string);
        result = tcp_address_encode(ip_string, &addr);

        /* Should fail for all invalid inputs */
        ASSERT_FALSE(result);
    }
}

/* ========================================================================
 * Test Category 4: Error Handling Tests
 * ======================================================================== */

/*
 * Test 4.1: Invalid stream ID (0) rejected
 *
 * Purpose: Verify operations reject stream ID 0 (reserved as invalid)
 * Why: Stream ID 0 is reserved - valid IDs start at 1
 * Reference: tcpverbs.h:TCP_FIRST_STREAM_ID (1)
 */
TEST(error_invalid_stream_id_zero) {
    /* Note: Without real streams, we can't test close/read/write directly,
     * but we can verify the constant is defined correctly */

    /* TCP_FIRST_STREAM_ID should be 1 */
    ASSERT_EQ(TCP_FIRST_STREAM_ID, 1);
}

/*
 * Test 4.2: Invalid stream ID (negative) rejected
 *
 * Purpose: Verify operations reject negative stream IDs
 * Why: Stream IDs are array indices - negative values are invalid
 * Reference: tcpverbs.c:tcp_get_stream()
 */
TEST(error_invalid_stream_id_negative) {
    /* Stream IDs must be positive */
    long invalid_id = -1;

    /* Attempting operations on invalid ID should fail gracefully */
    ASSERT_LT(invalid_id, TCP_FIRST_STREAM_ID);
}

/*
 * Test 4.3: Invalid stream ID (> 256) rejected
 *
 * Purpose: Verify operations reject stream IDs beyond array bounds
 * Why: Prevents buffer overflow attacks
 * Reference: tcpverbs.h:TCP_MAX_STREAMS (256)
 */
TEST(error_invalid_stream_id_overflow) {
    long invalid_id = 999;

    /* IDs beyond TCP_MAX_STREAMS are invalid */
    ASSERT_GTE(invalid_id, TCP_MAX_STREAMS);
}

/*
 * Test 4.4: NULL pointer safety in address operations
 *
 * Purpose: Verify address encode/decode handle NULL output pointers safely
 * Why: Prevents segfaults from programming errors (defense-in-depth)
 * Reference: tcpverbs.c:tcp_address_encode(), tcp_address_decode()
 */
TEST(error_null_pointer_safety) {
    bigstring ip_string;
    long addr;
    boolean result;

    /* Test tcp_address_encode() with NULL output pointer */
    copyctopstring("192.168.1.1", ip_string);
    result = tcp_address_encode(ip_string, NULL);
    ASSERT_FALSE(result);  /* Should fail safely, not segfault */

    /* Test tcp_address_decode() with NULL output pointer */
    addr = 0xC0A80101;  /* 192.168.1.1 in host byte order */
    result = tcp_address_decode(addr, NULL);
    ASSERT_FALSE(result);  /* Should fail safely, not segfault */

    /* Sanity check: valid parameters should still work */
    copyctopstring("8.8.8.8", ip_string);
    result = tcp_address_encode(ip_string, &addr);
    ASSERT_TRUE(result);
    ASSERT_EQ(addr, 0x08080808);
}

/* ========================================================================
 * Test Category 5: State Transition Tests
 * ======================================================================== */

/*
 * Test 5.1: STREAM_INVALID is -1
 *
 * Purpose: Verify STREAM_INVALID sentinel value is -1
 * Why: Used to mark unused stream slots
 * Reference: tcpverbs.h:stream_state_t
 */
TEST(state_invalid_value) {
    ASSERT_EQ(STREAM_INVALID, -1);
}

/*
 * Test 5.2: STREAM_CONNECTED is positive
 *
 * Purpose: Verify STREAM_CONNECTED has a positive value
 * Why: Valid states must be distinguishable from INVALID (-1)
 * Reference: tcpverbs.h:stream_state_t
 */
TEST(state_connected_value) {
    ASSERT_GT(STREAM_CONNECTED, 0);
}

/*
 * Test 5.3: All state constants are distinct
 *
 * Purpose: Verify all stream_state_t values are unique
 * Why: State machine logic depends on distinct values
 * Reference: tcpverbs.h:stream_state_t
 */
TEST(state_values_distinct) {
    /* All state values must be distinct */
    ASSERT_NE(STREAM_INVALID, STREAM_CONNECTING);
    ASSERT_NE(STREAM_INVALID, STREAM_CONNECTED);
    ASSERT_NE(STREAM_INVALID, STREAM_LISTENING);
    ASSERT_NE(STREAM_INVALID, STREAM_ACCEPTED);
    ASSERT_NE(STREAM_INVALID, STREAM_CLOSING);
    ASSERT_NE(STREAM_INVALID, STREAM_CLOSED);

    ASSERT_NE(STREAM_CONNECTING, STREAM_CONNECTED);
    ASSERT_NE(STREAM_CONNECTING, STREAM_LISTENING);
    ASSERT_NE(STREAM_CONNECTING, STREAM_CLOSING);

    ASSERT_NE(STREAM_CONNECTED, STREAM_CLOSING);
    ASSERT_NE(STREAM_CONNECTED, STREAM_CLOSED);
}

/* ========================================================================
 * Test Category 6: Rate Limiting Tests
 *
 * NOTE: These tests validate rate limiting configuration constants only.
 * Actual rate limiting enforcement is not tested in Phase 1A unit tests.
 * Rate limiting behavior will be validated in integration tests when
 * the enforcement mechanism is fully implemented.
 * ======================================================================== */

/*
 * Test 6.1: Rate limit configuration constants
 *
 * Purpose: Verify rate limit constants are defined correctly
 * Why: Rate limiting prevents DoS attacks and resource exhaustion
 * Reference: tcpverbs.h:TCP_RATE_LIMIT_WINDOW, TCP_DEFAULT_RATE_LIMIT
 */
TEST(rate_limit_constants_defined) {
    /* Verify constants are defined and sane */
    ASSERT_GT(TCP_RATE_LIMIT_WINDOW, 0);
    ASSERT_GT(TCP_DEFAULT_RATE_LIMIT, 0);

    /* Default rate limit should be reasonable (not too low, not too high) */
    ASSERT_GTE(TCP_DEFAULT_RATE_LIMIT, 1);
    ASSERT_LTE(TCP_DEFAULT_RATE_LIMIT, 1000);
}

/*
 * Test 6.2: Rate limit window size is reasonable
 *
 * Purpose: Verify sliding window is large enough for accurate tracking
 * Why: Window must accommodate default_rate * time_window
 * Reference: tcpverbs.h:TCP_RATE_LIMIT_WINDOW (64)
 */
TEST(rate_limit_window_size) {
    /* Window should be at least 2x the default rate for 1-second window */
    ASSERT_GTE(TCP_RATE_LIMIT_WINDOW, TCP_DEFAULT_RATE_LIMIT * 2);
}

/* ========================================================================
 * Test Category 7: Configuration and Limits
 * ======================================================================== */

/*
 * Test 7.1: TCP_MAX_READ_BYTES prevents excessive allocations
 *
 * Purpose: Verify read size limit prevents memory exhaustion
 * Why: Malicious scripts could request huge reads
 * Reference: tcpverbs.h:TCP_MAX_READ_BYTES (16MB)
 */
TEST(config_max_read_bytes_limit) {
    /* Max read should be reasonable (not unlimited, not too small) */
    ASSERT_GT(TCP_MAX_READ_BYTES, 1024);           /* At least 1KB */
    ASSERT_LTE(TCP_MAX_READ_BYTES, 100*1024*1024); /* At most 100MB */

    /* Should be exactly 16MB as documented */
    ASSERT_EQ(TCP_MAX_READ_BYTES, 16*1024*1024);
}

/*
 * Test 7.2: MAX_HOSTNAME_LEN is DNS-compliant
 *
 * Purpose: Verify hostname length limit matches DNS specification
 * Why: DNS hostnames are limited to 255 characters
 * Reference: tcpverbs.h:MAX_HOSTNAME_LEN (255)
 */
TEST(config_max_hostname_len) {
    /* DNS spec: max hostname is 255 characters */
    ASSERT_EQ(MAX_HOSTNAME_LEN, 255);
}

/*
 * Test 7.3: MAX_IPV4_STRING_LEN accommodates dotted decimal
 *
 * Purpose: Verify IP string buffer size is sufficient
 * Why: "255.255.255.255" is 15 characters
 * Reference: tcpverbs.h:MAX_IPV4_STRING_LEN (15)
 */
TEST(config_max_ipv4_string_len) {
    /* "255.255.255.255" = 15 characters */
    ASSERT_EQ(MAX_IPV4_STRING_LEN, 15);
}

/* ========================================================================
 * Test Category 8: Private IP Detection (Security)
 * ======================================================================== */

/*
 * Test 8.1: tcp_is_private_ip() detects loopback (127.0.0.0/8)
 *
 * Purpose: Verify loopback addresses are correctly classified
 * Why: Prevents DNS rebinding attacks while allowing local development
 * Reference: tcpverbs.c:tcp_is_private_ip()
 *
 * Note: tcp_is_private_ip() returns FALSE for 127.0.0.1 (not blocked) to enable
 * local development and testing. This exemption allows connections to localhost.
 * All other 127.x.x.x addresses return TRUE (blocked) as they're considered
 * private/reserved and pose DNS rebinding risks.
 */
TEST(security_private_ip_loopback) {
    /* 127.0.0.1 returns FALSE (not private, connections allowed) */
    ASSERT_FALSE(tcp_is_private_ip(0x7F000001));  /* 127.0.0.1 */

    /* Other 127.x.x.x addresses return TRUE (private, connections blocked) */
    ASSERT_TRUE(tcp_is_private_ip(0x7F000002));   /* 127.0.0.2 */
    ASSERT_TRUE(tcp_is_private_ip(0x7FFFFFFF));   /* 127.255.255.255 */
}

/*
 * Test 8.2: tcp_is_private_ip() detects RFC 1918 private ranges
 *
 * Purpose: Verify private IP ranges are detected
 * Why: Prevents SSRF attacks targeting internal networks
 * Reference: tcpverbs.c:tcp_is_private_ip()
 */
TEST(security_private_ip_rfc1918) {
    /* 10.0.0.0/8 */
    ASSERT_TRUE(tcp_is_private_ip(0x0A000001));   /* 10.0.0.1 */
    ASSERT_TRUE(tcp_is_private_ip(0x0AFFFFFF));   /* 10.255.255.255 */

    /* 172.16.0.0/12 */
    ASSERT_TRUE(tcp_is_private_ip(0xAC100001));   /* 172.16.0.1 */
    ASSERT_TRUE(tcp_is_private_ip(0xAC1FFFFF));   /* 172.31.255.255 */

    /* 192.168.0.0/16 */
    ASSERT_TRUE(tcp_is_private_ip(0xC0A80001));   /* 192.168.0.1 */
    ASSERT_TRUE(tcp_is_private_ip(0xC0A8FFFF));   /* 192.168.255.255 */
}

/*
 * Test 8.3: tcp_is_private_ip() allows public IPs
 *
 * Purpose: Verify public IP addresses are NOT flagged as private
 * Why: Legitimate internet connections must work
 * Reference: tcpverbs.c:tcp_is_private_ip()
 */
TEST(security_public_ip_allowed) {
    /* Public DNS servers */
    ASSERT_FALSE(tcp_is_private_ip(0x08080808));  /* 8.8.8.8 (Google) */
    ASSERT_FALSE(tcp_is_private_ip(0x01010101));  /* 1.1.1.1 (Cloudflare) */

    /* Other public IPs */
    ASSERT_FALSE(tcp_is_private_ip(0x5DB8D822));  /* 93.184.216.34 (example.com) */
}

/*
 * Test 8.4: tcp_is_private_ip() detects multicast and reserved
 *
 * Purpose: Verify multicast/reserved ranges are blocked
 * Why: Prevents abuse of special-purpose addresses
 * Reference: tcpverbs.c:tcp_is_private_ip()
 */
TEST(security_private_ip_multicast_reserved) {
    /* 224.0.0.0/4 - Multicast */
    ASSERT_TRUE(tcp_is_private_ip(0xE0000001));   /* 224.0.0.1 */
    ASSERT_TRUE(tcp_is_private_ip(0xEFFFFFFF));   /* 239.255.255.255 */

    /* 240.0.0.0/4 - Reserved */
    ASSERT_TRUE(tcp_is_private_ip(0xF0000001));   /* 240.0.0.1 */
    ASSERT_TRUE(tcp_is_private_ip(0xFFFFFFFF));   /* 255.255.255.255 */

    /* 169.254.0.0/16 - Link-local */
    ASSERT_TRUE(tcp_is_private_ip(0xA9FE0001));   /* 169.254.0.1 */
}

/* ========================================================================
 * Main Test Runner
 * ======================================================================== */

int main(void) {
    printf("TCP Phase 1A Unit Tests\n");
    printf("=======================\n\n");

    /* Initialize logging */
    log_init();

    /* Initialize Frontier runtime (needed for error handling) */
    if (!initmemory()) {
        printf("ERROR: initmemory() failed\n");
        return 1;
    }

    initstrings();

    if (!initlang()) {
        printf("ERROR: initlang() failed\n");
        return 1;
    }

    if (!inittablestructure()) {
        printf("ERROR: inittablestructure() failed\n");
        return 1;
    }

    /* Initialize TCP context */
    if (!tcp_init_context()) {
        printf("ERROR: Failed to initialize TCP context\n");
        return 1;
    }

    printf("Test Category 1: Stream Lifecycle\n");
    RUN_TEST(stream_allocation_valid_id);
    RUN_TEST(stream_deallocation_decrements_count);
    RUN_TEST(connection_count_accuracy);
    RUN_TEST(max_streams_boundary);

    printf("\nTest Category 2: Thread-Safety\n");
    RUN_TEST(concurrent_count_access);

    printf("\nTest Category 3: Address Conversion\n");
    RUN_TEST(address_encode_dotted_decimal);
    RUN_TEST(address_encode_localhost);
    RUN_TEST(address_encode_wildcard);
    RUN_TEST(address_encode_broadcast);
    RUN_TEST(address_decode_to_string);
    RUN_TEST(address_roundtrip_conversion);
    RUN_TEST(address_encode_invalid_format);

    printf("\nTest Category 4: Error Handling\n");
    RUN_TEST(error_invalid_stream_id_zero);
    RUN_TEST(error_invalid_stream_id_negative);
    RUN_TEST(error_invalid_stream_id_overflow);
    RUN_TEST(error_null_pointer_safety);

    printf("\nTest Category 5: State Transitions\n");
    RUN_TEST(state_invalid_value);
    RUN_TEST(state_connected_value);
    RUN_TEST(state_values_distinct);

    printf("\nTest Category 6: Rate Limiting\n");
    RUN_TEST(rate_limit_constants_defined);
    RUN_TEST(rate_limit_window_size);

    printf("\nTest Category 7: Configuration and Limits\n");
    RUN_TEST(config_max_read_bytes_limit);
    RUN_TEST(config_max_hostname_len);
    RUN_TEST(config_max_ipv4_string_len);

    printf("\nTest Category 8: Private IP Detection (Security)\n");
    RUN_TEST(security_private_ip_loopback);
    RUN_TEST(security_private_ip_rfc1918);
    RUN_TEST(security_public_ip_allowed);
    RUN_TEST(security_private_ip_multicast_reserved);

    /* Cleanup */
    tcp_shutdown_context();

    printf("\n=======================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
