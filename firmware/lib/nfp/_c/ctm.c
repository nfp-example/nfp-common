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
 * @file          firmware/lib/ctm.c
 * @brief         Library for NFP CTM functions
 */

/** Includes
 */
#include "ctm.h"
#include <stdint.h>
#include "me.h"

/** ctm_get_packet_address
 * 
 * @param packet_number
 *
 * Return the CTM address of a packet number
 */
__intrinsic uint32_t ctm_get_packet_address( int packet_number  )
{
    SIGNAL sig;
    __xread uint32_t pkt_address[2];

    __asm {
        mem[packet_read_packet_status, pkt_address, packet_number, 0, 1], \
            ctx_swap[sig];
    }
    return (pkt_address[0]&0x3ff)<<8;
}

/** ctm_dma_to_memory
 * 
 * @param mu_address  40-bit MU address to DMA to, 8B aligned
 * @param ctm_address 18-bit CTM address to DMA from, 8B aligned
 * @param size        size in 64B lumps
 *
 * Uses the PE DMA engine to DMA a region of CTM SRAM to memory unit
 *
 * Note that at most 16 DMAs can be in progress at once, and
 * this function does not attempt to manage that
 *
 * Uses the CTM command pe_dma_to_memory_buffer
 * - length = #64B lumps - 1
 * - address = bottom 32 bits of MU address (8B aligned)
 * - byte_mask = top 8 bits of MU address
 * - data_ref = bottom 14 bits of CTM address>>3
 * - data_master = top bit of CTM address>>3
 * - signals on DMA completion
 * 
 * Need to override the byte_mask, data_master/data_ref, length
 * - The byte_mask must go in cmd_indirect_ref_0, bottom 8 bits
 * - prev_alu must have OVE_DATA=2 (master/ref in DATA=[16;16])
 * - prev_alu must have OV_LEN (length in LENGTH=[5;8])
 */
__intrinsic void ctm_dma_to_memory( uint64_t mu_address,
                                    uint32_t ctm_address, 
                                    int size )
{
    SIGNAL sig;
    uint32_t override;
    uint32_t temp;

    local_csr_write(local_csr_cmd_indirect_ref0,
                    (uint32_t)(mu_address>>32) ); 
    override = ( (2<<3) | (1<<6) | (1<<7) |
                 ((size-1)<<8) |
                 (ctm_address<<(16-3)) );
    temp = (uint32_t) mu_address;
    __asm {
        alu[ --, --, B, override ];
        mem[pe_dma_to_memory_buffer, --, temp, 0, 1], indirect_ref, \
            ctx_swap[sig];
    }
}

