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

/** Defines
 */
#define PCIE_HUGEPAGE_SIZE (1<<20)

/** pktgen_load_schedule
 */
static void pktgen_load_schedule(void)
{
}

/** pktgen_load_scripts
 */
static void pktgen_load_scripts(void)
{
}

/** pktgen_load_packet_data
 * Could be one of many
 * Needs a base in MU
 * Needs an NFP instance
 * Linear data
 */
static void pktgen_load_packet_data(void)
{
}

struct pktgen_mem_region {
    const char *filename;
    FILE *file;
    int data_size;
    int mu_base_s8;
};
struct pktgen_mem_layout {
    int num_pkt_data;
    const char *dirname;
    struct pktgen_mem_region sched;
    struct pktgen_mem_region script;
    struct pktgen_mem_region data[8];
};

/** open_file
 */
static FILE *
open_file(const char *dirname, const char *filename)
{
    char *buf;
    FILE *f;

    if (dirname == NULL ) {
        buf = filename;
    } else {
        buf = malloc(strlen(dirname) + strlen(filename) + 2);
        sprintf(buf, "%s/%s", dirname, filename);
    }
    f = fopen(buf);
    if (dirname != NULL) { free(buf); }
    return f;
}

/** file_size
 */
static long file_size(FILE *f)
{
    long l;
    if (!f) return 0L;
    fseek(f,0L,SEEK_END);
    l = ftell(f);
    fseek(f,0L,SEEK_SET);
    return l;
}

/** pktgen_region_open
 */
static int
pktgen_region_open(struct pktgen_mem_layout *layout,
                   struct pktgen_mem_region *region)
{
    region->file = open_file(layout->dirname, file->filename);
    region->data_size = file_size(region->file);
    if (region->file == NULL) return 1;
    return 0;
}

/** pktgen_region_close
 */
static void
pktgen_region_close(struct pktgen_mem_layout *layout,
                    struct pktgen_mem_region *region)
{
    if (region->file != NULL) {
        fclose(region->file);
        region->file = NULL;
    }
}

/** pktgen_mem_open_directory
 */
static int
pktgen_mem_open_directory(struct pktgen_mem_layout *layout)
{
    int err;
    err = 0;

    layout->sched->filename = "sched";
    layout->script->filename = "script";
    layout->data[0]->filename = "data";
    layout->data[1]->filename = "data_1";
    layout->data[2]->filename = "data_2";
    layout->data[3]->filename = "data_3";
    err |= pktgen_region_open(layout, layout->sched);
    err |= pktgen_region_open(layout, layout->script);
    err |= pktgen_region_open(layout, layout->data[0]);
    pktgen_region_open(layout, layout->data[1]);
    pktgen_region_open(layout, layout->data[2]);
    pktgen_region_open(layout, layout->data[3]);

    return err;
}

/** pktgen_mem_load
 */
static int
pktgen_mem_load(struct pktgen_nfp *pktgen_nfp,
                struct pktgen_mem_layout *layout)
{
    int err;
    err = 0;

    allocate regions;

    pktgen_region_load(layout, layout->sched);
}

/** pktgen_mem_close
 */
static void
pktgen_mem_close(struct pktgen_mem_layout *layout)
{
    pktgen_region_close(layout, layout->sched);
}

struct pktgen_nfp {
    struct nfp *nfp;
    struct nfp_cppid cls_wptr, cls_ring;
    char *pcie_base;
    uint64_t pcie_base_addr, p;
    long pcie_size, s;
};

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
                             "cls_host_shared_data",
                             &cls_wptr) < 0) ||
        (nfp_get_rtsym_cppid(pktgen_nfp->nfp,
                             "cls_host_ring_base",
                             &cls_ring) < 0)) {
        fprintf(stderr, "Failed to find necessary symbols\n");
        return 1;
    }
    return 0;
}

/** pktgen_give_pcie_pcap_buffers
 */
static int pktgen_give_pcie_pcap_buffers(struct pktgen_nfp *pktgen_nfp,
                                         uint64_t pcie_base_addr,
                                         long pcie_size)
{
    int offset;
    int num_buffers;
    offset = 0;
    num_buffers = 0;
    while (pcie_size > 0) {
        err = nfp_write(nfp, &cls_ring, offset, (void *)&p, sizeof(p));
        if (err) return err;
        pcie_base_addr += 1 << 18;
        pcie_size -= 1 << 18;
        offset += sizeof(pcie_base_addr);
        num_buffers++;
    }

    if (nfp_write(nfp,&cls_wptr,0,(void *)&num_buffers,sizeof(num_buffers)) != 0) {
        fprintf(stderr,"Failed to write buffers etc to NFP memory\n");
        return 1;
    }
}

/** Main
    For this we load the firmware and give it packets.
 */
extern int
main(int argc, char **argv)
{
    struct pktgen_nfp pktgen_nfp;
    struct pktgen_mem_layout layout;
    int offset;
    int err;

    if (pktgen_mem_open_directory(&layout) != 0) {
        fprintf(stderr,"Failed to load packet generation data\n");
        return 4;
    }

    pktgen_load_nfp(&pktgen_nfp, 0, "build/pcap.nffw");

    pcie_size = nfp_huge_malloc(&pktgen_nfp.nfp,
                                (void **)&pcie_base,
                                &pcie_base_addr,
                                PCIE_HUGEPAGE_SIZE);
    if (pcie_size == 0) {
        fprintf(stderr,"Failed to allocate memory\n");
        return 4;
    }

    if (pktgen_give_pcie_pcap_buffers() != 0) {
        fprintf(stderr,"Failed to give PCIe pcap buffers\n");
        return 4;
    }

    if (nfp_fw_start(nfp)<0) {
        fprintf(stderr,"Failed to start NFP firmware\n");
        return 4;
    }

    if (pktgen_mem_load(&pktgen_nfp, &layout) != 0) {
    }

    usleep(1000*1000);
    nfp_huge_free(nfp,pcie_base);
    nfp_shutdown(nfp);
    return 0;
}

