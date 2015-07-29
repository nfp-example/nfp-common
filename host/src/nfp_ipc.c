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
    nfp_ipc->clients[client].server_ticket += 1;
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
            } else {
                break;
            }
        }
    }
    event->event_type = 0;
    event->nfp_ipc = nfp_ipc;
    event->client = client;
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
    memset(nfp_ipc, 0, sizeof(*nfp_ipc));
    if (max_clients >= NFP_IPC_MAX_CLIENTS)
        max_clients = NFP_IPC_MAX_CLIENTS;
    nfp_ipc->server.max_clients = max_clients;
    nfp_ipc->server.client_mask = (2ULL<<(max_clients-1)) - 1;
    nfp_ipc->server.state = NFP_IPC_STATE_ALIVE;
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

/** nfp_ipc_poll
 */
int
nfp_ipc_poll(struct nfp_ipc *nfp_ipc, int timeout, struct nfp_ipc_event *event)
{
    struct timer timer;
    if (nfp_ipc->server.state != NFP_IPC_STATE_ALIVE)
        return -1;

    timer_init(&timer, timeout);
    return server_poll(nfp_ipc, &timer, event);
}

