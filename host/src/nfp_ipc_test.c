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
 * @file          nfp_ipc_test.c
 * @brief         Test for NFP inter-process communication library
 *
 */

/*a Includes
 */
#include <stdio.h>
#include <stdlib.h>
#include "nfp_ipc.h"

/*a Useful functions
 */
/** get_rand
 * @brief Get a random number between 0 and max-1
 *
 * @param max Range of numbers to pick randomly from
 *
 * @returns Random number between 0 and @p max-1
 */
static int
get_rand(int max)
{
    long int r;
    double d;
    r = random() & ((1<<24)-1);
    d = ((double)(max)) * r;
    return (int)(d / (1<<24));
}

/*a Tests
 */
/*f test_simple */
/**
 * @brief test_simple
 *
 * @param num_clients Number of clients to use
 *
 * @returns Zero on success, else an error indications
 *
 * Start up the specified number of clients, then shut them down.
 *
 **/
static int
test_simple(int num_clients)
{
    struct nfp_ipc *nfp_ipc;
    struct nfp_ipc_server_desc server_desc;
    struct nfp_ipc_client_desc client_desc;
    int err;
    int i;
    int clients[64];

    server_desc.max_clients = num_clients;
    nfp_ipc = malloc(nfp_ipc_size());
    nfp_ipc_server_init(nfp_ipc, &server_desc);
    for (i=0; i<num_clients; i++) {
        clients[i] = nfp_ipc_client_start(nfp_ipc, &client_desc);
        if (clients[i]<0)
            return i+100;
    }
    for (i=0; i<num_clients; i++) {
        nfp_ipc_client_stop(nfp_ipc, clients[i]);
    }
    err = nfp_ipc_server_shutdown(nfp_ipc, 1000);
    free(nfp_ipc);
    return err;
}

/*f test_start_stop */
/**
 * @brief test_start_stop
 *
 * @param iter Number of iterations to run for
 *
 * @param num_clients Number of clients to use
 *
 * @returns Zero on success, else an error indications
 *
 * Randomly start and stop clients.
 *
 **/
static int
test_start_stop(int num_clients, int iter)
{
    struct nfp_ipc *nfp_ipc;
    struct nfp_ipc_server_desc server_desc;
    struct nfp_ipc_client_desc client_desc;
    int err;
    int i;
    int clients[64];
    struct nfp_ipc_event event;

    server_desc.max_clients = num_clients;
    nfp_ipc = malloc(nfp_ipc_size());
    nfp_ipc_server_init(nfp_ipc, &server_desc);
    for (i=0; i<num_clients; i++) {
        clients[i] = -1;
    }
    for (; iter > 0; iter--) {
        i = get_rand(num_clients);
        if (clients[i] < 0) {
            clients[i] = nfp_ipc_client_start(nfp_ipc, &client_desc);
            //printf("Started %d:%d\n",i,clients[i]);
            if (clients[i]<0) {
                for (i=0; i<num_clients; i++) {
                    printf("%d:%d ",i,clients[i]);
                }
                printf("\n");
                return 100;
            }
        } else {
            nfp_ipc_client_stop(nfp_ipc, clients[i]);
            //printf("Stopped %d:%d\n",i,clients[i]);
            clients[i] = -1;
        }
        nfp_ipc_server_poll(nfp_ipc, 0, &event);
    }
    for (i=0; i<num_clients; i++) {
        if (clients[i] >= 0) {
            nfp_ipc_client_stop(nfp_ipc, clients[i]);
        }
    }
    err = nfp_ipc_server_shutdown(nfp_ipc, 1000);
    free(nfp_ipc);
    return err;
}

/*f test_mem_simple */
/**
 * @brief test_msg_bounce
 *
 * @param iter Number of iterations to run for
 *
 * @param max_blocks
 *
 * @param size_base
 *
 * @param size_range
 *
 * @returns Zero on success, else an error indications
 *
 * Create a server, then allocate and free messages in the server.
 *
 * The allocations are randomly handled so that the heap will fragment
 * and require coalescing.
 *
 * At the end all the messages are freed.
 *
 * Ideally this test is run with the nfp_ipc heap checking enabled
 *
 **/
