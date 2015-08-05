/** Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file          nfp_ipc.c
 * @brief         NFP inter-process communication library
 *
 */

/** Includes
 */
#include "nfp_ipc.h"
#include <stdint.h> 
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h> 
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

/** struct timer
 */
struct timer {
    struct timespec timeout;
};

/** clock_gettime for OSX
 */
#ifdef __MACH__
#define CLOCK_REALTIME 0
static void
clock_gettime(int clk, struct timespec *ts)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec*1000L;
}
#endif

/** timer_init
 */
static void
timer_init(struct timer *timer, long timeout)
{
    long nsec;
    long sec;
    if (timeout==0) {
        timer->timeout.tv_sec = 0;
        return;
    }
    sec = timeout / 1000000;
    nsec = (timeout % 1000000)*1000;
    clock_gettime(CLOCK_REALTIME, &timer->timeout);
    timer->timeout.tv_nsec += nsec;
    timer->timeout.tv_sec  += sec;
    if (timer->timeout.tv_nsec > 1E9) {
        timer->timeout.tv_nsec -= 1E9;
        timer->timeout.tv_sec  += 1;
    }
}

/** timer_wait
 */
static int
timer_wait(struct timer *timer)
{
    struct timespec ts;
    if (timer->timeout.tv_sec==0)
        return NFP_IPC_EVENT_TIMEOUT;

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec = timer->timeout.tv_nsec - ts.tv_nsec;
    ts.tv_sec  = timer->timeout.tv_sec - ts.tv_sec;

    if (ts.tv_nsec < 0) {
        ts.tv_nsec += 1E9;
        ts.tv_sec -= 1;
    }
    if (ts.tv_sec < 0)
        return NFP_IPC_EVENT_TIMEOUT;

    if ((ts.tv_sec == 0) && (ts.tv_nsec <= 0))
        return NFP_IPC_EVENT_TIMEOUT;

    usleep(10000);
    return 0;
}
    
/** msg_queue_init
 */
static void
msg_queue_init(struct nfp_ipc_msg_queue *msgq)
{
    msgq->read_ptr = 0;
    msgq->write_ptr = 0;
}

/** msg_queue_empty
 */
static int
msg_queue_empty(struct nfp_ipc_msg_queue *msgq)
{
    return (msgq->write_ptr == msgq->read_ptr);
}

/** msg_queue_full
 */
static int
msg_queue_full(struct nfp_ipc_msg_queue *msgq)
{
    return (msgq->write_ptr-msgq->read_ptr) >= MSGS_PER_QUEUE;
}

/** msg_queue_get
 */
static int
msg_queue_get(struct nfp_ipc_msg_queue *msgq)
{
    int ptr;

    if (msg_queue_empty(msgq))
        return -1;
    ptr = (msgq->read_ptr % MSGS_PER_QUEUE);
    msgq->read_ptr++;
    return msgq->msg_ofs[ptr];
}

/** msg_queue_put
 *
 * Put a message on a queue
 *
 * @param msgq     Message queue to add message to
 * @param msg_ofs  Offset in to message pool of message
 *
 */
static int
msg_queue_put(struct nfp_ipc_msg_queue *msgq, int msg_ofs)
{
    int ptr;

    if (msg_queue_full(msgq))
        return -1;
    ptr = (msgq->write_ptr % MSGS_PER_QUEUE);
    msgq->write_ptr++;
    msgq->msg_ofs[ptr] = msg_ofs;
    return 0;
}

/** is_alive
 */
static int
is_alive(struct nfp_ipc *nfp_ipc)
{
    int state;
    state = nfp_ipc->server.state;
    return ( (state==NFP_IPC_STATE_ALIVE) ||
             0 );
}

/** find_first_set
 */
static int
find_first_set(uint64_t mask)
{
    int n;

    if (mask == 0) return -1;
    for (n=0; (mask&1)==0; n++)
        mask >>= 1;
    return n;
}

/** find_free_client
 */
