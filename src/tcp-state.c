/* Copyright (c) 2012, Victor J Roemer. All Rights Reserved.
 *
 * Redistribution  and  use  in  source   and  binary  forms,  with  or  without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions  of source  code must retain  the above  copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form  must reproduce the above copyright notice,
 * this list  of conditions  and the following  disclaimer in  the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. The  name of the  author may  not be used  to endorse or  promote products
 * derived from this software without specific prior written permission.
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

/*******************************************************************************
 *                              TCP State Machine                              *
 *******************************************************************************/


#define __USE_BSD

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "tcp-state.h"

#define TCP_SEQ_LT(a,b) ((int32_t)((a)-(b)) < 0)
#define TCP_SEQ_LEQ(a,b) ((int32_t)((a)-(b)) <= 0)
#define TCP_SEQ_EQ(a,b) ((int32_t)((a)-(b)) == 0)
#define TCP_SEQ_GT(a,b) ((int32_t)((a)-(b)) > 0)
#define TCP_SEQ_GEQ(a,b) ((int32_t)((a)-(b)) >= 0)

/* b <= a <= c */
#define TCP_SEQ_BETWEEN(a,b,c) (TCP_SEQ_GEQ(a,b) && TCP_SEQ_LEQ(a,c))

enum {
    CLOSED,
    SYN_SENT,
    SYN_RCVD,
    ESTABLISHED,
    FIN_WAIT_1,
    FIN_WAIT_2,
    CLOSING,
    TIME_WAIT,
    CLOSE_WAIT,
    LAST_ACK,
    MAX_STATE
};

char *state_name[] =
{
    "CLOSED",
    "SYN_SENT",
    "SYN_RCVD",
    "ESTABLISHED",
    "FIN_WAIT_1",
    "FIN_WAIT_2",
    "CLOSING",
    "TIME_WAIT",
    "CLOSE_WAIT",
    "LAST_ACK",
    NULL
};

char *tcp_flag_str(int flags)
{
    static char buf[9];
    const int fin = 0x01;
    const int syn = 0x02;
    const int rst = 0x04;
    const int psh = 0x08;
    const int ack = 0x10;
    const int urg = 0x20;
    const int ecn = 0x40;
    const int cwr = 0x80;
    int i = 0;
    
    if(flags & fin) buf[i++] = 'F';
    if(flags & syn) buf[i++] = 'S';
    if(flags & rst) buf[i++] = 'R';
    if(flags & psh) buf[i++] = 'P';
    if(flags & ack) buf[i++] = '.';
    if(flags & urg) buf[i++] = 'U';
    if(flags & ecn) buf[i++] = 'E';
    if(flags & cwr) buf[i++] = 'W';
    buf[i] = '\0';

    return buf;
}

void print_tcb_seg(struct tcp_seg *seg)
{
    printf("flags [%s] seq:%"PRIu32" ack:%"PRIu32" wnd:%"PRIu32" len:%"PRIu32"\n",
        tcp_flag_str(seg->flags), seg->seq, seg->ack, seg->wnd, seg->len);
}

void print_tcb_pcb(struct tcp_pcb *pcb)
{
    printf("STATE %s\n", state_name[pcb->state]);
    printf("UNA   %"PRIu32"\n", pcb->una - pcb->isn);
    printf("WND   %"PRIu32"\n", pcb->wnd);
    if (!(pcb->una - pcb->isn))
        printf("ISN   %"PRIu32"\n", pcb->isn);
    printf("NXT   %"PRIu32"\n", pcb->nxt - pcb->isn);
}

/* tcp process
 *
 * A TCP State machine.
 *
 * Arguments: rcv: receiving hosts pcb
 *            snd: sending hosts pcb
 *            seg: segment from the receiving host
 */
