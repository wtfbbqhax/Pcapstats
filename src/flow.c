/* Copyright (c) 2010-2012, Victor J. Roemer. All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 * notice, flowtable list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, flowtable list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * 3. The name of the author may not be used to endorse or promote
 * products derived from flowtable software without specific prior written
 * permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 /* flow.c
  *
  * See RFC 5102 for reference material
  *
  *
  *
  */
#include <config.h>

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "mesg.h"
#include "readconf.h"

#include "timequeue.h"
#include "hashtable.h"

#include <packet.h>
#include "tcp-state.h"

extern Options options;

static Hash *flowtable;
static struct tmq *timeout_queue;

static int _flow_timeout_queue_task(const void *key);
int flow_key_compare(const void *k1, const void *k2);

static struct flow_stats
{
    float avg_rtt;
    float min_rtt;
    float max_rtt;

    uint32_t total_flows;
    uint32_t active_flows;
    uint32_t finished_flows;
} flowstats;

typedef struct
{
    unsigned version;
    struct ipaddr srcaddr;
    struct ipaddr dstaddr;
    uint8_t  protocol;
    uint16_t srcport;
    uint16_t dstport;
    uint32_t octet_count;
    uint32_t packet_count;
    uint32_t fin_count;
    uint32_t syn_count;
    uint32_t rst_count;
    uint32_t psh_count;
    uint32_t ack_count;
    uint32_t urg_count;
    uint32_t ece_count;
    uint32_t cwr_count;
    struct timeval time_start;
    struct timeval time_end;
    
    uint8_t     __padding__[1];
} FlowTracker;

typedef struct
{
    struct ipaddr srcaddr;
    struct ipaddr dstaddr;
    uint8_t     protocol;
    uint16_t    srcport;
    uint16_t    dstport;
    uint8_t     padding[1];
} FlowKey;

int flow_remove(FlowKey *key);

int
flow_table_init( )
{
    flowtable = hash_create(options.flow_max_mem);
    if (flowtable == NULL)
        return -1;

    timeout_queue = tmq_create(options.flow_age_limit);
    if (timeout_queue == NULL) {
        free(flowtable);
        return -1;
    }

    timeout_queue->compare = flow_key_compare;
    timeout_queue->task = _flow_timeout_queue_task;

#ifdef ENABLE_PTHREADS
    tmq_start(timeout_queue);
#endif

    return 0;
}

void
flow_table_finalize( )
{
    assert(flowtable);

    FlowTracker *it;
    unsigned i;
    const void *key;

#ifdef ENABLE_PTHREADS
    tmq_stop(timeout_queue);
#endif
    tmq_destroy(timeout_queue);

    timeout_queue = NULL;

    for (it = hash_first(flowtable, &i, &key); it;
         it = hash_next(flowtable, &i, &key))
        flow_remove((FlowKey*)key);

    hash_destroy(flowtable);
}

FlowTracker *
flow_get(FlowKey *key)
{
    assert(flowtable);
    assert(key);

    struct tmq_element *tmq_elem;
    if ((tmq_elem = tmq_find(timeout_queue, &key))) {
        tmq_bump(timeout_queue, tmq_elem);
    }

    return (FlowTracker *)hash_get(flowtable, key, sizeof *key);
}

int
flow_key_compare(const void *k1, const void *k2)
{
    return memcmp(k1, k2, sizeof(FlowKey));
}

int
flow_remove(FlowKey *key)
{
    assert(flowtable);
    assert(key);

    struct tmq_element *tmq_elem = tmq_find(timeout_queue, &key);
    if (tmq_elem)
        tmq_delete(timeout_queue, tmq_elem);
    
    free(hash_remove(flowtable, key, sizeof *key));

    return 0;
}

int
flow_insert(FlowKey *key, FlowTracker *data)
{
    assert(flowtable);
    assert(key);

    struct tmq_element *tmq_elem =
        tmq_element_create(&key, sizeof key);
    tmq_insert(timeout_queue, tmq_elem);

    return hash_insert(flowtable, data, key, sizeof *key);
}

int
_flow_timeout_queue_task(const void *key)
{
    free(hash_remove(flowtable, key, sizeof(FlowKey)));

    return 0;
}