static int
find_free_client(struct nfp_ipc *nfp_ipc)
{
    uint64_t av_mask; /* Available clients */

    av_mask = nfp_ipc->server.client_mask;
    av_mask &= ~nfp_ipc->server.active_client_mask;
    av_mask &= -av_mask;
    return find_first_set(av_mask);
}

/** total_clients_inc
 */
static void
total_clients_inc(struct nfp_ipc *nfp_ipc)
{
    volatile int *total_clients;
    int one;

    one = 1;
    total_clients = &nfp_ipc->server.total_clients;
    (void) __atomic_fetch_add(total_clients, one, __ATOMIC_ACQ_REL);
}

/** total_clients_dec
 */
static void
total_clients_dec(struct nfp_ipc *nfp_ipc)
{
    volatile int *total_clients;
    int minus_one;

    minus_one = ~0;
    total_clients = &nfp_ipc->server.total_clients;
    (void) __atomic_fetch_add(total_clients, minus_one, __ATOMIC_ACQ_REL);
}

/** claim_client
 */
static int
claim_client(struct nfp_ipc *nfp_ipc, int client)
{
    volatile uint64_t *active_client_mask;
    uint64_t client_bit;
    uint64_t preclaim_mask;

    client_bit = 1UL << client;
    active_client_mask = &nfp_ipc->server.active_client_mask;
    preclaim_mask = __atomic_fetch_or(active_client_mask, client_bit, __ATOMIC_ACQ_REL);

    if (preclaim_mask & client_bit)
        return -1;
    return client;
}

/** alert_server
 */
static void
alert_server(struct nfp_ipc *nfp_ipc, int client)
{
    volatile uint64_t *doorbell_mask;
    uint64_t client_bit;

    client_bit = 1UL << client;
    doorbell_mask = &nfp_ipc->server.doorbell_mask;
    (void) __atomic_fetch_or(doorbell_mask, client_bit, __ATOMIC_ACQ_REL);
}

/** alert_client
 */
static void
alert_client(struct nfp_ipc *nfp_ipc, int client)
{
    nfp_ipc->clients[client].doorbell_mask |= 1;
}

/** alert_clients
 */
static void
alert_clients(struct nfp_ipc *nfp_ipc, uint64_t client_mask)
{
    int n;
    for (n=0; n<nfp_ipc->server.max_clients; n++) {
        if (client_mask & (1 << n)) {
            alert_client(nfp_ipc, n);
        }
    }
}

/** server_client_shutdown
 *
 * Fully shutdown a client from within the server - the client has already stopped using the API
 *
 */
static void
server_client_shutdown(struct nfp_ipc *nfp_ipc, int client)
{
    volatile uint64_t *active_client_mask;
    uint64_t client_mask;

    nfp_ipc->clients[client].state = NFP_IPC_STATE_INIT;
    total_clients_dec(nfp_ipc);

    client_mask = ~(1UL << client);
    active_client_mask = &nfp_ipc->server.active_client_mask;
    (void) __atomic_fetch_and(active_client_mask, client_mask, __ATOMIC_ACQ_REL);
}

/** msg_init
 */
static void
msg_init(struct nfp_ipc *nfp_ipc)
{
    int msg_ofs;
    struct nfp_ipc_msg *msg;
    
    msg_ofs = (char *)nfp_ipc->msg.data - (char *)&nfp_ipc->msg;
    nfp_ipc->msg.hdr.free_list = msg_ofs;

    msg = (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + msg_ofs);
    msg->hdr.next_in_list = 0;
    msg->hdr.prev_in_list = 0;
    msg->hdr.next_free = 0;
    msg->hdr.byte_size = sizeof(nfp_ipc->msg.data);
}

/** msg_claim_block
 */
static int
msg_claim_block(struct nfp_ipc *nfp_ipc)
{
    int i;

    i=0;
    for (;;) {
        int locked;
        int *msg_locked;
        msg_locked = &nfp_ipc->msg.hdr.locked;
        locked = __atomic_fetch_or(msg_locked, 1, __ATOMIC_ACQ_REL);
        if (!locked)
            break;
        usleep(10000);
        if (i>100)
            return -1;
    }
    return 0;
}

