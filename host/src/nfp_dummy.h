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
 * @file          nfp_dummy.h
 * @brief         NFP dummy library
 *
 * Header files required to bluff out the NFP, for example in an OSX
 * build
 *
 * On a Linux system with an NFP there will be support in the
 * libraries for huge pages and for the NFP firmware management. On a
 * system without an NFP these header files will not be there, and
 * neither will the librareis. This file provides function prototypes
 * for the functions that are missing on non-NFP systems, and
 * nfp_dummy.c provides implementations.
 *
 */

/*a Includes
 */
#include <stddef.h> 
#include <stdint.h> 

/*a Defines
 */
/** SHM_HUGETLB is provided by /usr/include/x86_64-linux-gnu/bits/shm.h normally **/
#define SHM_HUGETLB 0

/** GHP_DEFAULT is provided by /usr/include/hugetlbfs.h **/
#define GHP_DEFAULT 0

/** NFP_CPP_ISLAND_ID is provided by /opt/netronome/include/nfp-common/nfp_resid.h **/
#define NFP_CPP_ISLAND_ID(a,b,c,d) 0

/*a Structures
 */
/** struct nfp_rtsym is provided by /opt/netronome/include/nfp_nffw.h **/
struct nfp_rtsym {
    const char *name;
    int target;
    int domain;
    uint64_t addr;
};

/*a Functions from /usr/include/hugetlbfs.h
 */
/** Get the size of a huge page
 **/
int gethugepagesize(void);

/** Allocate a memory region of @p size bytes backed by huge pages; size must be @p gethugepagesize()
 **/
void *get_huge_pages(size_t size, int flags);

/** Free a huge page previous allocated by get_huge_pages
 **/
void free_huge_pages(void *ptr);

/*a Functions from /opt/netronome/include/nfp.h
 */
/** Open an NFP device given its number
 **/
struct nfp_device *nfp_device_open(int dev);

/** Close an open NFP device
 **/
void nfp_device_close(struct nfp_device *nfp);

/** Create an NFP CPP context for the NFP device
 **/
struct nfp_cpp *nfp_device_cpp(struct nfp_device *nfp);

/*a Functions from /opt/netronome/include/nfp_cpp.h
 */
/** Perform a CPP write - i.e. copy data from @p data, and perform a
 * CPP transaction of the specified size **/
int nfp_cpp_write(struct nfp_cpp *cpp, int cppid, uint64_t addr, void *data, int size);

/** Perform a CPP read - i.e. perform a CPP transaction of the
 * specified size and copy that size of data back to @p data **/
int nfp_cpp_read(struct nfp_cpp *cpp, int cppid, uint64_t addr, void *data, int size);

/*a Functions from /opt/netronome/include/nfp_nffw.h
 */
/** Load firmware into an NFP from a buffer in memory
 **/
int nfp_nffw_load(struct nfp_device *nfp, char *nffw, int nffw_size, uint8_t *fwid);

/** Unload previously loaded firmware from an NFP
 **/
int nfp_nffw_unload(struct nfp_device *nfp, uint8_t fwid);

/** Start a previously loaded firmware in an NFP
 **/
int nfp_nffw_start(struct nfp_device *nfp, uint8_t fwid);

/** Acquire access to the NFFW info for an NFP 
 **/
int nfp_nffw_info_acquire(struct nfp_device *nfp);

/** Determine if firmware is already loaded 
 **/
int nfp_nffw_info_fw_loaded(struct nfp_device *nfp);

/** Release access to the NFFW info
 **/
int nfp_nffw_info_release(struct nfp_device *nfp);

/** Reload the run-time symbol table in the host kernel from the NFP
 **/
void nfp_rtsym_reload(struct nfp_device *nfp);

/** Count symbols in the host kernel copy of the run-time symbol table
 **/
int nfp_rtsym_count(struct nfp_device *nfp);

/** Get a run-time symbol structure for the @p id'th symbol
 **/
const struct nfp_rtsym *nfp_rtsym_get(struct nfp_device *nfp, int id);

/** Lookup a run-time symbol structure from a symbol name
 **/
const struct nfp_rtsym *nfp_rtsym_lookup(struct nfp_device *nfp, const char *symname);
