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
 * @file          nfp_support.h
 * @brief         NFP support library
 *
 */

/** Includes
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> 
#include <stddef.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h>
#include <unistd.h>
#include <hugetlbfs.h>

/** struct nfp_cppid
 */
struct nfp_cppid {
    uint32_t cpp_id;
    uint64_t addr;
};

/** nfp_init
 *
 * Initialize NFP structure, attaching to specified device number
 * Returns NULL on error, otherwise an allocated NFP structure
 * Adds atexit handler to shut down NFP cleanly at exit
 *
 * @param device_num   NFP device number to attach to
 */
extern struct nfp *nfp_init(int device_num);

/** nfp_shutdown
 *
 * Shutdown the NFP, unloading firmware before closing the device
 * Performs an incremental shutdown of the activated components, and can
 * be performed many times successively without failure
 * Removes the NFP from the list to be shutdown at exit
 *
 * @param nfp    NFP structure of device to shut down
 *
 */
extern void nfp_shutdown(struct nfp *nfp);

/** nfp_fw_load
 *
 * Load firmware from an 'nffw' file
 * Return 0 on success, <0 on failure
 *
 * @param nfp       NFP structure of NFP device to unload firmware for
 * @param filename  Full filename of file to load
 *
 */
extern int nfp_fw_load(struct nfp *nfp, const char *filename);

/** nfp_fw_unload
 *
 * Unload any loaded firmware; if nothing loaded, then does nothing
 *
 * @param nfp    NFP structure of NFP device to unload firmware for
 *
 */
extern void nfp_unload(struct nfp *nfp);

/** nfp_fw_start
 *
 * Start any loaded firmware
 *
 * @param nfp    NFP structure of NFP device to load firmware for
 *
 */
extern int nfp_fw_start(struct nfp *nfp);

/** nfp_huge_malloc
 *
 * Malloc using hugepages, get physical address and void *,
 * and return number of bytes actually allocated
 *
 * @param nfp        NFP structure already initialized
 * @param ptr        Pointer to store (virtual) allocated memory ptr in
 * @param addr       Location to store physical address of allocated memory
 * @param byte_size  Byte size to allocate
 *
 */
extern long nfp_huge_malloc(struct nfp *nfp, void **ptr, uint64_t *addr, long byte_size);

/** nfp_huge_physical_address
 *
 * Find physical address of an offset into a huge malloc region
 *
 * @param nfp   NFP structure already initialized
 * @param ptr   Previously nfp_huge_malloc pointer
 * @param ofs   Offset from pointer to find address
 *
 */
uint64_t
nfp_huge_physical_address(struct nfp *nfp, void *ptr, uint64_t ofs);

/** nfp_huge_free
 *
 * Free a hugepage allocation
 *
 * @param nfp        NFP structure already initialized
 * @param ptr        Huge page allocation previous returned by nfp_huge_malloc
 *
 */
extern void nfp_huge_free(struct nfp *nfp, void *ptr);

/** nfp_show_rtsyms
 *
 * @param nfp      Nfp with loaded firmware whose run-time symbols are to be displayed
 *
 */
extern void nfp_show_rtsyms(struct nfp *nfp);

/** nfp_get_rtsym_cppid
 *
 * Read a run-time symbol and derive a struct nfp_cppid to use for reading/writing
 *
 * @param nfp      Nfp with loaded firmware whose run-time symbols are to be interrogated
 * @param sym_nam  Symbol name
 * @param cppid    Structure to store result in, used for later read/write
 *
 */
extern int nfp_get_rtsym_cppid(struct nfp *nfp,
                               const char *sym_name, struct nfp_cppid *cppid);

/** nfp_write
 *
 * Write data to an NFP memory or register
 *
 * @param nfp      Nfp structure
 * @param cppid    nfp_cppid structure filled in, for example, by nfp_get_rtsym_cppid
 * @param offset   address offset from start of CPP ID to write at (may be outside symbol)
 * @param data     Data to write
 * @param size     Size in bytes of data to write
 *
 */
extern int nfp_write(struct nfp *nfp, struct nfp_cppid *cppid, int offset, void *data, ssize_t size);

/** nfp_read
 *
 * Read data from an NFP memory or register
 *
 * @param nfp      Nfp structure
 * @param cppid    nfp_cppid structure filled in, for example, by nfp_get_rtsym_cppid
 * @param offset   address offset from start of CPP ID to write at (may be outside symbol)
 * @param data     Data buffer to place read data into
 * @param size     Size in bytes of data to read
 *
 */
extern int nfp_read(struct nfp *nfp, struct nfp_cppid *cppid, int offset, void *data, ssize_t size);
