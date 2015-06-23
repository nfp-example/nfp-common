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
 * @file        pktgen_lib.c
 * @brief       A packet generator library for pktgen application
 *
 * This is a library to support a packet generator using a host x86
 * system to supply scripted packet generation
 * 
 */
/** Includes
 */
#include "pktgen.h"
#include <stdint.h>

/** Defines
 */
#define PCIE_ISLAND 4
/* CSR is HALF of the CFG */
#define PKTGEN_PCIE_DMA_CFG 4
#define PKTGEN_PCIE_DMA_CFG_CSR 2

/** pktgen_tx_slave
 */
void pktgen_tx_slave(void);

/** pktgen_batch_distributor
 *
 * Consumes 8 entries of a script and distributed to batches
 *
 */
void pktgen_batch_distributor(void);

/** pktgen_master
 *
 * Monitors cluster scratch and distributes flow script entires to
 * batch distributors
 *
 */
void pktgen_master(void);