static int
test_mem_simple(int iter, int max_blocks, int size_base, int size_range)
{
    int i;
    struct nfp_ipc *nfp_ipc;
    struct nfp_ipc_server_desc server_desc;
    struct nfp_ipc_msg *msg[64];
    int size;
    int err;

    server_desc.max_clients = 1;

    nfp_ipc = malloc(nfp_ipc_size());
    nfp_ipc_server_init(nfp_ipc, &server_desc);

    for (i=0; i<max_blocks; i++) {
        msg[i] = NULL;
    }
    for (; iter > 0; iter--) {
        i = get_rand(max_blocks);
        if (!msg[i]) {
            size = size_base + get_rand(size_range);
            msg[i] = nfp_ipc_msg_alloc(nfp_ipc, size);
            if (!msg[i]) {
                printf("Failed to allocate blah\n");
                return 100;
            }
        } else {
            nfp_ipc_msg_free(nfp_ipc, msg[i]);
            msg[i] = NULL;
        }
    }

    for (i=0; i<max_blocks; i++) {
        if (msg[i]) {
            nfp_ipc_msg_free(nfp_ipc, msg[i]);
        }
    }

    err = nfp_ipc_server_shutdown(nfp_ipc, 1000);

    return err;
}

/*f test_msg_simple */
/**
 * @brief test_msg_simple
 *
 * @param iter Number of iterations to run for
 *
 * @param max_clients Maximum number of clients to use
 *
 * @returns Zero on success, else an error indications
 *
 * Start up all the clients (@p max_clients), and randomly either send
 * a message from a client or poll for a message in the server
 *
 * At most one message is sent per client
 *
 * When the server receives a message it frees it
 *
 * At the end make the server poll for all the messages that should be pending
 *
 **/
static int
test_msg_simple(int iter, int max_clients)
{
    int i;
    struct nfp_ipc *nfp_ipc;
    struct nfp_ipc_client_desc client_desc;
    struct nfp_ipc_server_desc server_desc;
    struct nfp_ipc_event event;
    struct nfp_ipc_msg *msg[64];
    int size;
    int err;

    server_desc.max_clients = max_clients;

    nfp_ipc = malloc(nfp_ipc_size());
    nfp_ipc_server_init(nfp_ipc, &server_desc);

    for (i=0; i<max_clients; i++) {
        nfp_ipc_client_start(nfp_ipc, &client_desc);
        msg[i] = NULL;
    }
    for (; iter > 0; iter--) {
        i = get_rand(max_clients);
        if (!msg[i]) {
            size = 64;
            msg[i] = nfp_ipc_msg_alloc(nfp_ipc, size);
            if (nfp_ipc_client_send_msg(nfp_ipc, i, msg[i])!=0) {
                printf("Adding message %d did not succeed but it should (max 1 queue entry per client in this use case)\n",i);
                return 100;
            }
        } else {
            if (nfp_ipc_server_poll(nfp_ipc, 0, &event)!=NFP_IPC_EVENT_MESSAGE) {
                printf("Poll of server did not yield message but one should have been waiting\n");
                return 100;
            }
            i = event.client;
            //printf("Received message %d succeeded\n",i);
            if (msg[i]!=event.msg) {
                printf("Message from poll %p does not match that expected for the client %p\n",
                       event.msg,
                       msg[i] );
                return 100;
            }
            nfp_ipc_msg_free(nfp_ipc, msg[i]);
            msg[i] = NULL;
        }
    }

    while (nfp_ipc_server_poll(nfp_ipc, 0, &event)==NFP_IPC_EVENT_MESSAGE) {
            i = event.client;
            if (msg[i]!=event.msg) {
                printf("Message from poll %p does not match that expected for the client %p\n",
                       event.msg,
                       msg[i] );
                return 100;
            }
            nfp_ipc_msg_free(nfp_ipc, msg[i]);
            msg[i] = NULL;
    }
    for (i=0; i<max_clients; i++) {
        if (msg[i]) {
            nfp_ipc_msg_free(nfp_ipc, msg[i]);
        }
        nfp_ipc_client_stop(nfp_ipc, i);
    }

    err = nfp_ipc_server_shutdown(nfp_ipc, 1000);
    free(nfp_ipc);

    return err;
}

/*f test_msg_bounce */
/**
 * @brief test_msg_bounce
 *
 * @param iter Number of iterations to run for
 *
 * @param max_clients Maximum number of clients to use for bouncing
 *
 * @returns Zero on success, else an error indications
 *
 * Start up all the clients (@p max_clients), and randomly either send
 * a message from a client, poll for a message in the server, or poll
 * for a message in the client
 *
 * At most one message is sent per client
 *
 * When the server receives a message from the client it bounces it back straight away
 *
 * At most one message will then be queued for a client
 *
 * At the end make the server poll and bounce, and then close out all clients
 *
 **/
