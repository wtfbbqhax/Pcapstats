/* Copyright (c) 2010-2012, Victor J. Roemer. All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 * notice, hosttable list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, hosttable list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * 3. The name of the author may not be used to endorse or promote
 * products derived from hosttable software without specific prior written
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

extern Options options;

static Hash *hosttable;
static struct tmq *timeout_queue;

typedef struct
{
    uint16_t sport;
    uint16_t dport;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_octets;
    uint64_t tx_octets;
} Application;

typedef struct
{
    struct ipaddr address;
    unsigned version;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_octets;
    uint64_t tx_octets;
} HostData;

typedef struct
{
    struct ipaddr address;
} HostKey;

static int _host_timeout_queue_task(const void *key);
int host_key_compare(const void *k1, const void *k2);


int host_remove(HostKey *key);

int
host_table_init( )
{
    hosttable = hash_create(options.host_max_mem);
    if (hosttable == NULL)
        return -1;

    timeout_queue = tmq_create(options.host_age_limit);
    if (timeout_queue == NULL) {
        free(hosttable);
        return -1;
    }

    timeout_queue->compare = host_key_compare;
    timeout_queue->task = _host_timeout_queue_task;

    return 0;
}

void
host_table_finalize( )
{
    assert(hosttable);

    HostData *it;
    unsigned i;
    const void *key;

    tmq_destroy(timeout_queue);
    timeout_queue = NULL;

    for (it = hash_first(hosttable, &i, &key); it;
         it = hash_next(hosttable, &i, &key))
        host_remove((HostKey*)key);

    hash_destroy(hosttable);
}

HostData *
host_get(HostKey *key)
{
    assert(hosttable);
    assert(key);

    struct tmq_element *tmq_elem;
    if ((tmq_elem = tmq_find(timeout_queue, &key))) {
        tmq_bump(timeout_queue, tmq_elem);
    }

    return (HostData *)hash_get(hosttable, key, sizeof *key);
}

int
host_key_compare(const void *k1, const void *k2)
{
    if(!k2) return 0; // something is broken
    return memcmp(k1, k2, sizeof(HostKey));
}

int
host_remove(HostKey *key)
{
    assert(hosttable);
    assert(key);

    struct tmq_element *tmq_elem = 
        tmq_find(timeout_queue, &key);
    if (tmq_elem)
        tmq_delete(timeout_queue, tmq_elem);
    
    free(hash_remove(hosttable, key, sizeof *key));

    return 0;
}

int
host_insert(HostKey *key, HostData *data)
{
    assert(hosttable);
    assert(key);

    struct tmq_element *tmq_elem =
        tmq_element_create(&key, sizeof key);
    tmq_insert(timeout_queue, tmq_elem);

    return hash_insert(hosttable, data, key, sizeof *key);
}

int
_host_timeout_queue_task(const void *key)
{
    free(hash_remove(hosttable, key, sizeof(HostKey)));

    return 0;
}

void
print_host(HostData *host)
{
    char addr[INET6_ADDRSTRLEN+1];

    inet_ntop(AF_INET, &host->address, addr, INET_ADDRSTRLEN);

    if(host->version == 6)
        inet_ntop(AF_INET6, &host->address, addr, INET6_ADDRSTRLEN);

    printf("host: %s\n", addr);

    printf("Tx Packets: %"PRIu64"\n", host->tx_packets);
    printf("Tx Octets:  %"PRIu64"\n", host->tx_octets);
    printf("Rx Packets: %"PRIu64"\n", host->rx_packets);
    printf("Rx Octets:  %"PRIu64"\n", host->rx_octets);
}

void
dump_hosts()
{
    HostData *it;
    unsigned i;
    const void *key;

    for (it = hash_first(hosttable, &i, &key); it;
         it = hash_next(hosttable, &i, &key)) {
        print_host(it);
    }
}

int
track_packet_host(Packet *p)
{
    struct ipaddr addr;
    addr = packet_srcaddr(p);

    HostData *host = host_get((HostKey *)&addr);
    if (host == NULL) {
        if ((host = calloc(1, sizeof *host)) == NULL) {
            warn("could not allocate host data");
            return -1;
        }
        if (host_insert((HostKey *)&addr, host) < 0)
            return -1;
        memset(host, 0, sizeof *host);
        host->address = packet_srcaddr(p);
        host->version = packet_version(p);
    }

    if (host != NULL) {
        host->tx_packets++;
        host->tx_octets += packet_paysize(p);
    }

    addr = packet_dstaddr(p);

    host = host_get((HostKey *)&addr);
    if (host == NULL) {
        if ((host = calloc(1, sizeof *host)) == NULL) {
            warn("could not allocate host data");
            return -1;
        }
        if (host_insert((HostKey *)&addr, host) < 0)
            return -1;
        memset(host, 0, sizeof *host);
        host->address = packet_dstaddr(p);
        host->version = packet_version(p);
    }

    if (host != NULL) {
        host->rx_packets++;
        host->rx_octets += packet_paysize(p);
    }

    tmq_timeout(timeout_queue);
    return 0;
}
