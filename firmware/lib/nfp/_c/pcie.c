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
 * @file          firmware/lib/nfp/_c/pcie.c
 * @brief         PCIe library functions
 *
 */

/** Includes
 */
#include <nfp/pcie.h>
#include <stdint.h>
#include <nfp.h>

/** Defines
 */
#define     NFP_PCIE_DMA_CMD_DMA_MODE_shf                    (14)
#define     NFP_PCIE_DMA_CMD_DMA_MODE_2                      (2)

/** pcie_dma_cmd_sig
 *
 * Sets signal for a DMA command for caller ME/context
 *
 * @param cmd   DMA command to set signal to
 * @param sig   Signal to include in DMA command
 *
 */
__intrinsic void
pcie_dma_cmd_sig(struct nfp_pcie_dma_cmd *cmd, SIGNAL *sig)
{
    uint32_t meid, ctx;
    uint32_t mode, mode_mask;

    /* signal (dma mode) is at the top of the register, so this shift
     * works fine */
    mode_mask = (-1)<<NFP_PCIE_DMA_CMD_DMA_MODE_shf;
    meid = __MEID;
    ctx = ctx();
    mode = ( ((meid & 0xf) << 13) |
             (((meid >> 4) & 0x3F) <<7 ) |
             ((ctx & 7) << 4) |
             __signal_number(sig) );

    cmd->__raw[1] = ((mode<<NFP_PCIE_DMA_CMD_DMA_MODE_shf) |
                     (cmd->__raw[1] &~ mode_mask));
}

/** pcie_read_int
 *
 * @param data    Transfer registers with data to read
 * @param pcie    PCIe island number
 * @param offset  Offset in to PCIe internal target space
 * @param size    Size in words to read
 *
 */
__intrinsic void
pcie_read_int(__xread void *data, int pcie, uint32_t offset, int size )
{
    uint32_t size_in_words = size>>2;
    uint32_t addr_s8 = pcie<<(38-8);
    __asm pcie[read_int, *data, pcie, <<8, offset, size_in_words];
}

/** pcie_write_int
 *
 * @param data    Transfer registers with data to write
 * @param pcie    PCIe island number
 * @param offset  Offset in to PCIe internal target space
 * @param size    Size in words to write
 *
 */
__intrinsic void
pcie_write_int(__xwrite void *data, int pcie, uint32_t offset, int size )
{
    uint32_t size_in_words = size>>2;
    uint32_t addr_s8 = pcie<<(38-8);
    __asm pcie[write_int, *data, pcie, <<8, offset, size_in_words];
}
