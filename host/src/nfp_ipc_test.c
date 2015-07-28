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

#define TEST_RUN(msg,x)                    \
    do { \
    int err = x; \
    if (err!=0) { \
    fprintf(stderr,"TEST FAILED (%d): %s\n",err,msg); \
    } else {                                   \
    fprintf(stderr,"Test passed: %s\n",msg); \
    } \
    } while (0);
extern int
main(int argc, char **argv)
{
    TEST_RUN("Simple test with 1 client",test_simple(1));
    TEST_RUN("Simple test with 8 clients",test_simple(8));
    TEST_RUN("Simple test with 64 clients",test_simple(64));
    return 0;
}