int tcp_process(struct tcp_pcb *snd, struct tcp_pcb *rcv, struct tcp_seg *seg)
{
    bool acceptable = false;

    if (seg->flags & TCP_RST)
    {
        if (rcv->state == SYN_SENT)
        {
            if (seg->ack == rcv->una + 1)
                acceptable = true;
        }
        else if (TCP_SEQ_BETWEEN(seg->seq, rcv->una + 1,
            rcv->una + rcv->wnd + 1))
        {
            acceptable = true;
        }

        if (acceptable)
        {
            rcv->state = CLOSED;
            snd->state = CLOSED;
            return 0;
        }

        return -1;
    }

    switch(rcv->state)
    {
        case CLOSED:
            /* Ignore ACK's for now */
            if (seg->flags & TCP_ACK)
                break;

            /* Receive a SYN goto SYN_RCVD */
            if (!(seg->flags & TCP_SYN))
                break;

            snd->isn = seg->seq;
            snd->una = seg->seq;
            snd->nxt = seg->seq + seg->len;
            snd->state = SYN_SENT;

            rcv->wnd = seg->wnd;
            rcv->state = SYN_RCVD;

            acceptable = true;
            break;

        case SYN_SENT:
            /* Sender is acknowledging receivers SYN */
            if (seg->flags & TCP_ACK)
            {
                if (!TCP_SEQ_BETWEEN(seg->ack, rcv->una, rcv->nxt))
                    break;

                if (seg->flags & TCP_SYN)
                {
                    /* Sender is already established, fuck off! */
                    if (snd->state == ESTABLISHED)
                        break;

                    snd->isn = seg->seq;
                    snd->una = seg->seq;
                    snd->state = SYN_SENT;
                }

                snd->nxt = seg->seq + seg->len;

                rcv->una = seg->ack;
                rcv->wnd = seg->wnd;
                rcv->state = ESTABLISHED;

                acceptable = true;
            }
            /* Sender did not acknowledge receivers SYN, check for 4way
             * handshake */
            else if (seg->flags & TCP_SYN)
            {
                snd->isn = seg->seq;
                snd->una = seg->seq;
                snd->nxt = seg->seq + seg->len;
                snd->state = SYN_SENT;

                rcv->wnd = seg->wnd;
                rcv->state = SYN_RCVD;

                acceptable = true;
            }
            break;

        case SYN_RCVD:
            break;

        case ESTABLISHED:
            /* Check that segment is in the window */
            /* FIXME Seqno could be outside window with data 'inside' the
             * window */
            if (!TCP_SEQ_BETWEEN(seg->seq, snd->una,
                snd->una + snd->wnd + 1))
                break;

            /* Check that segment data is within the window */
            /* TODO trim data to fit inside the window */
            if (!TCP_SEQ_BETWEEN(seg->seq + seg->len, snd->una,
                snd->una + snd->wnd + 1))
                break;

#if 0
XXX Future code
            if (!TCP_SEQ_EQ(seg->seq, rcv->nxt)) { Out-of-order segments }
            if (TCP_SEQ_LT(seg->seq, rcv->nxt) &&
                TCP_SEQ_GEQ(seg->seg, rcv->una))
            {
                Retransmitted data
            }
#endif

            /* Receiving a SYN inside the window is an error */
            if (seg->flags & TCP_SYN)
                break;

            /* Check the ack and that the ackno is good */
            if (seg->flags & TCP_ACK)
            {
                /* XXX Or, should this be snd.una + 1? */
                /* Make sure it acknowledges 'something' that we sent */
                if (!TCP_SEQ_BETWEEN(seg->ack, rcv->una, rcv->nxt))
                    break;

                /* Adjust sending window */
                rcv->una = seg->ack;
                rcv->wnd = seg->wnd;
                /* XXX I probably shouldn't presume that the host is going to
                 * start back at seg->ack? */
                /* rcv->nxt = seg->ack */
            }

            /* TODO Check if seqno needs to be an exact value */
            if (seg->flags & TCP_FIN)
            {
                rcv->state = CLOSE_WAIT;
                snd->state = FIN_WAIT_1;
            }

            /* XXX These assignments need to be moved into ACK handling above
             * if all data segments are required to have an ACK */
            snd->una = seg->seq;
            snd->nxt = seg->seq + seg->len;

            acceptable = true;
            break;

        case FIN_WAIT_1:
            if (!TCP_SEQ_BETWEEN(seg->seq, snd->una,
                snd->una + snd->wnd + 1))
                break;

            if (!TCP_SEQ_BETWEEN(seg->seq + seg->len, snd->una,
                snd->una + snd->wnd + 1))
                break;

            /* Receiving a SYN inside the window is an error */
            if (seg->flags & TCP_SYN)
                break;

            if (seg->flags & TCP_FIN)
            {
                rcv->state = CLOSING;
                snd->state = LAST_ACK;
            }

            if (seg->flags & TCP_ACK)
            {
                /* Ack needs to be exact */
                /* Using snd.nxt because we could have sent data with the FIN */
                if (!TCP_SEQ_EQ(seg->ack, rcv->nxt))
                    break;

                /* XXX if ACK == snd.una goto ESTABLISHED? */

                rcv->state++; // Clever hack to put it into TIME_WAIT if FIN was set

                rcv->una = seg->ack;
                rcv->wnd = seg->wnd;
            }

            snd->una = seg->seq;
            snd->nxt = seg->seq + seg->len;

            acceptable = true;
            break;

        case FIN_WAIT_2:
            if (!TCP_SEQ_BETWEEN(seg->seq, snd->una, snd->una + snd->wnd + 1))
                break;

            if (!TCP_SEQ_BETWEEN(seg->seq+seg->len, snd->una,
                snd->una + snd->wnd + 1))
                break;

            /* I think the ACK needs to be exact.. maybe? */
            if (seg->flags & TCP_ACK && !TCP_SEQ_EQ(seg->ack, rcv->nxt))
                break;

            /* Receiving a SYN inside the window is an error */
            if (seg->flags & TCP_SYN)
                break;

            /* FIN is valid */
            if (seg->flags & TCP_FIN)
            {
                rcv->state = TIME_WAIT;
                snd->state = LAST_ACK;
            }

            snd->una = seg->seq;
            snd->nxt = seg->seq + seg->len;

            rcv->una = seg->ack;
            rcv->wnd = seg->wnd;

            acceptable = true;
            break;

        case TIME_WAIT: /* For the sake of completeness */
            break;

        case CLOSING:
            if (!TCP_SEQ_BETWEEN(seg->seq, snd->una, snd->una + snd->wnd + 1))
                break;

            if (!TCP_SEQ_BETWEEN(seg->seq + seg->len, snd->una,
                snd->una + snd->wnd + 1))
                break;

            /* Receiving a SYN inside the window is an error */
            if (seg->flags & TCP_SYN)
                break;

            /* I think the ACK needs to be exact.. maybe? */
            if (seg->flags & TCP_ACK && !TCP_SEQ_EQ(seg->ack, rcv->nxt))
                break;

            snd->una = seg->seq;
            snd->nxt = seg->seq + seg->len;

            rcv->state = TIME_WAIT;
            /* I'll be sending a packet here */
            acceptable = true;
            break;

        case CLOSE_WAIT:
            /* Remote host is not allowed to send me any more data */
            if (!TCP_SEQ_EQ(seg->seq, snd->nxt))
                break;

            /* No data damnit! This check effectively ignores a SYN as well */
            if (seg->len)
                break;

            /* Didn't send an ACK? Well fuckem */
            if (!(seg->flags & TCP_ACK) ||
                !TCP_SEQ_BETWEEN(seg->ack, rcv->una, rcv->nxt))
                break;

            rcv->una = seg->ack;
            rcv->wnd = seg->wnd;

            acceptable = true;
            break;

        case LAST_ACK:
            if (!TCP_SEQ_EQ(seg->seq, snd->nxt))
                break;

            /* I think the ACK needs to be exact.. maybe? */
            if (seg->flags & TCP_ACK && !TCP_SEQ_EQ(seg->ack, rcv->nxt))
                break;

            rcv->state = CLOSED;
            snd->state = CLOSED;

            acceptable = true;
            break;
    }

    return acceptable;
}