/** msg_release_block
 */
static void
msg_release_block(struct nfp_ipc *nfp_ipc)
{
    int *msg_locked;
    msg_locked = &nfp_ipc->msg.hdr.locked;
    (void) __atomic_fetch_and(msg_locked, ~1, __ATOMIC_ACQ_REL);
}

/** msg_dump
 */
static void
msg_dump(struct nfp_ipc *nfp_ipc)
{
    int msg_ofs;
    int blk;
    struct nfp_ipc_msg *msg;

    if (msg_claim_block(nfp_ipc) != 0) {
        return;
    }
    printf("msg_dump %p : %6d\n",nfp_ipc, nfp_ipc->msg.hdr.free_list );
    msg_ofs = (char *)nfp_ipc->msg.data - (char *)&nfp_ipc->msg;
    blk = 0;
    while (msg_ofs != 0) {
        msg = (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + msg_ofs);
        printf("%4d @ %6d of %6dB (>%6d <%6d next_free %6d)\n",
               blk,
               msg_ofs,
               msg->hdr.byte_size,
               msg->hdr.next_in_list,
               msg->hdr.prev_in_list,
               msg->hdr.next_free);
        msg_ofs = msg->hdr.next_in_list;
        blk++;
    }
    msg_release_block(nfp_ipc);
}

/** msg_check_heap
 */
static int
msg_check_heap(struct nfp_ipc *nfp_ipc)
{
    int msg_ofs;
    int prev_ofs;
    int total_size;
    int free_list_found;
    int prev_free;
    struct nfp_ipc_msg *msg;
    int total_errors;

    if (msg_claim_block(nfp_ipc) != 0) {
        return -1;
    }

    total_errors = 0;
    free_list_found = 0;
    total_size = 0;
    prev_ofs = 0;
    prev_free = 0;
    msg_ofs = (char *)nfp_ipc->msg.data - (char *)&nfp_ipc->msg;
    while (msg_ofs != 0) {
        msg = (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + msg_ofs);
        if (nfp_ipc->msg.hdr.free_list == msg_ofs) {
            free_list_found = 1;
        }
        if (msg->hdr.prev_in_list != prev_ofs) {
            printf("Block at %6d has incorrect previous of %6d instead of %6d\n",
                   msg_ofs, msg->hdr.prev_in_list, prev_ofs);
            total_errors++;
        }
        if ((msg->hdr.next_in_list != 0) && (msg->hdr.next_in_list != msg_ofs + msg->hdr.byte_size)) {
            printf("Block at %6d has mismatching next (diff of %6d) and byte size %6d\n",
                   msg_ofs, msg->hdr.next_in_list - msg_ofs, msg->hdr.byte_size);
            total_errors++;
        }
        if (prev_ofs != 0) {
            if (prev_free && (msg->hdr.next_free != -1)) {
                printf("Successive blocks at %6d and %d are both free\n",
                       prev_ofs, msg_ofs);
                total_errors++;
            }
            prev_free = (msg->hdr.next_free != -1);
        }
        total_size += msg->hdr.byte_size;
        prev_ofs = msg_ofs;
        msg_ofs = msg->hdr.next_in_list;
    }

    msg_ofs = nfp_ipc->msg.hdr.free_list;
    if (msg_ofs < 0) {
            printf("Bad free list chain %6d\n",
                   msg_ofs);
            msg_ofs = 0;
    }
    while (msg_ofs != 0) {
        msg = (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + msg_ofs);
        if (msg->hdr.next_free < 0) {
            printf("Bad free list chain at %6d (next of %6d)\n",
                   msg_ofs, msg->hdr.next_free);
            total_errors++;
            msg_ofs = 0;
        } else {
            msg_ofs = msg->hdr.next_free;
        }
    }

    if (!free_list_found && (nfp_ipc->msg.hdr.free_list != 0)) {
        printf("Failed to find start of free list %6d\n",
               nfp_ipc->msg.hdr.free_list);
        total_errors++;
    }
    
    msg_release_block(nfp_ipc);

    if (total_errors>=0) {
        msg_dump(nfp_ipc);
    }
    return total_errors;
}

