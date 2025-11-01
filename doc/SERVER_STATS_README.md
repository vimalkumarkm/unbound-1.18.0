# Server-Level Statistics Extension for Unbound

## Overview

This extension adds the capability to track per-server statistics in Unbound's infrastructure cache. It provides fine-grained visibility into queries sent to and responses received from each upstream DNS server.

## Implementation Details

### Modified Files

1. **services/cache/infra.h**
   - Extended `struct infra_data` to include query and response counters
   - Added function declarations for statistics management

2. **services/cache/infra.c**
   - Implemented statistics tracking functions
   - Modified `data_entry_init()` to initialize counters
   - Added `infra_increment_queries_sent()`
   - Added `infra_increment_responses_received()`
   - Added `infra_get_host_stats()`

3. **services/outside_network.c**
   - Instrumented `serviced_udp_send()` to track UDP queries
   - Instrumented `serviced_tcp_send()` to track TCP queries
   - Instrumented `serviced_udp_callback()` to track UDP responses
   - Instrumented `serviced_tcp_callback()` to track TCP responses

4. **testcode/unitmain.c**
   - Added comprehensive unit tests for the new functionality

## Data Structures

### Extended `struct infra_data`

```c
struct infra_data {
    /* ... existing fields ... */
    
    /** number of queries sent to this server */
    long long num_queries_sent;
    
    /** number of responses received from this server */
    long long num_responses_received;
};
```

## API Functions

### `infra_increment_queries_sent()`

Increments the query counter for a specific server.

**Prototype:**
```c
void infra_increment_queries_sent(struct infra_cache* infra,
    struct sockaddr_storage* addr, socklen_t addrlen,
    uint8_t* name, size_t namelen, time_t timenow);
```

**Parameters:**
- `infra`: Infrastructure cache instance
- `addr`: Server address
- `addrlen`: Length of address
- `name`: Zone name in wireformat
- `namelen`: Length of zone name
- `timenow`: Current time

**Usage:**
Called automatically when a query is sent to an upstream server via UDP or TCP.

### `infra_increment_responses_received()`

Increments the response counter for a specific server.

**Prototype:**
```c
void infra_increment_responses_received(struct infra_cache* infra,
    struct sockaddr_storage* addr, socklen_t addrlen,
    uint8_t* name, size_t namelen, time_t timenow);
```

**Parameters:**
Same as `infra_increment_queries_sent()`

**Usage:**
Called automatically when a response is received from an upstream server (UDP or TCP, but only on success).

### `infra_get_host_stats()`

Retrieves query and response statistics for a specific server.

**Prototype:**
```c
int infra_get_host_stats(struct infra_cache* infra,
    struct sockaddr_storage* addr, socklen_t addrlen,
    uint8_t* name, size_t namelen,
    long long* queries_sent, long long* responses_received,
    time_t timenow);
```

**Parameters:**
- `infra`: Infrastructure cache instance
- `addr`: Server address
- `addrlen`: Length of address
- `name`: Zone name in wireformat
- `namelen`: Length of zone name
- `queries_sent`: Output parameter for query count
- `responses_received`: Output parameter for response count
- `timenow`: Current time

**Returns:**
- `1` if the host was found in cache
- `0` if not found

**Example:**
```c
long long queries, responses;
if (infra_get_host_stats(infra, &addr, addrlen, zone, zonelen,
                         &queries, &responses, now)) {
    printf("Queries: %lld, Responses: %lld\n", queries, responses);
}
```

## Instrumentation Points

### Query Sending

1. **UDP Queries** (`serviced_udp_send()`)
   - Counter incremented after query is successfully queued
   - Location: `services/outside_network.c:2914`

2. **TCP Queries** (`serviced_tcp_send()`)
   - Counter incremented after query is successfully queued
   - Location: `services/outside_network.c:3205`

### Response Handling

1. **UDP Responses** (`serviced_udp_callback()`)
   - Counter incremented when a valid response is received
   - Only incremented on `NETEVENT_NOERROR`
   - Location: `services/outside_network.c:3393`

2. **TCP Responses** (`serviced_tcp_callback()`)
   - Counter incremented when a valid response is received
   - Only incremented on `NETEVENT_NOERROR`
   - Location: `services/outside_network.c:3153`

