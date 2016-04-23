/** Copyright (C) 2015-2016,  Gavin J Stark.  All rights reserved.
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
 * This library abstracts some of the Netronome NFP library calls to
 * make host applications a little simpler.
 *
 * For example, the NFP device opening and closing is wrapped in the
 * library, adding atexit handlers and so on, so that firmware and
 * NFPs are handled cleanly.
 *
 */

/*a Includes
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> 
#include <stddef.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h>
#include <unistd.h>
#ifndef DUMMY_NFP
#include <hugetlbfs.h>
#endif

/*a Types
 */
/*f struct nfp_cppid */
/** Structure to contain the required CPP data for access a run-time
 * symbol, or other on-chip resource
 */
struct nfp_cppid {
    /** Encoding of CPP target, action, token etc **/
    uint32_t cpp_id;
    /** CPP-specific address of the resource **/
    uint64_t addr;
};

/*a Functions
 */
/*f nfp_init */
/**
 * @brief Initialize an NFP structure for use, attaching to an NFP if
 * required
 *
 * @param device_num   NFP device number to attach to (-1 => none)
 *
 * @param sig_term     Set to 1 if a signal handler for SIGTERM
 * should be installed
 *
 * @returns NULL on error, otherwise an allocated NFP structure
 *
 * Initialize NFP structure, attaching to specified device number
 *
 * For clients that do not interact directly with an NFP the device
 * number should be -1. The device number should only be non-negative
 * for a server thread that is to directly interact with an NFP (for
 * example to load firmware). (Many client interactions with an NFP
 * will take place using host memory alone.)
 * 
 * Adds atexit handler to shut down NFP cleanly at exit if required.
 *
 */
extern struct nfp *nfp_init(int device_num, int sig_term);

/*f nfp_shutdown */
/**
 * @brief Shutdown the NFP chip structure, making it available for others
 *
 * @param nfp    NFP structure of device to shut down
 *
 * Shutdown the NFP, unloading firmware before closing the device
 * Performs an incremental shutdown of the activated components, and can
 * be performed many times successively without failure
 * Removes the NFP from the list to be shutdown at exit
 *
 */
extern void nfp_shutdown(struct nfp *nfp);

/*f nfp_fw_load */
/**
 * @brief Load firmware onto an NFP, without starting it
 *
 * @param nfp       NFP structure of NFP device to unload firmware for
 *
 * @param filename  Full filename of file to load
 *
 * @returns 0 on success, <0 on failure
 *
 * Load firmware from an 'nffw' file
 *
 */
extern int nfp_fw_load(struct nfp *nfp, const char *filename);

/*f nfp_fw_unload */
/**
 *
 * @brief Unload firmware from an NFP
 *
 * @param nfp    NFP structure of NFP device to unload firmware for
 *
 * Unload any loaded firmware; if no firmware has been loaded then
 * this does nothing
 *
 */
extern void nfp_fw_unload(struct nfp *nfp);

/*f nfp_fw_start */
/**
 * @brief Start firmware that has been loaded
 *
 * @param nfp    NFP structure of NFP device that has firmware loaded
 *
 * @returns 0 on success, -1 on error
 *
 * Start all the firmware that has been loaded
 *
 */
extern int nfp_fw_start(struct nfp *nfp);

/*f nfp_shm_alloc */
/**
 *
 * @brief Allocate some shared memory (one area per NFP)
 *
 * @param nfp           NFP structure of NFP device to allocate SHM for
 *
 * @param shm_filename  Filename for a shared memory 'lock' file
 *
 * @param shm_key       32-bit key used with filename for SHM 'key'
 *
 * @param size          Size in bytes of memory to create (if create is non-zero)
 *
 * @param create        Non-zero if SHM should be created if it does not exist
 *
 * @returns Number of bytes allocated in shared memory
 *
 * This function allocates shared memory of @p size bytes, using
 * the @p shm_filename and @p shm_key to define a system-wide shared memory
 * handle so that multiple processes may share the same memory.
 *
 * Only a single shared memory is permitted per NFP structure.
 *
 * Generally the server will invoke this call with @p create set to a
 * vaue of 1. Clients then start up, connect to the server, and call
 * this function with create set to 0 (but the rest of the arguments
 * the same, except size is ignored)
 *
 */
extern int nfp_shm_alloc(struct nfp *nfp, const char *shm_filename,
                         int shm_key, size_t size, int create);

