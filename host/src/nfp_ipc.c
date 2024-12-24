/** Copyright (C) 2015-2016,  Gavin J Stark.  All rights reserved.
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

/*a Includes
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
#include <inttypes.h>

/*a Defines
 */

/*a Enumerations
 */
enum {
    TIMER_EXPIRED,
    TIMER_POLL
};

/*a Structures
 */
/*f struct timer */ /**
 * Timeout timer structure, used with @p timer_init and @p timer_wait
 */
struct timer {
    /** Timeout to wait for; timeouts are performed with @p usleep(),
     * and the timeout is kept as the clock_gettime seconds and
     * nsecs. If the @p tv_sec value is zero then the timeout is
     * immediate. If the @p tv_sec value is negative then the
     * timer_wait() call will never timeout. **/
    struct timespec timeout;
};

/*f struct nfp_ipc_msg_queue */ /**
 *
 *  @brief Internal structure for a message queue
 *
 * Message queues are used for to-server and to-client messaging. As
 * the IPC system is meant for limited interaction between clients and
 * servers (hopefully the clients are doing good work most of the
 * time) the message queues are of compile-time size; this also helps
 * with using a single shared memory structure for the client/server.
 *
 * Because the message queue size is a compile-time constant, the
 * message queue can be set up as a circular buffer.
 *
 */
struct nfp_ipc_msg_queue
{
    /** Pointer into msg_ofs of next message to be added to the
     * queue. Needs to be bounded to @p msg_ofs size, as it is
     * monotonically increasing. If @p write_ptr == @p read_ptr then
     * the queue is empty. The queue must never have more than @p
     * MSGS_PER_QUEUE entries, so @p write_ptr must never exceeed @p
     * read_ptr by more than @p MSGS_PER_QUEUE (when the queue would
     * be full)**/
    int write_ptr;
    /** Pointer into msg_ofs of next message to be removed from the
     * queue. Needs to be bounded to @p msg_ofs size, as it is
     * monotonically increasing. **/
    int read_ptr;
    /** Message buffer data, kept as offsets within the heap; pointers
     * are not valid across different processes, of course. **/
    int msg_ofs[MSGS_PER_QUEUE];
};

/*f struct nfp_ipc_server_data */ /**
 *
 * @brief Internal structure for the state of a server
 *
 *  This structure contains the state of the nfp_ipc server; it
 * is an internal data structure that should not be read or modified
 * by the user of the nfp_ipc functions, but it is declared here so
 * that the shared data structure (nfp_ipc) for the client/servers can
 * be defined cleanly.
 *
 * The structure contains the start of the server and bit masks used
 * for interacting with clients. This structure will be read
 * constantly by the server, and so is made to be exactly one cache
 * line long - it will reside in the L1 or L2 cache of the core
 * running the server process. When a client (infrequently) needs to
 * update the structure (for example when it adds a message for the
 * server) it has to atomically set a bit, and the client process core
 * will have to claim the cache line from the server core (forcing an
 * eviction from the L2 cache of the server core), modify it, write it
 * back to its L2, and the server will (almost immediately) import the
 * modified version back in to its L2 cache. It is critical that this
 * structure does not contain any additional information other than
 * client-to-server interactions; if it did so then the caches would
 * thrash more.
 */
struct nfp_ipc_server_data {
    /** State of the server from NFP_IPC_STATE_* **/
    int      state;
    /** Maximum number of clients that can connect to the server **/
    int      max_clients;
    /** Total number of clients currently connected to the server **/
    int      total_clients;
    /** Padding to align other fields to 8B alignment **/
    int      pad2;
    /** Client mask with one bit per potentially-active client; the
     * bottom @p max_clients bits of this mask should be set at all
     * times. **/
    uint64_t client_mask;
    /** Active client mask indicating which clients are fully active **/
    uint64_t active_client_mask;
    /** Doorbell mask with one bit per client indicating that the
     * client has added a message to its to-server message queue, or
     * that the client has changed state. This has to be atomically
     * set or cleared by the client or server - it cannot be just
     * written (it may just be read though) **/
    uint64_t doorbell_mask;
    /** Server-held mask indicating which clients have set their
     * doorbells in the recent past but have not yet been serviced **/
    uint64_t pending_mask;
    /** Padding to ensure this structure is 64B long (one cache line) **/
    char pad[16];
};