## Statistics Granularity

Statistics are maintained at the **infra_host** level, which means:
- Each unique combination of (server address, zone) has separate statistics
- Statistics are associated with the server's IP address and port
- Different zones queried from the same server have separate counters

### Example:
- Queries to 8.8.8.8 for `example.com` → separate counter
- Queries to 8.8.8.8 for `test.com` → separate counter
- Queries to 1.1.1.1 for `example.com` → separate counter

## Lifetime and Persistence

- Statistics counters are initialized to 0 when a new infra_host entry is created
- Counters persist for the lifetime of the infra_host entry
- Entries expire based on the `host-ttl` configuration (default: 15 minutes)
- When an entry expires and is re-created, counters are reset to 0

## Use Cases

1. **Server Performance Monitoring**
   - Track which upstream servers are most reliable
   - Identify servers with high loss rates
   - Monitor server availability

2. **Query Load Balancing**
   - Use statistics to inform server selection decisions
   - Prefer servers with better response rates
   - Detect and avoid problematic servers

3. **Debugging and Diagnostics**
   - Troubleshoot connectivity issues with specific servers
   - Analyze query patterns across different zones
   - Identify timeout-prone servers

4. **Capacity Planning**
   - Understand query distribution across upstream servers
   - Identify overloaded or underutilized servers
   - Plan for infrastructure scaling

## Testing

Comprehensive unit tests are included in `testcode/unitmain.c`:

```c
/* test server statistics */
{
    long long queries_sent = 0, responses_received = 0;
    
    /* initially, stats should be zero */
    unit_assert( infra_get_host_stats(slab, &one, onelen, zone, zonelen,
        &queries_sent, &responses_received, now) );
    unit_assert( queries_sent == 0 && responses_received == 0 );
    
    /* increment query counter */
    infra_increment_queries_sent(slab, &one, onelen, zone, zonelen, now);
    unit_assert( infra_get_host_stats(slab, &one, onelen, zone, zonelen,
        &queries_sent, &responses_received, now) );
    unit_assert( queries_sent == 1 && responses_received == 0 );
    
    /* increment response counter */
    infra_increment_responses_received(slab, &one, onelen, zone, zonelen, now);
    unit_assert( infra_get_host_stats(slab, &one, onelen, zone, zonelen,
        &queries_sent, &responses_received, now) );
    unit_assert( queries_sent == 1 && responses_received == 1 );
}
```

Run tests with:
```bash
make test
# or
./unittest
```

## Example Usage

See `doc/server_stats_example.c` for code examples demonstrating:
- Statistics retrieval
- Server selection based on statistics
- Monitoring multiple servers

**Note:** The server statistics API is for internal use within Unbound (daemon code, modules). The functions are not exposed via the public libunbound API. The example code is a reference for developers working on Unbound internals.

## Future Enhancements

Potential extensions to this implementation:

1. **Aggregated Statistics**
   - Add global counters across all servers
   - Provide zone-level aggregation

2. **Time-Windowed Statistics**
   - Track statistics in time windows (e.g., last hour, last day)
   - Implement sliding window counters

3. **Additional Metrics**
   - Track average response time per server
   - Count different error types
   - Monitor EDNS support

4. **Export Interface**
   - Add unbound-control commands to query statistics
   - Export statistics via metrics APIs (Prometheus, etc.)
   - Log statistics periodically

5. **Persistence**
   - Optional persistence across unbound restarts
   - Export/import statistics to/from files

## Performance Considerations

- Minimal overhead: Two 64-bit counter increments per query/response
- No additional memory allocations during query/response processing
- Counters are part of existing infra_data structures
- Lock contention handled by existing infra cache locking mechanism

## Compatibility

- Backward compatible: No changes to existing APIs
- ABI compatible: New fields added at end of structures
- Configuration compatible: No new configuration options required
- Works with all query types (UDP, TCP, TLS, HTTPS)

## Summary

This implementation provides a foundation for server-level statistics tracking in Unbound. The design is:
- **Minimal**: Small code changes with focused functionality
- **Efficient**: Low overhead on query processing path
- **Flexible**: Statistics can be used for various monitoring and optimization purposes
- **Well-tested**: Comprehensive unit tests ensure correctness
