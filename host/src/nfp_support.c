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
 * @file          nfp_support.c
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
#include <nfp.h>
#include <nfp_cpp.h>
#include <nfp_nffw.h>
#include "nfp_support.h"

/** struct pagemap_data
 */
struct pagemap_data {
    int   fd;
    int   page_size;
    long  huge_page_size;
};

/** struct nfp
 */
struct nfp {
    struct nfp *prev;
    struct nfp *next;
    struct pagemap_data pagemap;
    struct nfp_device *dev;
    struct nfp_cpp    *cpp;
    uint8_t firmware_id;
};

/** Statics
 */
static struct nfp *nfp_list;
static int exit_handler_registered=0;

/** read_file
 * Malloc a buffer and read the file in; return length of file, or -1 on error
 *
 * @param filename       Filename to load
 * @param file_data_ptr  Pointer to store address of mallocked file data buffer
 */
static int read_file(const char *filename, char **file_data_ptr)
{
    long file_len;
    char *file_data;

    FILE *f = fopen(filename, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    file_len=ftell(f);
    fseek(f, 0, SEEK_SET);

    file_data=malloc(file_len);
    if (!file_data) return -1;
    fread(file_data, file_len, 1, f);
    fclose(f);
    *file_data_ptr=file_data;
    return file_len;
}

/** exit_handler
 *
 * Shutdown all NFPs on the list
 *
 */
static void
exit_handler(void)
{
    while (nfp_list) {
        nfp_shutdown(nfp_list);
    }
}

/** nfp_link
 */
static void
nfp_link(struct nfp *nfp)
{
    struct nfp *n;
    for (n=nfp_list; n; n=n->next) {
        if (n==nfp) return;
    }

    nfp->next=nfp_list;
    if (nfp_list) nfp_list->prev=nfp;
    nfp_list = nfp;
    nfp->prev=NULL;
}

/** nfp_unlink
 */
static void
nfp_unlink(struct nfp *nfp)
{
    struct nfp **nfp_ptr;

    for (nfp_ptr=&nfp_list; *nfp_ptr; ) {
        if ((*nfp_ptr)==nfp) {
            *nfp_ptr=nfp->next;
            if (nfp->next) {
                nfp->next->prev=*nfp_ptr;
            }
        } else {
            nfp_ptr=&((*nfp_ptr)->next);
        }
    }
    nfp->next=NULL;
    nfp->prev=NULL;
}

/** nfp_init
 *
 * Initialize NFP structure, attaching to specified device number
 * Returns NULL on error, otherwise an allocated NFP structure
 * Adds atexit handler to shut down NFP cleanly at exit
 *
 * @param device_num   NFP device number to attach to
 */
struct nfp *
nfp_init(int device_num)
{
    struct nfp *nfp;
    nfp = malloc(sizeof(struct nfp));
    if (!nfp) return NULL;

    if (!exit_handler_registered) {
        exit_handler_registered=1;
        atexit(exit_handler);
    }
    nfp_link(nfp);

    nfp->pagemap.fd = -1;
    nfp->dev   = NULL;
    nfp->cpp   = NULL;

    nfp->pagemap.page_size      = getpagesize();
    nfp->pagemap.huge_page_size = gethugepagesize();
    nfp->pagemap.fd=open("/proc/self/pagemap", O_RDONLY);
    if (nfp->pagemap.fd<0) goto err;

    nfp->dev=nfp_device_open(device_num);
    if (!nfp->dev) goto err;
    nfp->cpp=nfp_device_cpp(nfp->dev);
    if (!nfp->cpp) goto err;
                       
    return nfp;

err:
    nfp_shutdown(nfp);
    return NULL;
}

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
void
nfp_shutdown(struct nfp *nfp)
{
    if (!nfp) return;
    if (nfp->dev) {
        nfp_unload(nfp);
        nfp_device_close(nfp->dev);
        nfp->dev = NULL;
    }
    nfp_unlink(nfp);
    free(nfp);
}

/** nfp_fw_load
 *
 * Load firmware from an 'nffw' file
 * Return 0 on success, <0 on failure
 *
 * @param nfp       NFP structure of NFP device to unload firmware for
 * @param filename  Full filename of file to load
 *
 */
int
nfp_fw_load(struct nfp *nfp, const char *filename)
{
    char *nffw;
    int nffw_size, err;

    nffw_size=read_file(filename,&nffw);
    if (nffw_size<0) return -1;
    err=nfp_nffw_load(nfp->dev, nffw, nffw_size, &nfp->firmware_id);
    free(nffw);
    return err;
}

/** nfp_fw_unload
 *
 * Unload any loaded firmware; if nothing loaded, then does nothing
 *
 * @param nfp    NFP structure of NFP device to unload firmware for
 *
 */
void
nfp_unload(struct nfp *nfp)
{
    if (!nfp->dev) return;
    nfp_nffw_info_acquire(nfp->dev);
    if (nfp_nffw_info_fw_loaded(nfp->dev)) {
        nfp_nffw_unload(nfp->dev,0);
    }
    nfp_nffw_info_release(nfp->dev);
}

/** nfp_fw_start
 *
 * Start any loaded firmware
 *
 * @param nfp    NFP structure of NFP device to load firmware for
 *
 */
int
nfp_fw_start(struct nfp *nfp)
{
    return nfp_nffw_start(nfp->dev,nfp->firmware_id);
}

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
long
nfp_huge_malloc(struct nfp *nfp, void **ptr, uint64_t *addr, long byte_size)
{
    int  num_huge_pages;
    long allocation_size;

    num_huge_pages = ((byte_size-1)/nfp->pagemap.huge_page_size)+1;
    allocation_size = num_huge_pages*nfp->pagemap.huge_page_size;

    *addr = 0;
    *ptr = get_huge_pages(allocation_size,GHP_DEFAULT);
    if (*ptr == NULL)
        return 0;

    ((uint64_t *)(*ptr))[0]=0;

    *addr = nfp_huge_physical_address(nfp, *ptr, 0);
    if (*addr == 0) {
        fprintf(stderr, "Failed to find linux page mapping in /proc/self/pagemap\n");
        free_huge_pages(*ptr);
        *ptr=NULL;
        return 0;
    }
    return allocation_size;
}

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
nfp_huge_physical_address(struct nfp *nfp, void *ptr, uint64_t ofs)
{
    uint64_t linux_pfn, linux_page_data;
    uint64_t addr;
    int err;

    /* Hack around with the internals of the pagemap file
       This is based on DPDK's huge page hacking
    */
    ptr = (void *)(((char *)ptr) + ofs);
    linux_pfn = ((uint64_t)ptr) / nfp->pagemap.page_size;
    err = (lseek(nfp->pagemap.fd, linux_pfn*sizeof(uint64_t),SEEK_SET)<0);
    if (!err) {
        err=(read(nfp->pagemap.fd, &linux_page_data, sizeof(uint64_t))<0);
    }
    if (!err) {
        err=(((linux_page_data>>63)&1)==0); /* page not present */
    }
    if (err) {
        return 0;
    }
    addr = (linux_page_data & (-1LL>>(64-55)))*nfp->pagemap.page_size;
    addr += ((uint64_t)ptr) % nfp->pagemap.page_size;
    return addr;
}

/** nfp_huge_free
 *
 * Free a hugepage allocation
 *
 * @param nfp        NFP structure already initialized
 * @param ptr        Huge page allocation previous returned by nfp_huge_malloc
 *
 */
void
nfp_huge_free(struct nfp *nfp, void *ptr)
{
    free_huge_pages(ptr);
}

/** nfp_show_rtsyms
 *
 * @param nfp      Nfp with loaded firmware whose run-time symbols are to be displayed
 *
 */
void
nfp_show_rtsyms(struct nfp *nfp)
{
    int i, num_symbols;
    const struct nfp_rtsym *rtsym;

    if ((!nfp) || (!nfp->dev)) return;
    nfp_rtsym_reload(nfp->dev);
    num_symbols=nfp_rtsym_count(nfp->dev);
    printf("Run-time symbol table has %d symbols\n",num_symbols);

    for (i=0; i<num_symbols; i++) {
        rtsym = nfp_rtsym_get(nfp->dev,i);
        printf("%d: %s\n",i,rtsym->name);
    }
}

/** nfp_get_rtsym_cppid
 *
 * Read a run-time symbol and derive a struct nfp_cppid to use for reading/writing
 *
 * @param nfp      Nfp with loaded firmware whose run-time symbols are to be interrogated
 * @param sym_nam  Symbol name
 * @param cppid    Structure to store result in, used for later read/write
 *
 */
int
nfp_get_rtsym_cppid(struct nfp *nfp, const char *sym_name, struct nfp_cppid *cppid)
{
    const struct nfp_rtsym *rtsym;

    if (!nfp->dev) return -1;

    if (!(rtsym=nfp_rtsym_lookup(nfp->dev,sym_name))) {
        fprintf(stderr, "Failed to find symbol '%s' in NFP symbol table\n",sym_name);
        return -1;
    }

    cppid->cpp_id = NFP_CPP_ISLAND_ID(rtsym->target, NFP_CPP_ACTION_RW, 0, rtsym->domain);
    cppid->addr   = rtsym->addr;
    return 0;
}

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
int
nfp_write(struct nfp *nfp, struct nfp_cppid *cppid, int offset, void *data, ssize_t size)
{
    fprintf(stderr,"Writing to %08x %016lx data %02x.%02x.%02x.%02x.%02x.%02x...\n",
            cppid->cpp_id, cppid->addr+offset,
            ((char *)data)[0],
            ((char *)data)[1],
            ((char *)data)[2],
            ((char *)data)[3],
            ((char *)data)[4],
            ((char *)data)[5] );
    if (nfp_cpp_write(nfp->cpp, cppid->cpp_id, cppid->addr+offset, data, size )==size)
        return 0;
    return -1;
}

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
int
nfp_read(struct nfp *nfp, struct nfp_cppid *cppid, int offset, void *data, ssize_t size)
{
    if (nfp_cpp_read(nfp->cpp, cppid->cpp_id, cppid->addr+offset, data, size )==size)
        return 0;
    return -1;
}