/*f struct nfp_ipc_client_data */ /**
 *
 *  @brief Internal structure for a client
 *
 *  The internal data structure for a client that is stored in the
 *  shared memory, with one instance per client.
 */
struct nfp_ipc_client_data {
    /** State of the client, e.g. inactive, active, shutting down **/
    int     state;
    /** Doorbell mask; the server atomically sets a bit in here to
     * wake client, for example when the @p to_clientq becomes
     * non-empty **/
    int     doorbell_mask;
    /** Message queue to the server from the client **/
    struct nfp_ipc_msg_queue to_serverq;
    /** Message queue to the client from the server**/
    struct nfp_ipc_msg_queue to_clientq;
};

/*f struct nfp_ipc */ /**
 *
 * @brief Structure containing all server/client data
 *
 * This structure
 */
struct nfp_ipc {
    struct nfp_ipc_server_data server;
    struct nfp_ipc_client_data clients[NFP_IPC_MAX_CLIENTS];
    struct _nfp_ipc_msg_data msg;
};

/*a Timer functions
 */
/*f clock_gettime for OSX
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

/*f timer_init */
/**
 * @brief Initialize a timeout timer
 * 
 * @param timer Timer to initialize
 *
 * @param timeout Number of microseconds to wait; if negative, wait
 * indefinitely.
 *
 * Initialize a timeout timer to a value.
 *
 * The timeout structure will be set to contain the current wall clock
 * time plus the timeout value; if the required timeout is zero, then
 * the @p tv_sec field is zeroed; if it is negative, for never
 * timeout, the @p tv_sec field is -1.
 *
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
    if (timeout<0) {
        timer->timeout.tv_sec = -1;
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

/*f timer_wait */
/**
 * @brief Wait for timeout
 *
 * @param timer Previously initialized timer (using @p timer_init)
 *
 * @returns TIMER_EXPIRED if the timer timed out, else TIMER_POLL
 *
 * Wait for a period of time (up to 10ms) up to the timeout, then
 * return. If the timer had expired before waiting, then an expiration
 * result is returned; otherwise a poll result is returned so that the
 * caller may (e.g.) poll for essages before calling @p timer_wait
 * again.
 *
 **/
static int
timer_wait(struct timer *timer)
{
    struct timespec ts;
    if (timer->timeout.tv_sec==0)
        return TIMER_EXPIRED;
    if (timer->timeout.tv_sec<0) {
        usleep(10000);
        return TIMER_POLL;
    }

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec = timer->timeout.tv_nsec - ts.tv_nsec;
    ts.tv_sec  = timer->timeout.tv_sec - ts.tv_sec;

    if (ts.tv_nsec < 0) {
        ts.tv_nsec += 1E9;
        ts.tv_sec -= 1;
    }
    if (ts.tv_sec < 0)
        return TIMER_EXPIRED;

    if ((ts.tv_sec == 0) && (ts.tv_nsec <= 0))
        return TIMER_EXPIRED;

    usleep(10000);
    return TIMER_POLL;
}
    
/*a Message queue functions
 */
/*f msg_queue_init */
/**
 * @brief Initialize a message queue
 *
 * @param msgq Message queue to initialize 
 *
 * Initialize a message queue by resetting the @p read_ptr and @p
 * write_ptr
 *
 **/
static void
msg_queue_init(struct nfp_ipc_msg_queue *msgq)
{
    msgq->read_ptr = 0;
    msgq->write_ptr = 0;
}

/*f msg_queue_empty */
/**
 * @brief Determine if a message queue is empty
 *
 * @param msgq Message queue to test
 *
 * @returns TRUE if the message queue is empty, otherwise FALSE
 *
 **/
static int
msg_queue_empty(struct nfp_ipc_msg_queue *msgq)
{
    return (msgq->write_ptr == msgq->read_ptr);
}

/*f msg_queue_full */
/**
 * @brief Determine if a message queue is full
 *
 * @param msgq Message queue to test
 *
 * @returns TRUE if the message queue is full, otherwise FALSE
 *
 **/
static int
msg_queue_full(struct nfp_ipc_msg_queue *msgq)
{
    return (msgq->write_ptr-msgq->read_ptr) >= MSGS_PER_QUEUE;
}

/*f msg_queue_get */
/**
 * @brief Get first available message from a message queue
 *
 * @param msgq Message queue to retrieve message from
 *
 * @returns -1 for no message, else offset in to message heap of
 * message
 *
 * Get a message from the front of a message queue.
 *
 **/
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