static int track_tcp_flow(Packet *p)
{
    FlowKey flowkey; 
    int state = 0;
    memset(&flowkey, 0, sizeof flowkey);

    flowkey.srcaddr = packet_srcaddr(p);
    flowkey.dstaddr = packet_dstaddr(p);
    flowkey.protocol = packet_protocol(p); 
    flowkey.srcport = packet_srcport(p);
    flowkey.dstport = packet_dstport(p);

    if (ip_compare(&flowkey.srcaddr, &flowkey.dstaddr) == IP_LESSER)
    {
        flowkey.srcaddr = packet_dstaddr(p);
        flowkey.dstaddr = packet_srcaddr(p);
        flowkey.srcport = packet_dstport(p);
        flowkey.dstport = packet_srcport(p);
    }

    FlowTracker *flow = flow_get(&flowkey);
    if (flow == NULL) {
        if ((flow = calloc(1, sizeof *flow)) == NULL) {
            warn("could not allocate flow data");
            return -1;
        }
        flow_insert(&flowkey, flow);
        memset(flow, 0, sizeof *flow);
    }

    flow->octet_count += packet_paysize(p);

    flow->packet_count++;

    return 0;
}

int
track_packet_flow(Packet *p)
{
    FlowKey flowkey; 
    memset(&flowkey, 0, sizeof flowkey);

    flowkey.srcaddr = packet_srcaddr(p);
    flowkey.dstaddr = packet_dstaddr(p);
    flowkey.protocol = packet_protocol(p); 
    flowkey.srcport = packet_srcport(p);
    flowkey.dstport = packet_dstport(p);

    /* if source address is numerically smaller use the
     * destination as the primary keying address */
    if (ip_compare(&flowkey.srcaddr, &flowkey.dstaddr) == IP_LESSER)
    {
        flowkey.srcaddr = packet_dstaddr(p);
        flowkey.dstaddr = packet_srcaddr(p);
        flowkey.srcport = packet_dstport(p);
        flowkey.dstport = packet_srcport(p);
    }

    FlowTracker *flow = flow_get(&flowkey);
    if (flow == NULL) {
        if ((flow = calloc(1, sizeof *flow)) == NULL) {
            warn("could not allocate flow data");
            return -1;
        }
        flow_insert(&flowkey, flow);
        memset(flow, 0, sizeof *flow);

        flow->version = packet_version(p);
        flow->srcaddr = packet_srcaddr(p);
        flow->dstaddr = packet_dstaddr(p);
        flow->srcport = packet_srcport(p);
        flow->dstport = packet_dstport(p);
        flow->protocol= packet_protocol(p);
    }

    flow->octet_count += packet_paysize(p);

    flow->packet_count++;

#ifdef DEBUG
    if (!options.quiet)
    {
        char srcaddr[INET6_ADDRSTRLEN];
        char dstaddr[INET6_ADDRSTRLEN];
        char *protocol= "unknown";

        inet_ntop(AF_INET, &flow->srcaddr, srcaddr, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &flow->dstaddr, dstaddr, INET_ADDRSTRLEN);

        if(packet_version(p) == 6) {
            inet_ntop(AF_INET6, &flow->srcaddr, srcaddr, INET6_ADDRSTRLEN);
            inet_ntop(AF_INET6, &flow->dstaddr, dstaddr, INET6_ADDRSTRLEN);
        }

        if (flow->protocol== IPPROTO_TCP) {
            protocol= "tcp";
        }
        else
        if (flow->protocol== IPPROTO_UDP) {
            protocol= "udp";
        }
        else
        if (flow->protocol== IPPROTO_ICMP ||
            flow->protocol== IPPROTO_ICMPV6) {
            protocol= "icmp";
        }

        if (packet_version(p) == 6)
            printf("[%s]:%-5d -> [%s]:%-5d %s %10"PRIu64" %10"PRIu64"\n",
                srcaddr, flow->srcport, dstaddr, flow->dstport, protocol,
                flow->octet_count, flow->packet_count);
        else
            printf("%s:%-5d -> %s:%-5d %s %10"PRIu64" %10"PRIu64"\n",
                srcaddr, flow->srcport, dstaddr, flow->dstport, protocol,
                flow->octet_count, flow->packet_count);
    }
#endif /* DEBUG */

#ifndef ENABLE_PTHREADS
    tmq_timeout(timeout_queue);
#endif

    return 0;
}
