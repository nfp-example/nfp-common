/*
 * Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
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
 * @file          firmware/lib/nfp/pcie.h
 * @brief         PCIe library functions
 *
 */
#ifndef _NFP__PCIE_H_
#define _NFP__PCIE_H_

/** Includes required
 */
#include <stdint.h>
#include <nfp.h>

/** Definitions
 */
#define NFP_PCIE_DMA_TOPCI_HI                              0x40000
#define NFP_PCIE_DMA_TOPCI_MED                             0x40020
#define NFP_PCIE_DMA_TOPCI_LO                              0x40040
#define NFP_PCIE_DMA_FROMPCI_HI                            0x40060
#define NFP_PCIE_DMA_FROMPCI_MED                           0x40080
#define NFP_PCIE_DMA_FROMPCI_LO                            0x400a0

/** struct nfp_pcie_dma - from databook
 */
struct nfp_pcie_dma_cmd {
    union {
        struct {
            unsigned int cpp_addr_lo:32;
            unsigned int mode_sel:2;
            unsigned int dma_mode:16;
            unsigned int cpp_token:2;
            unsigned int dma_cfg_index:4;
            unsigned int cpp_addr_hi:8;
            unsigned int pcie_addr_lo:32;
            unsigned int length:12;
            unsigned int rid:8;
            unsigned int rid_override:1;
            unsigned int trans_class:3;
            unsigned int pcie_addr_hi:8;
        };
        unsigned int __raw[4];
    };
};

/** pcie_dma_cmd_sig
 *
 * Sets signal for a DMA command for caller ME/context
 *
 * @param cmd   DMA command to set signal to
 * @param sig   Signal to include in DMA command
 *
 */
__intrinsic void pcie_dma_cmd_sig(struct nfp_pcie_dma_cmd *cmd, SIGNAL *sig);

/** pcie_read_int
 *
 * @param data    Transfer registers with data to read
 * @param pcie    PCIe island number
 * @param offset  Offset in to PCIe internal target space
 * @param size    Size in words to read
 *
 */
__intrinsic void pcie_read_int(__xread void *data, int pcie, uint32_t offset, int size );

/** pcie_write_int
 *
 * @param data    Transfer registers with data to write
 * @param pcie    PCIe island number
 * @param offset  Offset in to PCIe internal target space
 * @param size    Size in words to write
 *
 */
__intrinsic void pcie_write_int(__xwrite void *data, int pcie, uint32_t offset, int size );
/** Close guard
 */
#endif /*_NFP__PCIE_H_ */