/*f msg_queue_put */
/**
 * @brief Put a message on to a message queue
 *
 * @param msgq Message queue to retrieve message from
 *
 * @param msg_ofs  Offset in to message pool of message
 *
 * @returns -1 if the queue was full, else 0
 *
 * Put a message onto a message queue.
 *
 **/
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

/*a Server client management functions
 */
/*f is_alive */
/**
 * @brief Determine if server is alive
 *
 * @param nfp_ipc NFP IPC structure for the server
 *
 * @returns TRUE if the server is alive, else FALSE
 *
 **/
static int
is_alive(struct nfp_ipc *nfp_ipc)
{
    int state;
    state = nfp_ipc->server.state;
    return ( (state==NFP_IPC_STATE_ALIVE) ||
             0 );
}

/*f find_first_set */
/**
 * @brief Find lowest bit set in a bitmask
 *
 * @param mask Bit mask to find lowest bit set
 *
 * @returns -1 if the mask is clear, else the number of the lowest set
 * bit
 *
 **/
static int
find_first_set(uint64_t mask)
{
    int n;

    if (mask == 0) return -1;
    for (n=0; (mask&1)==0; n++)
        mask >>= 1;
    return n;
}

/*f find_free_client */
/**
 * @brief Find first available client
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @returns -1 if no clients are available, else a client number
 *
 * Uses the @p client_mask ANDed with the inverse of the @p
 * active_client_mask to find inactive clients, and the first
 * available of these (if any) is returned.
 *
 **/
static int
find_free_client(struct nfp_ipc *nfp_ipc)
{
    uint64_t av_mask; /* Available clients */

    av_mask = nfp_ipc->server.client_mask;
    av_mask &= ~nfp_ipc->server.active_client_mask;
    av_mask &= -av_mask;
    return find_first_set(av_mask);
}

/*f total_clients_inc */
/**
 * @brief Increment the count of total clients
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * Increment the total number of clients attempting to use the
 * server. This number may exceed the maximum number of clients when a
 * client is attempting to be added to the server - the bitmask of
 * active clients controls the actual client connectivity to the
 * server.
 *
 * This call is invoked when a client attempts to add itself to the
 * server. A corresponding @p total_clients_dec will be invoked when
 * the client eventually shuts down (if it successfully connects to
 * the server) or when it abandons attempts to connect to the server
 * (after retrying).
 *
 **/
static void
total_clients_inc(struct nfp_ipc *nfp_ipc)
{
    volatile int *total_clients;
    int one;

    one = 1;
    total_clients = &nfp_ipc->server.total_clients;
    (void) __atomic_fetch_add(total_clients, one, __ATOMIC_ACQ_REL);
}

/*f total_clients_dec */
/**
 * @brief Decrement the count of total clients
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * Decrement the total number of clients attempting to use the
 * server. This is invoked either when a client eventually shuts down,
 * or when the client abandons attempting to connect to the server.
 *
 **/
static void
total_clients_dec(struct nfp_ipc *nfp_ipc)
{
    volatile int *total_clients;
    int minus_one;

    minus_one = ~0;
    total_clients = &nfp_ipc->server.total_clients;
    (void) __atomic_fetch_add(total_clients, minus_one, __ATOMIC_ACQ_REL);
}

/*f claim_client */
/**
 * @brief Claim a particular client
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @param client Client number to claim
 *
 * @returns -1 if the claim failed (because another client won the
 * race); on success it returns @p client
 *
 * Attempt to set the @p active_client_mask bit for the client; if it
 * was already set, then the client has already been activated from a
 * different thread or process, so the call fails.
 *
 **/
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

/*f alert_server */
/**
 * @brief Alert server from a client
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @param client Client number to alert server with
 *
 * Alert the server to do something based on an event of some sort
 * from the specified @p client.
 *
 * This call may be invoked by a client when it adds a message to the
 * server message queue for that client, for example.
 *
 **/
static void
alert_server(struct nfp_ipc *nfp_ipc, int client)
{
    volatile uint64_t *doorbell_mask;
    uint64_t client_bit;

    client_bit = 1UL << client;
    doorbell_mask = &nfp_ipc->server.doorbell_mask;
    (void) __atomic_fetch_or(doorbell_mask, client_bit, __ATOMIC_ACQ_REL);
}

