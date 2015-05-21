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
 * @file          firmware/lib/nfp/ctm.c
 * @brief         Library for NFP CTM functions
 */
#ifndef _NFP__CTM_H_
#define _NFP__CTM_H_

/** Includes required
 */
#include <stdint.h>

/** ctm_get_packet_address
 * 
 * @param packet_number
 *
 * Return the CTM address of a packet number
 */
__intrinsic uint32_t ctm_get_packet_address(int packet_number);

/** ctm_dma_to_memory
 * 
 * @param mu_address  40-bit MU address to DMA to, 8B aligned
 * @param ctm_address 18-bit CTM address to DMA from, 8B aligned
 * @param size        size in 64B lumps
 *
 * Uses the PE DMA engine to DMA a region of CTM SRAM to memory unit
 *
 */
__intrinsic void ctm_dma_to_memory(uint64_t mu_address,
                                   uint32_t ctm_address, 
                                   int size);
/** Close guard
 */
#endif /*_NFP__CTM_H_ */