/** msg_get_ofs
 */
static int
msg_get_ofs(struct nfp_ipc *nfp_ipc, struct nfp_ipc_msg *nfp_ipc_msg)
{
    //printf("Get ofset of %p.%p is %ld\n",nfp_ipc,nfp_ipc_msg,(char *)nfp_ipc_msg - (char *)&nfp_ipc->msg);
    return (char *)nfp_ipc_msg - (char *)&nfp_ipc->msg;
}

/** msg_get_msg
 */
static struct nfp_ipc_msg *
msg_get_msg(struct nfp_ipc *nfp_ipc, int msg_ofs)
{
    //printf("Get msg of %p.%d is %p\n",nfp_ipc,msg_ofs,(struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + msg_ofs));
    return (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + msg_ofs);
}

/** server_poll
 */
static int
server_poll(struct nfp_ipc *nfp_ipc, struct timer *timer, struct nfp_ipc_event *event)
{
    uint64_t client_mask;
    int client;

    //printf("Poll:Active %016llx\n",nfp_ipc->server.active_client_mask);
    //printf("Poll:Doorbell %016llx\n",nfp_ipc->server.doorbell_mask);
    //printf("Poll:Pending %016llx\n",nfp_ipc->server.pending_mask);
    for (;;) {

        client_mask = nfp_ipc->server.pending_mask;
        if (client_mask == 0) {
            uint64_t *doorbell_mask;
            doorbell_mask = &nfp_ipc->server.doorbell_mask;
            client_mask = __atomic_fetch_and(doorbell_mask, 0, __ATOMIC_ACQ_REL);
        }
        client_mask  &= nfp_ipc->server.active_client_mask;
        nfp_ipc->server.pending_mask = client_mask;
        if (client_mask == 0) {
            if (timer_wait(timer) == NFP_IPC_EVENT_TIMEOUT)
                return NFP_IPC_EVENT_TIMEOUT;
        } else {
            client = find_first_set(client_mask);
            client_mask &= ~(1ULL << client);
            nfp_ipc->server.pending_mask = client_mask;

            if (nfp_ipc->clients[client].state == NFP_IPC_STATE_SHUTTING_DOWN) {
                server_client_shutdown(nfp_ipc, client);
                continue;
            } else if (!msg_queue_empty(&nfp_ipc->clients[client].to_serverq)) {
                break;
            }
        }
    }
    event->event_type = NFP_IPC_EVENT_MESSAGE;
    event->nfp_ipc = nfp_ipc;
    event->client = client;
    event->msg = msg_get_msg(nfp_ipc,msg_queue_get(&nfp_ipc->clients[client].to_serverq));
    return NFP_IPC_EVENT_MESSAGE;
}

/** client_poll
 */
static int
client_poll(struct nfp_ipc *nfp_ipc, int client, struct timer *timer, struct nfp_ipc_event *event)
{
    for (;;) {
        if (nfp_ipc->server.state != NFP_IPC_STATE_ALIVE)
            return NFP_IPC_EVENT_SHUTDOWN;

        if (nfp_ipc->clients[client].state != NFP_IPC_STATE_ALIVE)
            return NFP_IPC_EVENT_SHUTDOWN;

        if (nfp_ipc->clients[client].doorbell_mask==0) {
            if (timer_wait(timer) == NFP_IPC_EVENT_TIMEOUT)
                return NFP_IPC_EVENT_TIMEOUT;
        } else {
            nfp_ipc->clients[client].doorbell_mask = 0;
            if (!msg_queue_empty(&nfp_ipc->clients[client].to_clientq)) {
                nfp_ipc->clients[client].doorbell_mask = 1;
                break;
            }
        }
    }
    event->event_type = NFP_IPC_EVENT_MESSAGE;
    event->nfp_ipc = nfp_ipc;
    event->client = client;
    event->msg = msg_get_msg(nfp_ipc,msg_queue_get(&nfp_ipc->clients[client].to_clientq));
    return NFP_IPC_EVENT_MESSAGE;
}

