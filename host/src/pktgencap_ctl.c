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
 * @file          pktgencap_ctl.c
 * @brief         Control test of packet generator/capture
 *
 */

/** Includes
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> 
#include <string.h> 
#include <inttypes.h>
#include "nfp_support.h"
#include "nfp_ipc.h"
#include "pktgencap.h"

/** Defines
 */

/** struct pktgen_nfp
 */
struct pktgen_nfp {
    struct nfp *nfp;
    struct {
        /** base **/
        char *base;
        /** size **/
        size_t size;
        /** nfp_ipc **/
        struct nfp_ipc *nfp_ipc;
    } shm;
};

/** Static variables
 */
static const char *shm_filename="/tmp/nfp_shm.lock";
static int shm_key = 'x';

/** usage
 */
static void
usage(void)
{
    printf("Usage: pktgencap_ctl <cmd>*, where cmd is one of:\n"
           "    shutdown    shut down the pktgencap main process\n"
           "    pktdump     dump packets received to stdout\n"
        );
}

/** pktgen_alloc_shm
 */
static int
pktgen_alloc_shm(struct pktgen_nfp *pktgen_nfp)
{
    pktgen_nfp->shm.size = nfp_shm_alloc(pktgen_nfp->nfp,
                                         shm_filename, shm_key,
                                         pktgen_nfp->shm.size, 0);
    if (pktgen_nfp->shm.size == 0) {
        fprintf(stderr,"Failed to find NFP SHM\n");
        return -1;
    }

    pktgen_nfp->shm.base = nfp_shm_data(pktgen_nfp->nfp);
    pktgen_nfp->shm.nfp_ipc = (struct nfp_ipc *)pktgen_nfp->shm.base;
    return 0;
}

/** Main
 */
#include "firmware/pcap.h"
extern int
main(int argc, char **argv)
{
    struct pktgen_nfp pktgen_nfp;
    struct nfp_ipc_client_desc nfp_ipc_client_desc;
    int nfp_ipc_client;
    int i;

    pktgen_nfp.nfp = nfp_init(-1);

    if (pktgen_alloc_shm(&pktgen_nfp)<0) {
        fprintf(stderr, "Failed to find pktgencap shared memory\n");
        return 1;
    }

    nfp_ipc_client_desc.name = "pktgencap_ctl";
    nfp_ipc_client = nfp_ipc_client_start(pktgen_nfp.shm.nfp_ipc, &nfp_ipc_client_desc);
    if (nfp_ipc_client < 0) {
        fprintf(stderr, "Failed to connect to pktgen SHM\n");
        return 1;
    }

    for (i=1; i<argc; i++) {
        struct pktgen_ipc_msg *pktgen_msg;
        struct nfp_ipc_msg *msg;
        struct nfp_ipc_event event;
        int timeout;
        int poll;

        timeout = 1000*1000;
        msg = nfp_ipc_msg_alloc(pktgen_nfp.shm.nfp_ipc, sizeof(struct pktgen_ipc_msg));
        pktgen_msg = (struct pktgen_ipc_msg *)(&msg->data[0]);
        if (!strcmp(argv[i],"shutdown")) {
            pktgen_msg->reason = PKTGEN_IPC_SHUTDOWN;
            pktgen_msg->ack = 0;
            nfp_ipc_client_send_msg(pktgen_nfp.shm.nfp_ipc, nfp_ipc_client, msg);
        } else if (!strcmp(argv[i],"pktdump")) {
            pktgen_msg->reason = PKTGEN_IPC_DUMP_BUFFERS;
            pktgen_msg->ack = 0;
            nfp_ipc_client_send_msg(pktgen_nfp.shm.nfp_ipc, nfp_ipc_client, msg);
        } else if (!strcmp(argv[i],"bufshow")) {
            pktgen_msg->reason = PKTGEN_IPC_SHOW_BUFFER_HEADERS;
            pktgen_msg->ack = 0;
            nfp_ipc_client_send_msg(pktgen_nfp.shm.nfp_ipc, nfp_ipc_client, msg);
        } else if (!strcmp(argv[i],"load")) {
            pktgen_msg->reason = PKTGEN_IPC_LOAD;
            pktgen_msg->ack = 0;
            nfp_ipc_client_send_msg(pktgen_nfp.shm.nfp_ipc, nfp_ipc_client, msg);
        } else if (!strcmp(argv[i],"gen")) {
            pktgen_msg->reason = PKTGEN_IPC_HOST_CMD;
            pktgen_msg->ack = 0;
            pktgen_msg->generate.base_delay = 1<<24;
            pktgen_msg->generate.total_pkts = 57;
            nfp_ipc_client_send_msg(pktgen_nfp.shm.nfp_ipc, nfp_ipc_client, msg);
        } else {
            usage();
            break;
        }
        for (;;) {
            poll = nfp_ipc_client_poll(pktgen_nfp.shm.nfp_ipc, nfp_ipc_client, timeout, &event);
            if (poll==NFP_IPC_EVENT_SHUTDOWN) {
                i = argc;
                break;
            }
            if (poll==NFP_IPC_EVENT_MESSAGE) {
                struct pktgen_ipc_msg *msg;
                msg = (struct pktgen_ipc_msg *)&event.msg->data[0];
                if (msg->ack < 0) {
                    fprintf(stderr,"Error returned by pktgencap (%d) for command %s\n",msg->ack,argv[i]);
                    i = argc;
                }
                break;
            }
        }
    }

    nfp_ipc_client_stop(pktgen_nfp.shm.nfp_ipc, nfp_ipc_client);

    return 0;
}

