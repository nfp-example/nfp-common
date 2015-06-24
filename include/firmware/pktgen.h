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

/** Defines
 */
#ifndef STRINGIFY
#define __STRINGIFY(x) #x
#define STRINGIFY(x) __STRINGIFY(x)
#endif
#define PKTGEN_CLS_RING_SIZE 1024
#define PKTGEN_CLS_RING_SIZE__STR STRINGIFY(PKTGEN_CLS_RING_SIZE)

/** struct pktgen_sched_entry
 */
#ifdef __NFCC_VERSION
struct pktgen_sched_entry {
    uint32_t     tx_time_lo;   /* Not sure what units... */
    unsigned int script_ofs:24;   /* Offset to script from script base */
    unsigned int tx_time_hi:8; /* Top 8 bits */
    uint32_t     mu_base_s8;   /* 256B aligned packet start */
    unsigned int flags:16;    /* */
    unsigned int length:16;    /* Length of the packet (needed to DMA it) */
};
#else
struct pktgen_sched_entry {
    uint32_t     tx_time_lo;   /* Not sure what units... */
    unsigned int tx_time_hi:8; /* Top 8 bits */
    unsigned int script_ofs:24;   /* Offset to script from script base */
    uint32_t     mu_base_s8;   /* 256B aligned packet start */
    unsigned int length:16;    /* Length of the packet (needed to DMA it) */
    unsigned int flags:16;    /* */
};
#endif

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
    uint32_t ack_data;
};

/** struct pktgen_host_cmd
 */
enum {
    PKTGEN_HOST_CMD_PKT=1,
    PKTGEN_HOST_CMD_ACK=2,
    PKTGEN_HOST_CMD_DMA=3,
};
#ifdef __NFCC_VERSION
struct pktgen_host_cmd {
    union {
        struct {
            int      cmd_type:8;
            int      pad_0:24;
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
            int      pad_0:24;
            uint32_t base_delay;
            uint32_t mu_base_s8;
            int      total_pkts;
        } pkt_cmd;
        struct {
            int      cmd_type:8;
            unsigned int pad_0:24;
            uint32_t data;
            uint32_t pad_1[2];
        } ack_cmd;
    };
};

#else

struct pktgen_host_cmd {
    union {
        struct {
            int      pad_0:24;
            int      cmd_type:8;
            uint32_t pad[3];
        } all_cmds;
        struct {
            unsigned int length:24;
            int      cmd_type:8;
            uint32_t mu_base_s8;
            uint32_t pcie_base_low;
            uint32_t pcie_base_high;
        } dma_cmd;
        struct {
            int      pad_0:24;
            int      cmd_type:8;
            uint32_t base_delay;
            uint32_t mu_base_s8;
            int      total_pkts;
        } pkt_cmd;
        struct {
            unsigned int pad_0:24;
            int      cmd_type:8;
            uint32_t data;
            uint32_t pad_1[2];
        } ack_cmd;
    };
};

#endif