/** nfp_ipc_start_client
 *
 * Start a new client
 *
 * If added client 'n', then active_client_mask will now have bit 'n'
 * set, and total_clients will be incremented
 *
 * The approach is to select a client that is not active, and try to
 * claim that. If that fails, try again
 *
 * If all clients are busy, of the nfp_ipc is shutting down, then fail.
 *
 */
int
nfp_ipc_start_client(struct nfp_ipc *nfp_ipc)
{
    int client;

    //printf("Start:Active %016llx\n",nfp_ipc->server.active_client_mask);
    for (;;) {
        if (!is_alive(nfp_ipc))
            return -1;

        total_clients_inc(nfp_ipc);
        client = find_free_client(nfp_ipc);
        if (client < 0)
        {
            total_clients_dec(nfp_ipc);
            return -1;
        }

        client = claim_client(nfp_ipc, client);
        if (client >= 0)
            break;
        total_clients_dec(nfp_ipc);
    }
    memset(&nfp_ipc->clients[client], 0, sizeof(nfp_ipc->clients[0]));
    nfp_ipc->clients[client].state = NFP_IPC_STATE_ALIVE;
    return client;
}

/** nfp_ipc_stop_client
 *
 * Stop a client that has previously been started
 *
 * Make state to be shutting down, and alerts server
 *
 */
void
nfp_ipc_stop_client(struct nfp_ipc *nfp_ipc, int client)
{
    nfp_ipc->clients[client].state = NFP_IPC_STATE_SHUTTING_DOWN;
    alert_server(nfp_ipc, client);
    //printf("Stop:Active %016llx\n",nfp_ipc->server.active_client_mask);
}

/** nfp_ipc_init
 */
void
nfp_ipc_init(struct nfp_ipc *nfp_ipc, int max_clients)
{
    int i;

    memset(nfp_ipc, 0, sizeof(*nfp_ipc));
    if (max_clients >= NFP_IPC_MAX_CLIENTS)
        max_clients = NFP_IPC_MAX_CLIENTS;
    nfp_ipc->server.max_clients = max_clients;
    nfp_ipc->server.client_mask = (2ULL<<(max_clients-1)) - 1;
    nfp_ipc->server.state = NFP_IPC_STATE_ALIVE;

    for (i=0; i<max_clients; i++) {
        nfp_ipc->clients[i].state = NFP_IPC_STATE_INIT;
        nfp_ipc->clients[i].doorbell_mask = 0;
        msg_queue_init(&nfp_ipc->clients[i].to_clientq);
        msg_queue_init(&nfp_ipc->clients[i].to_serverq);
    }
    msg_init(nfp_ipc);
}

/** nfp_ipc_shutdown
 */
int
nfp_ipc_shutdown(struct nfp_ipc *nfp_ipc, int timeout)
{
    struct timer timer;
    struct nfp_ipc_event event;
    int rc;

    if (nfp_ipc->server.state != NFP_IPC_STATE_ALIVE)
        return -1;

    nfp_ipc->server.state = NFP_IPC_STATE_SHUTTING_DOWN;
    timer_init(&timer, timeout);
    for (;;) {
        alert_clients(nfp_ipc, nfp_ipc->server.active_client_mask);
        if (nfp_ipc->server.total_clients == 0)
        {
            rc = 0;
            break;
        }
        if (server_poll(nfp_ipc, &timer, &event) == NFP_IPC_EVENT_TIMEOUT)
        {
            rc = 0;
            if (nfp_ipc->server.total_clients != 0) {
                rc = 1;
            }
            break;
        }
    }
    nfp_ipc->server.state = NFP_IPC_STATE_DEAD;
    return rc;
}

/** nfp_ipc_alloc_msg
 */
