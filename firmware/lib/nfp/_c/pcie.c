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

#define CLS_DEBUG_JOURNAL_RING 0
#define PCIE_ISLAND 4
#define PCIE_BURST_SIZE 1024

/** pcie_read_int
 *
 * Read from the internal target of the PCIe controller in an island
 *
 * @param data    Transfer registers with data to read
 * @param island  PCIe island number
 * @param offset  Offset in to PCIe internal target space
 * @param size    Size in words to read
 *
 */
__intrinsic void
pcie_read_int(__xread void *data, int island, uint32_t offset, int size)
{
    uint32_t size_in_words = size >> 2;
    uint32_t addr_s8 = island << (38 - 8);
    __asm {
        pcie[read_int, *data, addr_s8, <<8, offset, size_in_words];
    }
}

/** pcie_write_int
 *
 * Write to the internal target of the PCIe controller in an island
 *
 * @param data    Transfer registers with data to write
 * @param island  PCIe island number
 * @param offset  Offset in to PCIe internal target space
 * @param size    Size in words to write
 *
 */
__intrinsic void
pcie_write_int(__xwrite void *data, int island, uint32_t offset, int size)
{
    uint32_t size_in_words = size>>2;
    uint32_t addr_s8 = island << (38 - 8);
    __asm pcie[write_int, *data, addr_s8, <<8, offset, size_in_words];
}

/** pcie_dma_enqueue
 *
 * Enqueue a PCIe DMA command to a queue in an island
 *
 * @param island  PCIe island number
 * @param cmd     DMA command to enqueue
 * @param queue   Queue number to use
 *
 */
__intrinsic void
pcie_dma_enqueue(int island, __xwrite struct nfp_pcie_dma_cmd *cmd,
                 int queue)
{
    pcie_write_int(cmd, island, queue, sizeof(*cmd));
}

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
__intrinsic void
pcie_dma_buffer(int island, uint64_32_t pcie_addr, uint64_32_t cpp_addr,
                int length, int queue, int token, int dma_config)
{
    struct nfp_pcie_dma_cmd cmd;
    __xwrite struct nfp_pcie_dma_cmd cmd_out;
    int meid;
    int ctx;
    int signal;
    SIGNAL sig;

    cmd.__raw[0] = cpp_addr.uint32_lo;
    cmd.__raw[1] = cpp_addr.uint32_hi;
    cmd.__raw[2] = pcie_addr.uint32_lo;
    cmd.__raw[3] = pcie_addr.uint32_hi;
    cmd.cpp_token     = token;      /* Part of __raw[1] */
    cmd.dma_cfg_index = dma_config; /* Part of __raw[1] */

    meid = __MEID;
    ctx  = ctx();
    signal = ( ((meid & 0xf) << 13) |
             (((meid >> 4) & 0x3F) <<7 ) |
             ((ctx & 7) << 4) |
             __signal_number(&sig) );

    while (length > 0) {
        int length_to_dma;

        length_to_dma = length;
        if (length_to_dma > PCIE_BURST_SIZE) {
            length_to_dma = PCIE_BURST_SIZE;
        }
        cmd_out.__raw[0] = cmd.__raw[0];
        cmd_out.__raw[1] = cmd.__raw[1] | (signal << 14);
        cmd_out.__raw[2] = cmd.__raw[2];
        cmd_out.__raw[3] = cmd.__raw[3] | ((length_to_dma - 1) << 20);

        if (0) {
            uint32_t addr_s8;
            uint32_t ofs=CLS_DEBUG_JOURNAL_RING<<2;
            addr_s8 = PCIE_ISLAND << (34 - 8);
            cls_ring_journal_rem(&cmd_out, addr_s8, ofs, sizeof(cmd_out));
        } else {
            pcie_dma_enqueue(island,
                             &cmd_out,
                             queue );
        }
        length -= length_to_dma;
        if (length > 0) {
            cmd.__raw[0] += length_to_dma;
            cmd.__raw[2] += length_to_dma;
        }
        if (0) {
        } else {
            wait_for_all(&sig);
        }
    }
}

