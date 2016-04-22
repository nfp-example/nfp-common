/*a Copyright */
/** Copyright (C) 2016,  Gavin J Stark.  All rights reserved.
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
 * @file   include/firmware/data_coproc.h
 * @brief  Data coprocessor firmware include shared with host
 *
 * This header file contains data structures that are shared by the
 * host and data coprocessing firmware, particularly the work items
 * and the content of the NFP write pointer memory that is written by
 * the host to indicate more work items are ready.
 *
 */

/*a Includes
 */
#include <stdint.h> 

/*a Defines
 */
#define DCPRC_MAX_WORKQS 64

#ifdef __NFCC_VERSION
#ifndef __DATA_BIG_ENDIAN
#define __DATA_BIG_ENDIAN
#endif
#endif

/*a Types
 */
/*t struct workq_entry */
/**
 *
 * Work queue entry; the work queue in host memory is populated by
 * these structures
 *
 * The work assumes it has a host physical address for the work item
 * data, plus additional operands, plus the 'valid' indication that is
 * set when the NFP is permitted to read/write the workq_entry, and
 * clear when the host is permitted to read/write the workq_entry
 *
 * For the NFCC version the data is big-endian, else assumed to be
 * little-endian
 */
#ifdef __DATA_BIG_ENDIAN
struct workq_entry {
    uint32_t host_physical_address_lo;
    uint32_t host_physical_address_hi;
    uint32_t operand_0;
    uint32_t operand_1:31;
    uint32_t valid_work:1;
};
#else
struct workq_entry {
    uint64_t host_physical_address;
    uint32_t operand_0;
    uint32_t valid_work:1;
    uint32_t operand_1:31;
};
#endif

/** struct workq_buffer_desc
 *
 * Host workq circular buffer information - base address, size, and write pointer
 *
 * These should be reset to 0 on firmware loading, and configured by the host
 *
 * Note that this structure is 16B long. DO NOT CHANGE THIS.
 */
#ifdef __DATA_BIG_ENDIAN
struct workq_buffer_desc {
    uint32_t host_physical_address_lo;
    uint32_t host_physical_address_hi;
    uint32_t max_entries;
    uint32_t wptr;
};
#else
struct workq_buffer_desc {
    uint64_t host_physical_address;
    uint32_t max_entries;
    uint32_t wptr;
};
#endif

/** struct cls_workq
 */
struct cls_workq {
    struct workq_buffer_desc workqs[DCPRC_MAX_WORKQS];
};