static int
test_msg_bounce(int iter, int max_clients)
{
    int i;
    struct nfp_ipc *nfp_ipc;
    struct nfp_ipc_client_desc client_desc;
    struct nfp_ipc_server_desc server_desc;
    struct nfp_ipc_event event;
    struct nfp_ipc_msg *msg[64];
    int msg_state[64];
    int size;
    int err;

    server_desc.max_clients = max_clients;

    nfp_ipc = malloc(nfp_ipc_size());
    nfp_ipc_server_init(nfp_ipc, &server_desc);

    for (i=0; i<max_clients; i++) {
        nfp_ipc_client_start(nfp_ipc, &client_desc);
        msg[i] = NULL;
        msg_state[i] = 0;
    }
    for (; iter > 0; iter--) {
        i = get_rand(max_clients);
        if (msg_state[i]==0) {
            size = 64;
            msg[i] = nfp_ipc_msg_alloc(nfp_ipc, size);
            if (nfp_ipc_client_send_msg(nfp_ipc, i, msg[i])!=0) {
                printf("Adding message %d did not succeed but it should (max 1 queue entry per client in this use case)\n",i);
                return 100;
            }
            msg_state[i] = 1;
        } else if (msg_state[i]==1) {
            if (nfp_ipc_server_poll(nfp_ipc, 0, &event)!=NFP_IPC_EVENT_MESSAGE) {
                printf("Poll of server did not yield message but one should have been waiting\n");
                return 100;
            }
            i = event.client;
            if (msg[i]!=event.msg) {
                printf("Message from poll %p does not match that expected for the client %p\n",
                       event.msg,
                       msg[i] );
                return 100;
            }
            if (nfp_ipc_server_send_msg(nfp_ipc, i, msg[i])!=0) {
                printf("Adding message %d to client did not succeed but it should (max 1 queue entry per client in this use case)\n",i);
                return 100;
            }
            msg_state[i] = 2;
        } else {
            if (nfp_ipc_client_poll(nfp_ipc, i, 0, &event)!=NFP_IPC_EVENT_MESSAGE) {
                printf("Poll of client did not yield message but one should have been waiting\n");
                return 100;
            }
            if (msg[i]!=event.msg) {
                printf("Message from poll %p does not match that expected for the client %p\n",
                       event.msg,
                       msg[i] );
                return 100;
            }
            nfp_ipc_msg_free(nfp_ipc, msg[i]);
            msg[i] = NULL;
            msg_state[i] = 0;
        }
    }

    while (nfp_ipc_server_poll(nfp_ipc, 0, &event)==NFP_IPC_EVENT_MESSAGE) {
            i = event.client;
            if (msg[i]!=event.msg) {
                printf("Message from poll %p does not match that expected for the client %p\n",
                       event.msg,
                       msg[i] );
                return 100;
            }
            // check msg_state[i]==1
            nfp_ipc_msg_free(nfp_ipc, msg[i]);
            msg[i] = NULL;
    }
    for (i=0; i<max_clients; i++) {
        if (msg[i]) {
            if (msg_state[i]!=2) {
                printf("Message state expected to be 2 if msg[i] exists and not for server\n");
                return 100;
            }
            nfp_ipc_msg_free(nfp_ipc, msg[i]);
        }
        nfp_ipc_client_stop(nfp_ipc, i);
    }

    err = nfp_ipc_server_shutdown(nfp_ipc, 1000);
    free(nfp_ipc);

    return err;
}

/*a Toplevel - main
 */
/*f TEST_RUN */
/**
 * @brief Run a test and display a pass/fail message
 *
 * @param msg Message describing the test for (success or failure)
 *
 * @param x Test function to run for the test
 *
 */
#define TEST_RUN(msg,x)                    \
    do { \
    int err = x; \
    if (err!=0) { \
    fprintf(stderr,"TEST FAILED (%d): %s\n",err,msg); \
    } else {                                   \
    fprintf(stderr,"Test passed: %s\n",msg); \
    } \
    } while (0);

/*f main */
/**
 * @brief Main function - run the required tests
 *
 */
extern int
main(int argc, char **argv)
{
    TEST_RUN("Simple message test of 1 clients (1 msg per client)",test_msg_simple(150000,1));
    TEST_RUN("Simple message test of 64 client (1 msg per client)",test_msg_simple(150000,64));

    TEST_RUN("Bounce message test of 1 clients (1 msg per client)",test_msg_bounce(150000,1));
    TEST_RUN("Bounce message test of 1 clients (1 msg per client)",test_msg_bounce(150000,64));

    TEST_RUN("Simple memory test ",test_mem_simple(10000,64,16,0));
    TEST_RUN("Simple memory test of different sizes ",test_mem_simple(150000,64,16,128));
    TEST_RUN("Simple memory test of different sizes 2 ",test_mem_simple(150000,64,16,48));

    TEST_RUN("Simple test with 1 client",test_simple(1));
    TEST_RUN("Simple test with 8 clients",test_simple(8));
    TEST_RUN("Simple test with 64 clients",test_simple(64));

    TEST_RUN("Start/stop test with 1 client",test_start_stop(1,1000));
    TEST_RUN("Start/stop test with 8 clients",test_start_stop(8,10000));
    TEST_RUN("Start/stop test with 64 clients",test_start_stop(64,10000));
    return 0;
}
