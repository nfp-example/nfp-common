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
 * @file          pcap_test.c
 * @brief         Test of packet capture
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

/** Main
    For this we load the firmware and give it packets.
 */
extern int main(int argc, char **argv)
{
    struct nfp *nfp;
    struct nfp_cppid cls_wptr, cls_ring;
    char *pcie_base;
    uint64_t pcie_base_addr, p;
    long pcie_size, s;
    int offset;
    int err;

    nfp=nfp_init(0);
    if (!nfp) {
        fprintf(stderr,"Failed to open NFP\n");
        return 4;
    }
    if (nfp_fw_load(nfp,"build/pcap.nffw")<0) {
        fprintf(stderr,"Failed to load NFP firmware\n");
        return 4;
    }
    nfp_show_rtsyms(nfp);
    if ((nfp_get_rtsym_cppid(nfp, "cls_host_shared_data", &cls_wptr)<0) ||
        (nfp_get_rtsym_cppid(nfp, "cls_host_ring_base", &cls_ring)<0)) {
        fprintf(stderr,"Failed to find necessary symbols\n");
        return 4;
    }
    pcie_size = nfp_huge_malloc(nfp, (void **)&pcie_base, &pcie_base_addr, PCIE_HUGEPAGE_SIZE);
    if (pcie_size==0) {
        fprintf(stderr,"Failed to allocate memory\n");
        return 4;
    }
    err=0;
    p=pcie_base_addr;
    s=pcie_size;
    for (offset=0;(s>0);offset+=sizeof(p)) {
        bzero(pcie_base+offset,16*1024);
        if (err==0) {
            err=nfp_write(nfp,&cls_ring,offset,(void *)&p,sizeof(p));
        }
        p+=1<<18;
        s-=1<<18;
    }
    offset = offset/sizeof(p);
    if (err==0) err=nfp_write(nfp,&cls_wptr,0,(void *)&offset,sizeof(offset));
    if (err!=0) {
        fprintf(stderr,"Failed to write buffers etc to NFP memory\n");
    }
    if (nfp_fw_start(nfp)<0) {
        fprintf(stderr,"Failed to start NFP firmware\n");
        return 4;
    }
    usleep(1000*1000);
    nfp_huge_free(nfp,pcie_base);
    nfp_shutdown(nfp);
    return 0;
}