/*f nfp_shm_data */
/**
 * @brief Get pointer to NFP SHM data allocated with @p nfp_shm_alloc
 *
 * @param nfp           NFP structure of NFP device to get SHM pointer of
 *
 * @returns Pointer to (process virtual) address of shared memory
 * allocated by @p nfp_shm_alloc
 *
 * Get SHM data pointer after it has been allocated
 *
 */
extern void *nfp_shm_data(struct nfp *nfp);

/*f nfp_shm_close */
/**
 *
 * @brief Close the shared memory corresponding to the NFP device
 *
 * @param nfp           NFP structure of NFP device to close SHM for
 *
 * Close the shared memory previousa allocated with @p nfp_shm_alloc
 *
 */
extern void nfp_shm_close(struct nfp *nfp);

/*f nfp_huge_malloc */
/**
 *
 * @brief Malloc using hugepages and get pointer to it
 *
 * @param nfp        NFP structure already initialized
 *
 * @param ptr        Pointer to store (process virtual) allocated memory ptr in
 *
 * @param byte_size  Byte size to allocate
 *
 * @returns amount of memory allocated
 *
 * Allocate huge pages with @p get_huge_pages, and ensure it is mapped.
 *
 */
extern int nfp_huge_malloc(struct nfp *nfp, void **ptr, size_t byte_size);

/*f nfp_huge_physical_address */
/**
 *
 * @brief Find physical address of an offset into a huge malloc region
 *
 * @param nfp   NFP structure already initialized
 *
 * @param ptr   Previously nfp_huge_malloc pointer
 *
 * @param ofs   Offset from pointer to find address
 *
 * This function for Ubuntu LTS14.04 uses a /proc/self/pagemap hack to
 * find the physical address for a process virtual address
 *
 */
uint64_t
nfp_huge_physical_address(struct nfp *nfp, void *ptr, uint64_t ofs);

/*f nfp_huge_free */
/**
 *
 * @brief Free a hugepage allocation
 *
 * @param nfp        NFP structure already initialized
 *
 * @param ptr        Huge page allocation previous returned by nfp_huge_malloc
 *
 * Free a huge page allocation previous allocated by @p nfp_huge_malloc
 */
extern void nfp_huge_free(struct nfp *nfp, void *ptr);

/*f nfp_show_rtsyms */
/*
 * @brief Display run-time symbols for NFP, for debug
 *
 * @param nfp      Nfp with loaded firmware whose run-time symbols are to be displayed
 *
 * Display the run-time symbols from firmware loaded on and NFP to
 * stdout, for debug purposes.
 *
 */
extern void nfp_show_rtsyms(struct nfp *nfp);

/*f nfp_get_rtsym_cppid */
/**
 *
 * @brief Read a run-time symbol and fill an nfp_cppid for accessing it
 *
 * @param nfp      Nfp with loaded firmware whose run-time symbols are to be interrogated
 *
 * @param sym_nam  Symbol name
 *
 * @param cppid    Structure to store result in, used for later read/write
 *
 * @returns -1 on failure (e.g. symbol not found), 0 for success
 *
 * Read a run-time symbol and derive a struct nfp_cppid to use for
 * reading/writing. The firmware need not have been started, just
 * loaded.
 *
 */
extern int nfp_get_rtsym_cppid(struct nfp *nfp,
                               const char *sym_name, struct nfp_cppid *cppid);

/*f nfp_write */
/**
 *
 * @brief Write data to an NFP memory or register
 *
 * @param nfp      Nfp structure
 *
 * @param cppid    nfp_cppid structure filled in, for example, by nfp_get_rtsym_cppid
 *
 * @param offset   address offset from start of CPP ID to write at (may be outside symbol)
 *
 * @param data     Data to write
 *
 * @param size     Size in bytes of data to write
 *
 * @returns 0 on success, -1 on error
 *
 *
 */
extern int nfp_write(struct nfp *nfp, struct nfp_cppid *cppid, int offset, void *data, ssize_t size);

/*f nfp_read */
/**
 *
 * @brief Read data from an NFP memory or register
 *
 * @param nfp      Nfp structure
 *
 * @param cppid    nfp_cppid structure filled in, for example, by nfp_get_rtsym_cppid
 *
 * @param offset   address offset from start of CPP ID to write at (may be outside symbol)
 *
 * @param data     Data buffer to place read data into
 *
 * @param size     Size in bytes of data to read
 *
 * @returns 0 on success, -1 on error
 *
  */
extern int nfp_read(struct nfp *nfp, struct nfp_cppid *cppid, int offset, void *data, ssize_t size);
