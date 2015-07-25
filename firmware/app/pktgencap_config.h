/** Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
 *
 * @file        pktgencap.h
 * @brief       Packet generator/capture ME/ISLAND configuration
 *
 */

/** Configuration summary:
 *
 * This data is used for the startup staged synchronization. It
 * requires the number of islands; for each island, the number of MEs
 * used in the island, and for each ME the number of contexts used
 *
 * 1 host island
 * 3 Rx island
 * 2 Tx islands
 * 1 init island
 *
 * Host island is two MEs
 *  First Rx ME:
 *    One thread runs DMA master, others DMA slave
 *  Second Rx ME:
 *    One thread runs buffer recycler, others DMA slave
 *  First Tx ME:
 *    One thread runs DMA master, others batch distributors
 *  Second Tx ME:
 *    Runs eight batch distributors
 * 
 * Rx islands is single ME
 *  All threads run packet receive
 *
 * Tx islands is eight MEs (one par batch)
 *  All threads run packet transmit slave
 */
#define PKTGENCAP_RX_ISLANDS 3
#define PKTGENCAP_TX_ISLANDS 1
#define PKTGENCAP_ISLANDS  (1+1+PKTGENCAP_RX_ISLANDS+PKTGENCAP_TX_ISLANDS)
#define PKTGEN_ISLANDS  PKTGENCAP_ISLANDS
#define PCAP_ISLANDS    PKTGENCAP_ISLANDS

#define PCAP_HOST_CTXTS 8
#define PCAP_HOST_MES 3
#define PKTGEN_HOST_CTXTS 8
#define PKTGEN_HOST_MES 3

#define PCAP_RX_CTXTS 8
#define PCAP_RX_MES 1


#define PKTGEN_TX_CTXTS 8
#define PKTGEN_TX_MES 8

#define PKTGEN_INIT_CTXTS 1
#define PKTGEN_INIT_MES 1

#define PKTGEN_INIT_STAGES 4
#define PKTGEN_INIT_STAGE_CSR_INIT      1
#define PKTGEN_INIT_STAGE_PREHOST_LOAD  2
#define PKTGEN_INIT_STAGE_HOST_STARTED  3
#define PKTGEN_INIT_STAGE_READY_TO_RUN  4

#define PCAP_INIT_STAGES 4
#define PCAP_INIT_STAGE_CSR_INIT      1
#define PCAP_INIT_STAGE_PREHOST_LOAD 2
#define PCAP_INIT_STAGE_READY_TO_RUN 4