struct nfp_ipc_msg *
nfp_ipc_alloc_msg(struct nfp_ipc *nfp_ipc, int size)
{
    struct nfp_ipc_msg *msg;
    int byte_size;
    int prev_ofs;
    int msg_ofs;

    if (0) {
        printf("alloc %d\n",size);
        msg_check_heap(nfp_ipc);
    }

    if (msg_claim_block(nfp_ipc) != 0) {
        return NULL;
    }

    prev_ofs = 0;
    msg_ofs = nfp_ipc->msg.hdr.free_list;
    byte_size = size + sizeof(struct nfp_ipc_msg_hdr);
    byte_size = (byte_size + 7) & ~7;

    for (;;) {
        if (msg_ofs == 0) {
            msg_release_block(nfp_ipc);
            return NULL;
        }
        msg = (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + msg_ofs);
        if (msg->hdr.byte_size >= byte_size) 
            break;
        prev_ofs = msg_ofs;
        msg_ofs = msg->hdr.next_free;
    }

    // printf("Allocating %d\n",msg_ofs);
    msg = (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + msg_ofs);
    if (msg->hdr.byte_size <= byte_size+32) {
        if (prev_ofs == 0) {
            nfp_ipc->msg.hdr.free_list = msg->hdr.next_free;
        } else {
            struct nfp_ipc_msg *prev_msg;
            prev_msg = (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + prev_ofs);
            prev_msg->hdr.next_free = msg->hdr.next_free;
        }
        msg->hdr.next_free = 0;
    } else {
        int new_msg_ofs;
        struct nfp_ipc_msg *new_msg;
        // Split into two - reduce size
        msg->hdr.byte_size -= byte_size;
        new_msg_ofs = msg_ofs + msg->hdr.byte_size;
        new_msg = (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + new_msg_ofs);
        new_msg->hdr.next_in_list = msg->hdr.next_in_list;
        new_msg->hdr.prev_in_list = msg_ofs;
        new_msg->hdr.byte_size = byte_size;
        new_msg->hdr.next_free = 0;
        msg->hdr.next_in_list = new_msg_ofs;
        if (new_msg->hdr.next_in_list != 0) {
            msg = (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + new_msg->hdr.next_in_list);
            msg->hdr.prev_in_list = new_msg_ofs;
        }
        msg = new_msg;
    }

    msg->hdr.next_free = -1;

    msg_release_block(nfp_ipc);

    if (0) msg_check_heap(nfp_ipc);

    return msg;
}

/** nfp_ipc_free_msg
 */
