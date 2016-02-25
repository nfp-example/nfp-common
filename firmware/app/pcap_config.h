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
#define PCAP_HOST_CTXTS 8
#define PCAP_HOST_MES 2

#define PCAP_RX_CTXTS 8
#define PCAP_RX_MES 1
#define PCAP_RX_ISLANDS    3

#define PCAP_ISLANDS (1+PCAP_RX_ISLANDS)

// The CSR init stage is used to write basic CSRs
// For example, this is network configuration, DMA setup, CTM allocation, etc
// The next stage is prehost load.
// Here queues should be configured
// Once the prehost load is done, final preparation must be performed
// This can include setting up buffer allocations and adding to queues
// Then the code is ready to run
#define PCAP_INIT_STAGES 3
#define PCAP_INIT_STAGE_CSR_INIT      1
#define PCAP_INIT_STAGE_PREHOST_LOAD 2
#define PCAP_INIT_STAGE_READY_TO_RUN 3

#define PCAP_NBI_ISLAND   8
#define PCAP_PCIE_ISLAND  4
#define PCAP_PCIE_DMA_CFG 0
#define PCAP_PCIE_DMA_CFG_CSR 0
