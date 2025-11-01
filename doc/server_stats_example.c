/**
 * Example code demonstrating how to use the server-level statistics API
 * 
 * This is a code reference showing how to:
 * 1. Retrieve query and response counts for a specific server
 * 2. Track statistics across multiple DNS servers
 * 
 * NOTE: This code demonstrates the API for internal use within Unbound.
 * The statistics functions are not exposed via the public libunbound API.
 * This example is for developers working on Unbound internals or modules.
 * 
 * To use these functions, you need to be working within the Unbound codebase,
 * for example in a custom module or in the daemon code itself.
 */

#include "config.h"
#include "services/cache/infra.h"
#include "util/config_file.h"
#include "util/net_help.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Example function that could be called from within Unbound's query processing */
void example_check_server_stats(struct infra_cache* infra,
                               struct sockaddr_storage* addr,
                               socklen_t addrlen,
                               uint8_t* zone,
                               size_t zonelen,
                               time_t now) 
{
    long long queries_sent, responses_received;
/* Example function that could be called from within Unbound's query processing */
void example_check_server_stats(struct infra_cache* infra,
                               struct sockaddr_storage* addr,
                               socklen_t addrlen,
                               uint8_t* zone,
                               size_t zonelen,
                               time_t now) 
{
    long long queries_sent, responses_received;
    
    /* Retrieve statistics for the server */
    if (infra_get_host_stats(infra, addr, addrlen, zone, zonelen,
                             &queries_sent, &responses_received, now)) {
        /* Calculate loss rate */
        if (queries_sent > 0) {
            double loss_rate = 100.0 * (queries_sent - responses_received) / queries_sent;
            
            /* Log statistics if loss rate is high */
            if (loss_rate > 10.0) {
                verbose(VERB_ALGO, "High packet loss to server: %.1f%% "
                       "(%lld queries, %lld responses)",
                       loss_rate, queries_sent, responses_received);
            }
        }
    }
}

/* Example of how statistics are automatically tracked during query processing */
void example_query_flow(void) 
{
    /*
     * When a query is sent (in serviced_udp_send or serviced_tcp_send):
     * 
     * infra_increment_queries_sent(sq->outnet->infra, &sq->addr, sq->addrlen,
     *                              sq->zone, sq->zonelen, now);
     *
     * When a response is received (in callbacks):
     * 
     * if (error == NETEVENT_NOERROR) {
     *     infra_increment_responses_received(outnet->infra, &sq->addr,
     *                                        sq->addrlen, sq->zone, 
     *                                        sq->zonelen, now);
     * }
     */
}

/* Example of using statistics for server selection */
int example_should_prefer_server(struct infra_cache* infra,
                                 struct sockaddr_storage* addr1,
                                 socklen_t addrlen1,
                                 struct sockaddr_storage* addr2,
                                 socklen_t addrlen2,
                                 uint8_t* zone,
                                 size_t zonelen,
                                 time_t now)
{
    long long q1, r1, q2, r2;
    double rate1 = 0.0, rate2 = 0.0;
    
    /* Get statistics for both servers */
    if (infra_get_host_stats(infra, addr1, addrlen1, zone, zonelen,
                             &q1, &r1, now)) {
        if (q1 > 0) {
            rate1 = (double)r1 / q1;
        }
    }
    
    if (infra_get_host_stats(infra, addr2, addrlen2, zone, zonelen,
                             &q2, &r2, now)) {
        if (q2 > 0) {
            rate2 = (double)r2 / q2;
        }
    }
    
    /* Prefer server with better response rate */
    return rate1 > rate2 ? 1 : 0;
}

/* Complete example showing typical usage pattern */
void complete_example(void) 
{
    /* 
     * Example 1: Query Statistics Tracking
     * =====================================
     * 
     * Unbound automatically tracks statistics when queries are sent
     * and responses are received. The instrumentation happens in:
     * 
     * - serviced_udp_send() for UDP queries
     * - serviced_tcp_send() for TCP queries
     * - serviced_udp_callback() for UDP responses  
     * - serviced_tcp_callback() for TCP responses
     * 
     * No manual intervention is needed for basic tracking.
     */
    
    /*
     * Example 2: Retrieving Statistics
     * =================================
     */
    #if 0
    struct infra_cache* infra = /* from environment */;
    struct sockaddr_storage addr;
    socklen_t addrlen;
    uint8_t* zone = (uint8_t*)"\007example\003com\000";
    size_t zonelen = 13;
    time_t now = time(NULL);
    long long queries, responses;
    
    /* Parse server address */
    if (ipstrtoaddr("8.8.8.8", 53, &addr, &addrlen)) {
        /* Get statistics */
        if (infra_get_host_stats(infra, &addr, addrlen, zone, zonelen,
                                 &queries, &responses, now)) {
            log_info("Server 8.8.8.8 (example.com): "
                    "%lld queries, %lld responses",
                    queries, responses);
        }
    }
    #endif
    
    /*
     * Example 3: Monitoring Multiple Servers
     * =======================================
     */
    #if 0
    const char* servers[] = {"8.8.8.8", "1.1.1.1", "9.9.9.9", NULL};
    int i;
    const char* servers[] = {"8.8.8.8", "1.1.1.1", "9.9.9.9", NULL};
    int i;
    
    for (i = 0; servers[i]; i++) {
        struct sockaddr_storage addr;
        socklen_t addrlen;
        long long q, r;
        
        if (ipstrtoaddr(servers[i], 53, &addr, &addrlen)) {
            if (infra_get_host_stats(infra, &addr, addrlen, zone, zonelen,
                                     &q, &r, now)) {
                double rate = q > 0 ? 100.0 * r / q : 0.0;
                log_info("%s: %lld queries, %lld responses (%.1f%%)",
                        servers[i], q, r, rate);
            }
        }
    }
    #endif
}

