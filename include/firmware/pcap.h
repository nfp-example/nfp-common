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

#define PCAP_HOST_CLS_SHARED_DATA_SIZE 64
#define PCAP_HOST_CLS_RING_SIZE 1024
#define PCAP_HOST_CLS_RING_SIZE__STR STRINGIFY(PKTGEN_CLS_RING_SIZE)

/* Note that PCAP_BUF_TOTAL_PKTS MUST NOT exceed the 'number' field in
   the pcap_buf_desc*/
#define PCAP_BUF_TOTAL_PKTS 1024

/* PCAP_BUF_MAX_PKT must be a little less than PCAP_BUF_TOTAL_PKTS -
 possibly one less would be sufficient
*/
#define PCAP_BUF_MAX_PKT (PCAP_BUF_TOTAL_PKTS-4)

/* PCAP_BUF_FIRST_PKT_OFFSET must be greater than
 * 64B+(PCAP_BUF_TOTAL_PKTS/8)+PCAP_BUF_MAX_PKT*sizeof(pcap_pkt_buf_desc)
 * 64 + 128 + 1020*8
 *
 * Since the latter dominates, 16*PCAP_BUF_MAX_PKT is fine... it wastes
 * a bit of the buffer but not much
 */
#define PCAP_BUF_FIRST_PKT_OFFSET (16*1024)

/** struct pcap_pkt_buf_desc
 *
 * Packet buffer descriptor stored in the host and MU buffer.  The offset
 * is the 64B block offset from mu_base_s8.  num_blocks is the number
 * of 64B block spaces used in the MU buffer for the packet. The
 * sequence number is a 16/32-bit sequence number of the packet, as
 * supplied by the NBI Rx.
 *
 */
#ifdef __NFCC_VERSION
struct pcap_pkt_buf_desc {
    uint32_t offset:16;
    uint32_t num_blocks:16;
    uint32_t seq;
};
#else
struct pcap_pkt_buf_desc {
    uint32_t num_blocks:16;
    uint32_t offset:16;
    uint32_t seq;
};
#endif

/** struct pcap_buf_hdr
 *
 *  Structure placed at start of an MU/host buffer
 *
 *  Not actually transferred to host, so really only in the MU side
 */
struct pcap_buf_hdr {
    uint32_t buf_seq;         /* MU/host buffer sequence number */
    uint32_t total_packets;   /* Valid when buffer is complete */
    uint32_t pcie_base_low;   /* Filled at pre-allocation */
    uint32_t pcie_base_high;  /* Filled at pre-allocation */
};

/** struct pcap_buffer
 *
 * MU/host buffer layout, up to the packet data, which is placed at
 * PCAP_BUF_FIRST_PKT_OFFSET
 *
 * Note that this must be less than PCAP_BUF_FIRST_PKT_OFFSET in size
 * Note also that the pkt_add_mu_buf_desc clears this structure in a
 * 'knowledgeable manner', i.e. it knows the structure and offsets
 * intimately. So changing this structure requires changing that
 * function.
 */
struct pcap_buffer {
    struct pcap_buf_hdr hdr;
    int      dmas_completed;  /* For DMA Master/slaves */
    uint32_t pad[11];         /* Pad to 64B alignment */
    uint32_t pkt_bitmask[PCAP_BUF_TOTAL_PKTS/32]; /* n*64B to pad
                                                 * properly */
    struct pcap_pkt_buf_desc pkt_desc[PCAP_BUF_MAX_PKT];
};

/** struct pcap_cls_host
 */
struct pcap_cls_host {
    uint32_t wptr;
};

