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
 * @file          pktgen_mem.h
 * @brief         Packet generator memory support
 *
 *
 * This set of library functions supports the packet generator firmware
 * and its memory initialization. It provides functions for loading and
 * populating the NFP memories as required by the packet generator
 * firmware.
 * 
 * The memory layout consists of a schedule, a set of scripts, and
 * regions of packet data.
 *
 * The schedule contains sets of flow-packet entries, each entry of
 * which contains a transmit time, packet MU address (including
 * island), length, flags and a script offset.
 *
 * The file/data to be loaded consists of a schedule file, a script
 * file, and at least one packet data file.
 *
 * The loading process requires allocation of the NFP memory, and
 * fixing-up of packet addresses from the schedule to match the actual
 * memory addresss of packet data.
 * 
 */

/** Includes
 */
#include <stdint.h> 

/** struct pktgen_mem_layout
 */
struct pktgen_mem_layout;

/** struct pktgen_mem_data
 */
struct pktgen_mem_data {
    const char *base; /* Base of data to transfer */
    uint32_t    mu_base_s8;
    uint64_t    size;
};

/** pktgen_mem_alloc_hints hint types
 */
enum {
    PKTGEN_ALLOC_HINT_END=0,
    PKTGEN_ALLOC_HINT_BALANCE_PACKETS,
};

/** struct pktgen_mem_alloc_hints
 */
struct pktgen_mem_alloc_hints {
    int hint_type; /* Enumerated hint type, last in array must be
                      PKTGEN_ALLOC_HINT_END */
    union {
        /** a */
        struct {
        /** a */
            int region; /* Which region to hint for (schedule/script/data N) */
        /** a */
            int memory_mask; /* Mask of memories to spread packets across */
        /** a */
            uint64_t size; /* Size in bytes per memory to spread */
        } balance;
    };
};

/** pktgen_mem_load_callback
 */
typedef int (*pktgen_mem_load_callback)(void *handle,
                                       struct pktgen_mem_layout *layout,
                                       struct pktgen_mem_data *data);
/** pktgen_mem_alloc_callback
 */
typedef int (*pktgen_mem_alloc_callback)(void *handle,
                                         uint64_t size,
                                         uint64_t min_break_size,
                                         int memory_mask,
                                         struct pktgen_mem_data *data);

/** pktgen_mem_alloc
 *
 * @param handle          Handle for callbacks
 * @param alloc_callback  Callback invoked to allocate MU for a structure
 * @param load_callback   Callback invoked to load data into the NFP
 * @param alloc_hints     Allocation hint array used to split up packet data
 *
 * Return an allocated memory layout
 *
 * Allocate a packet generator memory layout structure
 *
 */
extern struct pktgen_mem_layout *pktgen_mem_alloc(void *handle,
                                                  pktgen_mem_alloc_callback alloc_callback,
                                                  pktgen_mem_load_callback load_callback,
                                                  struct pktgen_mem_alloc_hints *alloc_hints);

/** pktgen_mem_open_directory
 *
 * @param layout   Memory layout previously allocated
 * @param dirname  Directory with the schedule, script and packet data
 *
 * Returns 0 on success, non-zero on error
 *
 * Open a packet generator memory contents directory, and determine
 * the memory requirements for it
 *
 */
extern int pktgen_mem_open_directory(struct pktgen_mem_layout *layout,
                                     const char *dirname);

/** pktgen_mem_load
 *
 * @param layout   Memory layout previously allocated
 *
 * Allocate memory required for the layout, and load memory onto the NFP
 *
 */
extern int pktgen_mem_load(struct pktgen_mem_layout *layout);

/** pktgen_mem_close
 *
 * @param layout   Memory layout previously allocated
 *
 * Close and free a packet generator memory layout
 *
 */
extern void pktgen_mem_close(struct pktgen_mem_layout *layout);

/** pktgen_mem_get_mu
 *
 * @param layout   Memory layout previously allocated
 * @param region   Memory region number to find mu_base of
 * @param ofs      Offset into region to find mu_base of
 *
 * Return the MU address of the offset in the region
 *
 */
extern uint64_t pktgen_mem_get_mu(struct pktgen_mem_layout *layout,
                                  int region,
                                  uint64_t ofs );