void
nfp_ipc_free_msg(struct nfp_ipc *nfp_ipc, struct nfp_ipc_msg *nfp_ipc_msg)
{
    int msg_ofs;
    int prev_ofs;
    int next_ofs;

    if (0) {
        printf("free %p\n",nfp_ipc_msg);
        msg_check_heap(nfp_ipc);
    }

    if (msg_claim_block(nfp_ipc) != 0) {
        return;
    }

    msg_ofs = (char *)nfp_ipc_msg - (char *)&nfp_ipc->msg;
    if (0) {
        printf("free %d\n",msg_ofs);
    }
    nfp_ipc_msg->hdr.next_free = 0;

    prev_ofs = nfp_ipc_msg->hdr.prev_in_list;
    if (prev_ofs) {
        struct nfp_ipc_msg *prev_msg;
        prev_msg = (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + prev_ofs);
        if (prev_msg->hdr.next_free == -1) {
            // Previous block is allocated, so chase back to find last free block
            int blah_ofs;
            struct nfp_ipc_msg *blah_msg;
            blah_ofs = prev_msg->hdr.prev_in_list;
            while (blah_ofs != 0) {
                blah_msg = (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + blah_ofs);
                if (blah_msg->hdr.next_free != -1)
                    break;
                blah_ofs = blah_msg->hdr.prev_in_list;
            }
            if (blah_ofs != 0) {
                blah_msg->hdr.next_free = msg_ofs;
            } else {
                nfp_ipc->msg.hdr.free_list = msg_ofs;
            }
        } else {
            // Previous block is free so amalgamate
            prev_msg->hdr.next_in_list = nfp_ipc_msg->hdr.next_in_list;
            prev_msg->hdr.byte_size    = prev_msg->hdr.byte_size + nfp_ipc_msg->hdr.byte_size;
            msg_ofs = prev_ofs;
            nfp_ipc_msg = prev_msg;
        }
    } else {
        nfp_ipc_msg->hdr.next_free = nfp_ipc->msg.hdr.free_list;
        nfp_ipc->msg.hdr.free_list = msg_ofs;
    }

    next_ofs = nfp_ipc_msg->hdr.next_in_list;
    if (next_ofs) {
        struct nfp_ipc_msg *next_msg;
        next_msg = (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + next_ofs);
        if (next_msg->hdr.next_free == -1) {
            next_msg->hdr.prev_in_list = msg_ofs;
            int blah_ofs;
            struct nfp_ipc_msg *blah_msg;
            // Next block is allocated, so chase forward to find next free block if needed
            if (nfp_ipc_msg->hdr.next_free == 0) {
                blah_ofs = nfp_ipc_msg->hdr.next_in_list;
                while (blah_ofs != 0) {
                    blah_msg = (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + blah_ofs);
                    if (blah_msg->hdr.next_free != -1)
                        break;
                    blah_ofs = blah_msg->hdr.next_in_list;
                }
                nfp_ipc_msg->hdr.next_free = blah_ofs;
            }
        } else {
            nfp_ipc_msg->hdr.next_in_list = next_msg->hdr.next_in_list;
            nfp_ipc_msg->hdr.next_free    = next_msg->hdr.next_free;
            nfp_ipc_msg->hdr.byte_size    = nfp_ipc_msg->hdr.byte_size + next_msg->hdr.byte_size;
        }
    }

    next_ofs = nfp_ipc_msg->hdr.next_in_list;
    if (next_ofs) {
        struct nfp_ipc_msg *next_msg;
        next_msg = (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + next_ofs);
        next_msg->hdr.prev_in_list = msg_ofs;
    }
    msg_release_block(nfp_ipc);

    if (0) msg_check_heap(nfp_ipc);
}

/** nfp_ipc_server_send_msg
 */
int
nfp_ipc_server_send_msg(struct nfp_ipc *nfp_ipc, int client, struct nfp_ipc_msg *msg)
{
    struct nfp_ipc_msg_queue *msgq;    
    int rc;

    msgq = &(nfp_ipc->clients[client].to_clientq);
    rc = msg_queue_put(msgq, msg_get_ofs(nfp_ipc, msg));
    if (rc == 0) {
        alert_server(nfp_ipc, client);
    }
    return rc;
}

/** nfp_ipc_client_send_msg
 */
int
nfp_ipc_client_send_msg(struct nfp_ipc *nfp_ipc, int client, struct nfp_ipc_msg *msg)
{
    struct nfp_ipc_msg_queue *msgq;
    int rc;
    
    msgq = &(nfp_ipc->clients[client].to_serverq);
    rc = msg_queue_put(msgq, msg_get_ofs(nfp_ipc, msg));
    if (rc == 0) {
        alert_server(nfp_ipc, client);
    }
    return rc;
}

/** nfp_ipc_server_poll
 */
int
nfp_ipc_server_poll(struct nfp_ipc *nfp_ipc, int timeout, struct nfp_ipc_event *event)
{
    struct timer timer;
    if (nfp_ipc->server.state != NFP_IPC_STATE_ALIVE)
        return NFP_IPC_EVENT_SHUTDOWN;

    timer_init(&timer, timeout);
    return server_poll(nfp_ipc, &timer, event);
}

/** nfp_ipc_client_poll
 */
int
nfp_ipc_client_poll(struct nfp_ipc *nfp_ipc, int client, int timeout, struct nfp_ipc_event *event)
{
    struct timer timer;
    if (nfp_ipc->server.state != NFP_IPC_STATE_ALIVE)
        return NFP_IPC_EVENT_SHUTDOWN;

    timer_init(&timer, timeout);
    return client_poll(nfp_ipc, client, &timer, event);
}

