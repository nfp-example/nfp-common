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
 * @file          pktgencap.c
 * @brief         Test of packet generator/capture
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

/** Defines
 */

/** struct pktgen_nfp
 */
struct pktgen_nfp {
    struct nfp *nfp;
    struct {
        char *base;
        size_t size;
        struct nfp_ipc *nfp_ipc;
    } shm;
};

/** Static variables
 */
static const char *shm_filename="/tmp/nfp_shm.lock";
static int shm_key = 'x';

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

/** mem_dump
 */
static void mem_dump(void *addr, int size)
{
    int i;
    int repetitions;
    unsigned char text[17];

    repetitions = 0;
    for (i=0; i<size; i++) {
        unsigned char ch;
        int pos;
        ch = ((unsigned char *)addr)[i];
        pos = i % 16;
        if (pos == 0) {
            if (i != 0) {
                if (repetitions == 0)
                    printf(" : %s\n", text);
                if (memcmp(((unsigned char *)addr)+i,
                           ((unsigned char *)addr)+i-16,
                           16)==0) {
                    i+=15;
                    repetitions++;
                    continue;
                }
                if (repetitions!=0)
                    printf("%04x: *\n", i-16);
                repetitions = 0;
            }
            printf("%04x:", i);
        }
        printf(" %02x", ch);
        text[pos] = ch;
        if ((ch < 0x20) || (ch > 0x7e)) {
            text[pos] = '.';
        }
        text[pos+1]=0;
    }
    if (repetitions==0) {
        for (; (i % 16)!= 0; i++)
            printf("   ");
        printf(" : %s\n", text);
    } else {
        printf("%04x: *\n", i-16);
    }
}

/** Main
 */
#include "firmware/pcap.h"
extern int
main(int argc, char **argv)
{
    struct pktgen_nfp pktgen_nfp;
    int nfp_ipc_client;

    pktgen_nfp.nfp = nfp_init(-1);

    pktgen_alloc_shm(&pktgen_nfp);

    struct nfp_ipc_client_desc nfp_ipc_client_desc;
    nfp_ipc_client = nfp_ipc_client_start(pktgen_nfp.shm.nfp_ipc, &nfp_ipc_client_desc);
    if (nfp_ipc_client < 0) {
        fprintf(stderr, "Failed to connect to pktgen SHM\n");
        return 1;
    }

    usleep(10*1000*1000);

        if (1) {
            uint64_t phys_offset;
            phys_offset = 1<<20;
            int j;
            struct pcap_buffer *pcap_buffer;
            pcap_buffer = (struct pcap_buffer *)(pktgen_nfp.shm.base + phys_offset);
            for (j=0; j<PCAP_BUF_MAX_PKT; j++) {
                if (pcap_buffer->pkt_desc[j].offset==0)
                    break;
                printf("%d: %04x %04x %08x\n",j,
                       pcap_buffer->pkt_desc[j].offset,
                       pcap_buffer->pkt_desc[j].num_blocks,
                       pcap_buffer->pkt_desc[j].seq
                    );
                mem_dump(((char *)pcap_buffer) + (pcap_buffer->pkt_desc[j].offset<<6), 64);
            }
        }

    nfp_ipc_client_stop(pktgen_nfp.shm.nfp_ipc, nfp_ipc_client);

    return 0;
}

