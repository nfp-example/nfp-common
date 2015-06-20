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
#include "nfp_support.h"
#include "pktgen_mem.h"

/** Defines
 */
#define PCIE_HUGEPAGE_SIZE (1<<20)

/** struct pktgen_nfp
 */
struct pktgen_nfp {
    struct nfp *nfp;
    struct nfp_cppid cls_wptr, cls_ring;
    char *pcie_base;
    uint64_t pcie_base_addr, p;
    long pcie_size, s;
    struct pktgen_mem_layout *mem_layout;
};

/** pktgen_load_schedule
static void pktgen_load_schedule(void)
{
}
 */

/** pktgen_load_scripts
static void pktgen_load_scripts(void)
{
}
 */

/** pktgen_load_packet_data
 * Could be one of many
 * Needs a base in MU
 * Needs an NFP instance
 * Linear data
static void pktgen_load_packet_data(void)
{
}
 */

/** pktgen_load_nfp
 */
static int
pktgen_load_nfp(struct pktgen_nfp *pktgen_nfp, int dev_num, const char *nffw_filename)
{
    pktgen_nfp->nfp = nfp_init(dev_num);
    if (!pktgen_nfp->nfp) {
        fprintf(stderr, "Failed to open NFP\n");
        return 1;
    }
    if (nfp_fw_load(pktgen_nfp->nfp, nffw_filename) < 0) {
        fprintf(stderr, "Failed to load NFP firmware\n");
        return 1;
    }
    /*nfp_show_rtsyms(nfp);*/
    if ((nfp_get_rtsym_cppid(pktgen_nfp->nfp,
                             "host_shared_data",
                             &pktgen_nfp->cls_wptr) < 0) ||
        (nfp_get_rtsym_cppid(pktgen_nfp->nfp,
                             "host_ring_base",
                             &pktgen_nfp->cls_ring) < 0)) {
        fprintf(stderr, "Failed to find necessary symbols\n");
        return 1;
    }
    return 0;
}

/** pktgen_give_pcie_pcap_buffers
 */
static int pktgen_give_pcie_pcap_buffers(struct pktgen_nfp *pktgen_nfp,
                                         void *p,
                                         uint64_t pcie_base_addr,
                                         long pcie_size)
{
    int offset;
    int num_buffers;
    int err;

    offset = 0;
    num_buffers = 0;
    while (pcie_size > 0) {
        err = nfp_write(pktgen_nfp->nfp,
                        &pktgen_nfp->cls_ring,
                        offset,
                        (void *)&p, sizeof(p));
        if (err) return err;
        pcie_base_addr += 1 << 18;
        pcie_size -= 1 << 18;
        offset += sizeof(pcie_base_addr);
        num_buffers++;
    }

    if (nfp_write(pktgen_nfp->nfp,&pktgen_nfp->cls_wptr,0,(void *)&num_buffers,sizeof(num_buffers)) != 0) {
        fprintf(stderr,"Failed to write buffers etc to NFP memory\n");
        return 1;
    }
    return 0;
}

/** mem_load_callback
 */
static int 
mem_load_callback(void *handle,
                  struct pktgen_mem_layout *layout,
                  struct pktgen_mem_data *data)
{
    printf("Load data from %p to %010lx size %ld\n",
           data->base,
           ((uint64_t)data->mu_base_s8)<<8,
           data->size);
    return 0;
}

/** mem_alloc_callback
 */
static uint64_t emem0_base=0x80000;
static int 
mem_alloc_callback(void *handle,
                   uint64_t size,
                   uint64_t min_break_size,
                   int memory_mask,
                   struct pktgen_mem_data *data)
{

    if (memory_mask==0) return 0;
    if ((memory_mask & 1) == 0 ) return 0;

    size = ((size+4095)/4096)*4096;
    data[0].size = size;
    data[0].mu_base_s8 = emem0_base>>8;
    emem0_base += size;
    printf("Allocated memory size %ld base %08x00\n",
           size,
           data[0].mu_base_s8);
    return 0;
}

/** Main
    For this we load the firmware and give it packets.
 */
extern int
main(int argc, char **argv)
{
    struct pktgen_nfp pktgen_nfp;
    int pcie_size;
    void *pcie_base;
    uint64_t pcie_base_addr;

    pktgen_nfp.mem_layout = pktgen_mem_alloc(&pktgen_nfp,
                                             mem_alloc_callback,
                                             mem_load_callback,
                                             NULL );
    if (pktgen_mem_open_directory(pktgen_nfp.mem_layout,
                                  "../pktgen_data/") != 0) {
        fprintf(stderr,"Failed to load packet generation data\n");
        return 4;
    }

    if (pktgen_mem_load(pktgen_nfp.mem_layout) != 0) {
        fprintf(stderr,"Failed to load generator memory\n");
        return 4;
    }

    if (pktgen_load_nfp(&pktgen_nfp, 0, "firmware/nffw/pcap.nffw")!=0) {
        fprintf(stderr,"Failed to open and load up NFP with ME code\n");
        return 4;
    }

    pcie_size = nfp_huge_malloc(pktgen_nfp.nfp,
                                (void **)&pcie_base,
                                &pcie_base_addr,
                                PCIE_HUGEPAGE_SIZE);
    if (pcie_size == 0) {
        fprintf(stderr,"Failed to allocate memory\n");
        return 4;
    }

    if (pktgen_give_pcie_pcap_buffers(&pktgen_nfp,
                                      pcie_base, pcie_base_addr, pcie_size) != 0) {
        fprintf(stderr,"Failed to give PCIe pcap buffers\n");
        return 4;
    }

    if (nfp_fw_start(pktgen_nfp.nfp)<0) {
        fprintf(stderr,"Failed to start NFP firmware\n");
        return 4;
    }

    if (pktgen_mem_load(pktgen_nfp.mem_layout) != 0) {
        fprintf(stderr,"Failed to load generator memory\n");
        return 4;
    }

    usleep(1000*1000);

    pktgen_mem_load(pktgen_nfp.mem_layout);
    nfp_huge_free(pktgen_nfp.nfp, pcie_base);
    nfp_shutdown(pktgen_nfp.nfp);
    return 0;
}

