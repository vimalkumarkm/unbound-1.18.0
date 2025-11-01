# Server Statistics Implementation - Architecture Diagram

## Data Flow

```
┌─────────────────────────────────────────────────────────────┐
│                    Client Query Request                      │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                   Unbound Query Processing                   │
│                  (services/mesh.c, worker.c)                 │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│              Upstream Query Sending                          │
│          services/outside_network.c                          │
│                                                              │
│  ┌────────────────────┐         ┌────────────────────┐      │
│  │ serviced_udp_send()│         │ serviced_tcp_send()│      │
│  │                    │         │                    │      │
│  │ ✓ Send UDP Query  │         │ ✓ Send TCP Query  │      │
│  │ ✓ Increment Counter│         │ ✓ Increment Counter│      │
│  └─────────┬──────────┘         └─────────┬──────────┘      │
│            │                               │                 │
└────────────┼───────────────────────────────┼─────────────────┘
             │                               │
             │  infra_increment_queries_sent(infra, addr, zone)
             │                               │
             ▼                               ▼
┌─────────────────────────────────────────────────────────────┐
│          Infrastructure Cache (services/cache/infra.c)       │
│                                                              │
│  ┌────────────────────────────────────────────────────┐     │
│  │  struct infra_data                                 │     │
│  │  ┌──────────────────────────────────────────────┐  │     │
│  │  │  Server: 8.8.8.8:53, Zone: example.com      │  │     │
│  │  │  ──────────────────────────────────────────  │  │     │
│  │  │  num_queries_sent: 1234                     │  │     │
│  │  │  num_responses_received: 1189               │  │     │
│  │  │  (other fields: RTT, EDNS, lameness, etc.)  │  │     │
│  │  └──────────────────────────────────────────────┘  │     │
│  │                                                    │     │
│  │  ┌──────────────────────────────────────────────┐  │     │
│  │  │  Server: 1.1.1.1:53, Zone: example.com      │  │     │
│  │  │  ──────────────────────────────────────────  │  │     │
│  │  │  num_queries_sent: 567                      │  │     │
│  │  │  num_responses_received: 565                │  │     │
│  │  └──────────────────────────────────────────────┘  │     │
│  └────────────────────────────────────────────────────┘     │
└─────────────────────────┬───────────────────────────────────┘
                          │
        Response from upstream DNS server
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│              Upstream Response Handling                      │
│          services/outside_network.c                          │
│                                                              │
│  ┌────────────────────┐         ┌────────────────────┐      │
│  │serviced_udp_callback│        │serviced_tcp_callback│     │
│  │                    │         │                    │      │
│  │ ✓ Receive Response│         │ ✓ Receive Response│      │
│  │ ✓ Check NOERROR   │         │ ✓ Check NOERROR   │      │
│  │ ✓ Increment Counter│         │ ✓ Increment Counter│      │
│  └─────────┬──────────┘         └─────────┬──────────┘      │
│            │                               │                 │
└────────────┼───────────────────────────────┼─────────────────┘
             │                               │
             │  infra_increment_responses_received(infra, addr, zone)
             │                               │
             ▼                               ▼
┌─────────────────────────────────────────────────────────────┐
│     Infrastructure Cache Updates (infra_data counters)       │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                  Response to Client                          │
└─────────────────────────────────────────────────────────────┘
```

## Statistics Retrieval API

```
┌─────────────────────────────────────────────────────────────┐
│             Module or Daemon Code                            │
│         (custom module, monitoring code, etc.)               │
│                                                              │
│  Call: infra_get_host_stats(infra, addr, zone,             │
│                             &queries, &responses, now)      │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│          Infrastructure Cache Lookup                         │
│          services/cache/infra.c                              │
│                                                              │
│  1. Lookup (addr, zone) in infra cache                      │
│  2. Read num_queries_sent                                   │
│  3. Read num_responses_received                             │
│  4. Return to caller                                        │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│          Use Statistics for:                                 │
│          • Monitoring                                        │
│          • Server selection                                  │
│          • Load balancing                                    │
│          • Debugging                                         │
└─────────────────────────────────────────────────────────────┘
```

## Key Objects Extended

### struct infra_data (before)
```c
struct infra_data {
    time_t ttl;
    time_t probedelay;
    struct rtt_info rtt;
    int edns_version;
    uint8_t edns_lame_known;
    uint8_t isdnsseclame;
    uint8_t rec_lame;
    uint8_t lame_type_A;
    uint8_t lame_other;
    uint8_t timeout_A;
    uint8_t timeout_AAAA;
    uint8_t timeout_other;
};
```

### struct infra_data (after)
```c
struct infra_data {
    time_t ttl;
    time_t probedelay;
    struct rtt_info rtt;
    int edns_version;
    uint8_t edns_lame_known;
    uint8_t isdnsseclame;
    uint8_t rec_lame;
    uint8_t lame_type_A;
    uint8_t lame_other;
    uint8_t timeout_A;
    uint8_t timeout_AAAA;
    uint8_t timeout_other;
    
    // NEW FIELDS
    long long num_queries_sent;        ← Tracks queries sent to server
    long long num_responses_received;  ← Tracks responses from server
};
```

## Functions Instrumented

### Query Path
```
serviced_udp_send()
    ↓
    Send UDP query to server
    ↓
    infra_increment_queries_sent()  ← NEW

serviced_tcp_send()
    ↓
    Send TCP query to server
    ↓
    infra_increment_queries_sent()  ← NEW
```

### Response Path
```
serviced_udp_callback(error, ...)
    ↓
    if (error == NETEVENT_NOERROR)
        ↓
        infra_increment_responses_received()  ← NEW

serviced_tcp_callback(error, ...)
    ↓
    if (error == NETEVENT_NOERROR)
        ↓
        infra_increment_responses_received()  ← NEW
```

## Statistics Lifecycle

```
Time: T0
─────────────────────────────────────────────────────────────
First query to server 8.8.8.8 for zone example.com

  infra_data entry created:
    num_queries_sent = 0
    num_responses_received = 0

Time: T0 + 1ms
─────────────────────────────────────────────────────────────
Query sent

  infra_increment_queries_sent()
    num_queries_sent = 1
    num_responses_received = 0

Time: T0 + 50ms
─────────────────────────────────────────────────────────────
Response received

  infra_increment_responses_received()
    num_queries_sent = 1
    num_responses_received = 1

Time: T0 + 100ms to T0 + 15min
─────────────────────────────────────────────────────────────
More queries/responses...

    num_queries_sent = 1234
    num_responses_received = 1189

Time: T0 + 15min (TTL expired)
─────────────────────────────────────────────────────────────
Entry expired, next query creates new entry

    num_queries_sent = 0
    num_responses_received = 0
    (counters reset)
```

## Use Case Example: Server Selection

```c
// Get statistics for two potential servers
long long q1, r1, q2, r2;

infra_get_host_stats(infra, &server1_addr, ..., &q1, &r1, now);
infra_get_host_stats(infra, &server2_addr, ..., &q2, &r2, now);

// Calculate success rates
double rate1 = (q1 > 0) ? (double)r1 / q1 : 0.0;
double rate2 = (q2 > 0) ? (double)r2 / q2 : 0.0;

// Select server with better response rate
if (rate1 > rate2) {
    use_server1();
} else {
    use_server2();
}
```
