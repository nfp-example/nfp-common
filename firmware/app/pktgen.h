/** Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
 *
 * @file        pcap.h
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
 * 2 Tx islands
 * 1 init island
 *
 * Host island is two MEs
 *  First ME:
 *    One thread runs DMA master, others batch distributors
 *  Second ME:
 *    Runs eight batch distributors
 * 
 * Tx islands is eight MEs (one par batch)
 *  All threads run packet receive
 */
#define PKTGEN_TX_ISLANDS 2
#define PKTGEN_ISLANDS (1+1+PKTGEN_TX_ISLANDS)

#define PKTGEN_HOST_CTXTS 8
#define PKTGEN_HOST_MES 1

#define PKTGEN_TX_CTXTS 8
#define PKTGEN_TX_MES 8

#define PKTGEN_INIT_CTXTS 1
#define PKTGEN_INIT_MES 1

#define PKTGEN_INIT_STAGES 4
#define PKTGEN_INIT_STAGE_CSR_INIT      1
#define PKTGEN_INIT_STAGE_PREHOST_LOAD  2
#define PKTGEN_INIT_STAGE_HOST_STARTED  3
#define PKTGEN_INIT_STAGE_READY_TO_RUN  4
