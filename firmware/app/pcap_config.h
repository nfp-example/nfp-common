/** Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
 *
 * @file        pcap_config.h
 * @brief       Packet capture ME/ISLAND configuration
 *
 */

/** Configuration summary:
 *
 * This data is used for the startup staged synchronization. It
 * requires the number of islands; for each island, the number of MEs
 * used in the island, and for ach ME the number of contexts used
 *
 * 1 host island
 * 3 Rx island
 *
 * Host island is two MEs
 *  First ME:
 *    One thread runs DMA master, others DMA slave
 *  Second ME:
 *    One thread runs buffer recycler, others DMA slave
 * 
 * Rx islands is single ME
 *  All threads run packet receive
 */
#define PCAP_ISLANDS (1+3)

#define PCAP_HOST_CTXTS 8
#define PCAP_HOST_MES 2

#define PCAP_RX_CTXTS 8
#define PCAP_RX_MES 1

#define PCAP_INIT_STAGES 2
#define PCAP_INIT_STAGE_PREHOST_LOAD 1
#define PCAP_INIT_STAGE_READY_TO_RUN 2