/*f alert_client */
/**
 * @brief Alert client from the server
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @param client Client number to alert
 *
 * Alert a client to do something based on an event of some sort
 * from the server.
 *
 * This call may be invoked by the server when it adds a message to the
 * client message queue for that client, for example.
 *
 **/
static void
alert_client(struct nfp_ipc *nfp_ipc, int client)
{
    nfp_ipc->clients[client].doorbell_mask |= 1;
}

/*f alert_clients */
/**
 * @brief Alert all clients
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * Alert all the clients (active or not) that the server has some event for the clients to handle.
 *
 * This call is invoked, for example, when the server starts to shut down.
 *
 **/
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

/*f server_client_shutdown */
/**
 * @brief Fully shutdown a client from within the server
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @param client Client number to shut down
 *
 * Shut down the client within the server.
 *
 * The client must have previously started, and be active.
 *
 * The client is removed from the @p active_client_mask and the total
 * number of connected clients.
 *
 **/
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

/*a Message functions
 */
/*f msg_init
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

/*f msg_claim_block
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

/*f msg_release_block
 */
static void
msg_release_block(struct nfp_ipc *nfp_ipc)
{
    int *msg_locked;
    msg_locked = &nfp_ipc->msg.hdr.locked;
    (void) __atomic_fetch_and(msg_locked, ~1, __ATOMIC_ACQ_REL);
}

/*f msg_dump
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

/*f msg_check_heap
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

/*f msg_get_ofs
 */
static int
msg_get_ofs(struct nfp_ipc *nfp_ipc, struct nfp_ipc_msg *nfp_ipc_msg)
{
    //printf("Get ofset of %p.%p is %ld\n",nfp_ipc,nfp_ipc_msg,(char *)nfp_ipc_msg - (char *)&nfp_ipc->msg);
    return (char *)nfp_ipc_msg - (char *)&nfp_ipc->msg;
}

/*f msg_get_msg
 */
static struct nfp_ipc_msg *
msg_get_msg(struct nfp_ipc *nfp_ipc, int msg_ofs)
{
    //printf("Get msg of %p.%d is %p\n",nfp_ipc,msg_ofs,(struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + msg_ofs));
    return (struct nfp_ipc_msg *) ((char *)&nfp_ipc->msg + msg_ofs);
}

/*a Polling functions
 */
/*f server_poll */
/**
 *
 * @brief Server call to poll for messages, or other events
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @param timer Timeout to wait for while polling
 *
 * @param event Structure to be filled out if a valid event has
 * occurred
 *
 * @returns NFP_IPC_EVENT_* enumeration indicating that an event is
 * valid (and @p event is filled out), or that the timeout occurred
 *
 * Poll for events from clients on behalf of for the specified amount
 * of time. If a valid event occurs then @p event will be filled out;
 * otherwise (e.g. on timeouts) an appropriate return value is
 * used and @p event is not touched.
 *
 * The process is to maintain a mask of clients that have work for the
 * server to do. If the mask is empty, then it is refreshed from the
 * client doorbells, atomically clearing the doorbells at the same
 * time. If there is still no work to do, then a period of waiting (up
 * to the timer timeout) can be performed.
 *
 * If there is work to do then the first client is
 * handled. Potentially the client may still have more work for the
 * server to do, for example if its message queue contains more than
 * one message, so in some cases the client remains in the mask of
 * clients with work to do @p pending_mask.
 *
 * If a client has nothing for the server then it is removed from the
 * @p pending_mask.
 *
 **/
