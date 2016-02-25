/**
 *
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
 * @file          firmware/lib/nfp/me.h
 * @brief         Microengine assist library functions and defines
 *
 * This is a library of functions used by application firmware to
 * utilize various ME features in a uniform manner
 * 
 */
#ifndef _NETWORK__INIT_H_
#define _NETWORK__INIT_H_

/** Includes required
 */
#include <stdint.h>
#include <nfp.h>

/** network_init_tm
 */
void network_tm_init(int nbi_island);

/** network_dma_init
 */
void network_dma_init(int nbi_island);

/** network_dma_init_buffer_list
 */
void network_dma_init_buffer_list(int nbi_island, int buffer_list, int num_buffers, uint64_t base, uint32_t stride );

int
network_dma_init_bp(int nbi_island, int buffer_pool, int bpe_start,
                    int ctm_offset, int split);

int
network_dma_init_bpe(int nbi_island, int buffer_pool, int bpe,
                     int ctm_island, int pkt_credit, int buf_credit);
void
network_dma_init_bp_complete(int nbi_island, int buffer_pool, int bpe);

/** network_ctm_init
 */
void network_ctm_cleanup(int ctm_island, int timeout);

/** network_ctm_init
 */
void network_ctm_init(int ctm_island, int pe_config);

/** network_npc_init
 */
void network_npc_init(int nbi_island);

/** network_npc_control
 */
void network_npc_control(int nbi_island, int enable_packets);

/** Close guard
 */
#endif /*_NFP__ME_H_ */
