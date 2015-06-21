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
 * @file          include/firmware/pktgen.h
 * @brief         Packet generator firmware include shared with host
 *
 */

/** Includes
 */
#include <stdint.h> 

/** struct pktgen_cls_ring
 */
struct pktgen_cls_ring {
    uint32_t base;
    uint32_t item_mask;
};

/** struct pktgen_cls_host
 */
struct pktgen_cls_host {
    struct pktgen_cls_ring cls_ring;
    uint32_t wptr;
    uint32_t rptr;
};

/** struct pktgen_host_cmd
 */
enum {
    PKTGEN_HOST_CMD_DMA=0,
    PKTGEN_HOST_CMD_PKT=1,
};
struct pktgen_host_cmd {
    union {
        struct {
            int      cmd_type:8;
            uint32_t pad[3];
        } all_cmds;
        struct {
            int      cmd_type:8;
            unsigned int length:24;
            uint32_t mu_base_s8;
            uint32_t pcie_base_low;
            uint32_t pcie_base_high;
        } dma_cmd;
        struct {
            int      cmd_type:8;
            uint32_t base_delay;
            uint32_t mu_base_s8;
            int      total_pkts;
        } pkt_cmd;
    };
};

