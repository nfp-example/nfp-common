/*a Copyright */
/**
 * Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
 *
 * @file        data_coproc_config.h
 * @brief       Configuration for basic data coprocessor
 *
 */

/** Configuration summary:
 *
 * This data is used for the startup staged synchronization. It
 * requires the number of islands; for each island, the number of MEs
 * used in the island, and for each ME the number of contexts used
 *
 */
#define DCPRC_ISLANDS     2
#define DCPRC_MES_PCIE0   1
#define DCPRC_MES_WORKER  1
#define DCPRC_INIT_STAGES 8
#define DCPRC_INIT_STAGE_CSR_INIT      2
#define DCPRC_INIT_STAGE_PREHOST_LOAD  4
#define DCPRC_INIT_STAGE_HOST_STARTED  6
#define DCPRC_INIT_STAGE_READY_TO_RUN  8

/** Defines
 */
#define PCIE_ISLAND 4
#define PCIE_DMA_CFG 0
#define PCIE_DMA_CFG_CSR 0

