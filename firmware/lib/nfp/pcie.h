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
/** a **/
    union {
/** a **/
        struct {
/** a **/
            unsigned int cpp_addr_lo:32;
/** a **/
            unsigned int mode_sel:2;
/** a **/
            unsigned int dma_mode:16;
/** a **/
            unsigned int cpp_token:2;
/** a **/
            unsigned int dma_cfg_index:4;
/** a **/
            unsigned int cpp_addr_hi:8;
/** a **/
            unsigned int pcie_addr_lo:32;
/** a **/
            unsigned int length:12;
/** a **/
            unsigned int rid:8;
/** a **/
            unsigned int rid_override:1;
/** a **/
            unsigned int trans_class:3;
/** a **/
            unsigned int pcie_addr_hi:8;
/** a **/
        };
/** a **/
        struct {
/** a **/
            unsigned int pad_0:32;
/** a **/
            unsigned int signal:17;
/** a **/
            unsigned int dma_mode_signal:1;
/** a **/
            unsigned int pad_1:14;
/** a **/
            unsigned int pad_2:32;
/** a **/
            unsigned int pad_3:32;
/** a **/
        };
/** a **/
        unsigned int __raw[4];
/** a **/
    };
/** a **/
};

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

/** pcie_dma_enqueue
 *
 * Enqueue a PCIe DMA command to a queue in an island
 *
 * @param island  PCIe island number
 * @param cmd     DMA command to enqueue
 * @param queue   Queue number to use
 *
 */
__intrinsic void pcie_dma_enqueue(int island,
                                  __xwrite struct nfp_pcie_dma_cmd *cmd,
                                  int queue);

/** pcie_dma_buffer
 *
 * DMA a buffer to/from PCIe from/to CPP of any length
 *
 * @param island  PCIe island number
 * @param cmd     DMA command to enqueue
 * @param queue   Queue number to use
 *
 * The PCIe address of the buffer and CPP address must not cross a 4GB
 * boundary.
 *
 * Only a single DMA is used at any one time - this is slower than
 * necessary, if many PCIe DMA queue entries were used. However, it
 * permits many instances of this function to be called across the
 * chip simultaneously (subject to PCIe DMA credits)
 *
 */
__intrinsic void pcie_dma_buffer(int island, uint64_32_t pcie_addr,
                                 uint64_32_t cpp_addr, int length,
                                 int queue, int token, int dma_config);

/** Close guard
 */
#endif /*_NFP__PCIE_H_ */
