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
 * @file          nfp_ipc_test.c
 * @brief         Test for NFP inter-process communication library
 *
 */

/** Includes
 */
#include <stdio.h>
#include <stdlib.h>
#include "nfp_ipc.h"

/** get_rand
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

/** test_simple
 */
static int
test_simple(int num_clients)
{
    struct nfp_ipc nfp_ipc;
    int err;
    int i;
    int clients[64];

    nfp_ipc_init(&nfp_ipc, num_clients);
    for (i=0; i<num_clients; i++) {
        clients[i] = nfp_ipc_start_client(&nfp_ipc);
        if (clients[i]<0)
            return i+100;
    }
    for (i=0; i<num_clients; i++) {
        nfp_ipc_stop_client(&nfp_ipc, clients[i]);
    }
    err = nfp_ipc_shutdown(&nfp_ipc, 1000);

    return err;
}

/** test_start_stop
 */
static int
test_start_stop(int num_clients, int iter)
{
    struct nfp_ipc nfp_ipc;
    int err;
    int i;
    int clients[64];
    struct nfp_ipc_event event;

    nfp_ipc_init(&nfp_ipc, num_clients);
    for (i=0; i<num_clients; i++) {
        clients[i] = -1;
    }
    for (; iter > 0; iter--) {
        i = get_rand(num_clients);
        if (clients[i] < 0) {
            clients[i] = nfp_ipc_start_client(&nfp_ipc);
            //printf("Started %d:%d\n",i,clients[i]);
            if (clients[i]<0) {
                for (i=0; i<num_clients; i++) {
                    printf("%d:%d ",i,clients[i]);
                }
                printf("\n");
                return 100;
            }
        } else {
            nfp_ipc_stop_client(&nfp_ipc, clients[i]);
            //printf("Stopped %d:%d\n",i,clients[i]);
            clients[i] = -1;
        }
        nfp_ipc_server_poll(&nfp_ipc, 0, &event);
    }
    for (i=0; i<num_clients; i++) {
        if (clients[i] >= 0) {
            nfp_ipc_stop_client(&nfp_ipc, clients[i]);
        }
    }
    err = nfp_ipc_shutdown(&nfp_ipc, 1000);

    return err;
}

/** test_mem_simple
 **/
static int
test_mem_simple(int iter, int max_blocks, int size_base, int size_range)
{
    int i;
    struct nfp_ipc nfp_ipc;
    struct nfp_ipc_msg *msg[64];
    int size;
    int err;

    nfp_ipc_init(&nfp_ipc, 1);

    for (i=0; i<max_blocks; i++) {
        msg[i] = NULL;
    }
    for (; iter > 0; iter--) {
        i = get_rand(max_blocks);
        if (!msg[i]) {
            size = size_base + get_rand(size_range);
            msg[i] = nfp_ipc_alloc_msg(&nfp_ipc, size);
            if (!msg[i]) {
                printf("Failed to allocate blah\n");
                return 100;
            }
        } else {
            nfp_ipc_free_msg(&nfp_ipc, msg[i]);
            msg[i] = NULL;
        }
    }

    for (i=0; i<max_blocks; i++) {
        if (msg[i]) {
            nfp_ipc_free_msg(&nfp_ipc, msg[i]);
        }
    }

    err = nfp_ipc_shutdown(&nfp_ipc, 1000);

    return err;
}

/** test_msg_simple
 **/
static int
test_msg_simple(int iter, int max_clients)
{
    int i;
    struct nfp_ipc nfp_ipc;
    struct nfp_ipc_event event;
    struct nfp_ipc_msg *msg[64];
    int size;
    int err;

    nfp_ipc_init(&nfp_ipc, max_clients);

    for (i=0; i<max_clients; i++) {
        nfp_ipc_start_client(&nfp_ipc);
        msg[i] = NULL;
    }
    for (; iter > 0; iter--) {
        i = get_rand(max_clients);
        if (!msg[i]) {
            size = 64;
            msg[i] = nfp_ipc_alloc_msg(&nfp_ipc, size);
            if (nfp_ipc_client_send_msg(&nfp_ipc, i, msg[i])!=0) {
                printf("Adding message %d did not succeed but it should (max 1 queue entry per client in this use case)\n",i);
                return 100;
            }
        } else {
            if (nfp_ipc_server_poll(&nfp_ipc, 0, &event)!=NFP_IPC_EVENT_MESSAGE) {
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
            nfp_ipc_free_msg(&nfp_ipc, msg[i]);
            msg[i] = NULL;
        }
    }

    while (nfp_ipc_server_poll(&nfp_ipc, 0, &event)==NFP_IPC_EVENT_MESSAGE) {
            i = event.client;
            if (msg[i]!=event.msg) {
                printf("Message from poll %p does not match that expected for the client %p\n",
                       event.msg,
                       msg[i] );
                return 100;
            }
            nfp_ipc_free_msg(&nfp_ipc, msg[i]);
            msg[i] = NULL;
    }
    for (i=0; i<max_clients; i++) {
        if (msg[i]) {
            nfp_ipc_free_msg(&nfp_ipc, msg[i]);
        }
        nfp_ipc_stop_client(&nfp_ipc, i);
    }

    err = nfp_ipc_shutdown(&nfp_ipc, 1000);

    return err;
}

/** TEST_RUN
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

/** main
 */
extern int
main(int argc, char **argv)
{
    TEST_RUN("Simple message test of 1 clients (1 msg per client)",test_msg_simple(150000,1));
    TEST_RUN("Simple message test of 64 client (1 msg per client)",test_msg_simple(150000,64));

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
