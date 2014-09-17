/* Copyright (c) 2010-2012, Victor J. Roemer. All Rights Reserved.
 * 
 * Redistribution  and  use  in  source   and  binary  forms,  with  or  without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions  of source  code must retain  the above  copyright notice,
 * table list of conditions and the following disclaimer.
 *
 * 2.  Redistributions  in  binary  form  must  reproduce  the  above  copyright
 * notice,  table list  of conditions  and the  following disclaimer  in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. The  name of the  author may  not be used  to endorse or  promote products
 * derived from table software without specific prior written permission.
 *
 * THIS SOFTWARE IS  PROVIDED BY THE COPYRIGHT HOLDERS AND  CONTRIBUTORS "AS IS"
 * AND  ANY  EXPRESS OR  IMPLIED  WARRANTIES,  INCLUDING,  BUT NOT  LIMITED  TO,
 * THE  IMPLIED  WARRANTIES OF  MERCHANTABILITY  AND  FITNESS FOR  A  PARTICULAR
 * PURPOSE  ARE DISCLAIMED.  IN NO  EVENT  SHALL THE  AUTHOR BE  LIABLE FOR  ANY
 * DIRECT, INDIRECT,  INCIDENTAL, SPECIAL,  EXEMPLARY, OR  CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE,  DATA, OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER CAUSED AND
 * ON ANY  THEORY OF LIABILITY, WHETHER  IN CONTRACT, STRICT LIABILITY,  OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include "hashtable.h"
#include "tcp-state.h"

#include <packet.h>

static Hash *table;

typedef struct
{
    struct tcp_pcb a;
    struct tcp_pcb b;
} TCP_SSN;

typedef struct
{
    struct ipaddr ip_a;
    struct ipaddr ip_b;
    uint16_t    port_a;
    uint16_t    port_b;
} TCP_KEY;

int tcpssn_table_init( )
{
    table = hash_create(1024);
    if (table == NULL)
        return -1;

    return 0;
}

void tcpssn_remove(TCP_KEY *key)
{
    free(hash_remove(table, key, sizeof *key));
}

void tcpssn_table_finalize( )
{
    TCP_SSN *it;
    unsigned i;
    const void *key;

    for (it = hash_first(table, &i, &key); it; it = hash_next(table, &i, &key))
        tcpssn_remove((TCP_KEY*)key);

    hash_destroy(table);
}

TCP_SSN *tcpssn_get(TCP_KEY *key)
{
    TCP_SSN *ssn = (TCP_SSN *)hash_get(table, key, sizeof *key);

    if (ssn != NULL)
        return ssn;

    if ((ssn = calloc(1, sizeof *ssn)) == NULL)
        return NULL;

    hash_insert(table, ssn, key, sizeof *key);
    memset(ssn, 0, sizeof *ssn);

    return ssn;
}

void tcp_key_from_packet(TCP_KEY *key, Packet *p, int *dir)
{
    memset(key, 0, sizeof *key);

    key->ip_a = packet_srcaddr(p);
    key->ip_b = packet_dstaddr(p);
    key->port_a = packet_srcport(p);
    key->port_b = packet_dstport(p);
    *dir = 0;

    if (ip_compare(&key->ip_a, &key->ip_b) == IP_LESSER)
    {
        key->ip_a = packet_dstaddr(p);
        key->ip_b = packet_srcaddr(p);
        key->port_a = packet_dstport(p);
        key->port_b = packet_srcport(p);
        *dir = 1;
    }
}

int track_tcp(Packet *p)
{
    TCP_KEY key;
    int dir;
    tcp_key_from_packet(&key, p, &dir);

    TCP_SSN *ssn = tcpssn_get(&key);
    if (ssn == NULL)
    {
        warn("could not get ssn");
        return -1;
    }

    struct tcp_seg seg;
    seg.flags = packet_tcpflags(p);
    seg.seq = packet_seq(p);
    seg.ack = packet_ack(p);
    seg.wnd = packet_win(p);
    seg.len = packet_paysize(p);

    if ((seg.flags & TCP_SYN))
    {
        seg.len += 1;
    }

    if ((seg.flags & TCP_FIN))
    {
        seg.len += 1;
    }

    print_tcb_seg(&seg);

    if (dir)
    {
        tcp_process(&ssn->a, &ssn->b, &seg);
    }
    else
    {
        tcp_process(&ssn->b, &ssn->a, &seg);
    }

    printf("------------\nTCP A\n");
    print_tcb_pcb(&ssn->a);

    printf("------------\nTCP B\n");
    print_tcb_pcb(&ssn->b);
    printf("\n\n");
//    printf("A State = %d\n", ssn->a.state);
//    printf("B State = %d\n\n", ssn->b.state);

    return 0;
}