static int
server_poll(struct nfp_ipc *nfp_ipc, struct timer *timer, struct nfp_ipc_event *event)
{
    uint64_t client_mask;
    int client;

    for (;;) {

        /* Get the client_mask from the server data; if there are no
         * clients, then get the doorbel data and clear it
         */
        client_mask = nfp_ipc->server.pending_mask;
        if (client_mask == 0) {
            uint64_t *doorbell_mask;
            doorbell_mask = &nfp_ipc->server.doorbell_mask;
            client_mask = __atomic_fetch_and(doorbell_mask, 0, __ATOMIC_ACQ_REL);
        }
        client_mask  &= nfp_ipc->server.active_client_mask;
        nfp_ipc->server.pending_mask = client_mask;

        /* If there is nothing to do then wait for the timer, else
         * handle the client
         */
        if (client_mask == 0) {
            if (timer_wait(timer) == TIMER_EXPIRED)
                return NFP_IPC_EVENT_TIMEOUT;
        } else {
            /* Get ready to remove the client from the pending set
             */
            client = find_first_set(client_mask);
            client_mask &= ~(1ULL << client);

            if (nfp_ipc->clients[client].state == NFP_IPC_STATE_SHUTTING_DOWN) {
                /* Client is shutting down; drop its pending bit
                 */
                nfp_ipc->server.pending_mask = client_mask;
                server_client_shutdown(nfp_ipc, client);
                continue;
            } else if (!msg_queue_empty(&nfp_ipc->clients[client].to_serverq)) {
                /* Client has at least one message - KEEP its pending bit
                 */
                break;
            }
            /* Client seems to have nothing to do - drop its pending bit
             */
            nfp_ipc->server.pending_mask = client_mask;
        }
    }
    event->event_type = NFP_IPC_EVENT_MESSAGE;
    event->nfp_ipc = nfp_ipc;
    event->client = client;
    event->msg = msg_get_msg(nfp_ipc,msg_queue_get(&nfp_ipc->clients[client].to_serverq));
    return NFP_IPC_EVENT_MESSAGE;
}

/*f client_poll */
/**
 *
 * @brief Client call to poll for messages, or other events
 *
 * @param nfp_ipc Previously initialized server structure
 *
 * @param client Client number to poll for messages for
 *
 * @param timer Timeout to wait for while polling
 *
 * @param event Structure to be filled out if a valid event has
 * occurred
 *
 * @returns NFP_IPC_EVENT_* enumeration indicating that an event is
 * valid (and @p event is filled out), or that the timeout occurred
 *
 * Poll for events from the server for a client for the specified amount
 * of time. If a valid event occurs then @p event will be filled out;
 * otherwise (e.g. on timeouts) an appropriate return value is
 * used and @p event is not touched.
 *
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
            if (timer_wait(timer) == TIMER_EXPIRED)
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

/*a Public fnuctions
 */
/*f nfp_ipc_client_start
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
nfp_ipc_client_start(struct nfp_ipc *nfp_ipc, const struct nfp_ipc_client_desc *desc)
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

/*f nfp_ipc_client_stop
 *
 * Stop a client that has previously been started
 *
 * Make state to be shutting down, and alerts server
 *
 */
void
nfp_ipc_client_stop(struct nfp_ipc *nfp_ipc, int client)
{
    nfp_ipc->clients[client].state = NFP_IPC_STATE_SHUTTING_DOWN;
    alert_server(nfp_ipc, client);
    //printf("Stop:Active %016llx\n",nfp_ipc->server.active_client_mask);
}

/*f nfp_ipc_size
 */
int
nfp_ipc_size(void)
{
    return sizeof(struct nfp_ipc);
}

/*f nfp_ipc_server_init
 */
void
nfp_ipc_server_init(struct nfp_ipc *nfp_ipc, const struct nfp_ipc_server_desc *desc)
{
    int max_clients;
    int i;

    max_clients = desc->max_clients;
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

/*f nfp_ipc_server_shutdown
 */
int
nfp_ipc_server_shutdown(struct nfp_ipc *nfp_ipc, int timeout)
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

/*f nfp_ipc_msg_alloc
 */
struct nfp_ipc_msg *
nfp_ipc_msg_alloc(struct nfp_ipc *nfp_ipc, int size)
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
    byte_size = size + sizeof(struct _nfp_ipc_msg_hdr);
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

/*f nfp_ipc_msg_free
 */
void
nfp_ipc_msg_free(struct nfp_ipc *nfp_ipc, struct nfp_ipc_msg *nfp_ipc_msg)
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

/*f nfp_ipc_server_send_msg
 */
int
nfp_ipc_server_send_msg(struct nfp_ipc *nfp_ipc, int client, struct nfp_ipc_msg *msg)
{
    struct nfp_ipc_msg_queue *msgq;    
    int rc;

    msgq = &(nfp_ipc->clients[client].to_clientq);
    rc = msg_queue_put(msgq, msg_get_ofs(nfp_ipc, msg));
    //printf("Send message to client %d msg %p yields rc %d\n",client,msg,rc);
    if (rc == 0) {
        alert_client(nfp_ipc, client);
    }
    return rc;
}

/*f nfp_ipc_client_send_msg
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

/*f nfp_ipc_server_poll
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

/*f nfp_ipc_client_poll
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

