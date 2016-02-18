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
 * @file          pktgen_mem.c
 * @brief         Packet generator memory support
 *
 * This file provides functions to support loading of the memories in
 * an NFP for the packet generator, from a set of packet generator
 * files created (for example) in pktgen_lib.py.
 *
 * The set of files includes a schedule, a script, and at least one
 * packet data region.
 *
 * An instance of the 'pktgen_mem_layout' structure must be allocated,
 * and then filled by loading from a directory.
 *
 * The layout can then be allocated within the NFP, using an
 * allocation callback and hints for regions of the data to be placed
 * in suitable memories. A single region may be placed in more than
 * one memory of the NFP, being split up in no smaller than
 * 'min_break_size' pieces.
 *
 * After allocation, the structuce can be patched up - the schedule
 * particularly refers to absolute NFP memory addresses for packets,
 * and the addresses clearly depend on the allocation.
 *
 * After patching, the layout can be loaded into an NFP, with a
 * callback for each region of memory that needs to be loaded.
 *
 */

/** Includes
 */
#include "pktgen_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> 
#include <string.h> 
#include <inttypes.h>
#include <firmware/pktgen.h>

/** Defines
 */
#define MAX_MEMORIES 4
#define MAX_SIZE_TO_LOAD (2*1024*1024)
#define ERROR(f, args...)    fprintf(stderr,"pktgen_mem_error: " f, ## args)
#define VERBOSE(f, args...)  do {if (verbose) { printf("pktgen_mem_verbose:" f, ## args); }} while (0)

/** Enum regions
 * 
 * A packet generator memory load has one schedule region, one script
 * region (optional), and at least one packet data region. These are
 * loaded from files.
 *
 * The enumeration here is used in the layout structure and the
 * allocation hints.
 *
 */
enum {
    REGION_SCHED,
    REGION_SCRIPT,
    REGION_DATA,
    REGION_DATA_1,
    REGION_DATA_2,
    REGION_DATA_3,
    REGION_DATA_4,
    REGION_DATA_5,
    REGION_DATA_6,
    MAX_REGIONS
};

/** struct pktgen_mem_region_allocation
 *
 * A region allocation is an allocation in NFP memory for part of the
 * region. They are placed in a linked list in the order in which they
 * will be filled
 *
 */
struct pktgen_mem_region_allocation {
    struct pktgen_mem_region_allocation *next;
    uint32_t mu_base_s8; /* Base address in MU */
    uint64_t size;       /* Size in bytes */
};

/** struct pktgen_mem_region
 *
 * A region of a packet generator memory layout contains the contents
 * of a single file, be it the schedule, scripts, or one of the packet
 * data files.
 *
 */
struct pktgen_mem_region {
    const char *filename; /* Leaf filename for the region file */
    FILE *file;           /* File handle while leaf file is open */
    int required;         /* True if region is required (scrip, data0 */
    uint64_t data_size;   /* Size of region (file size) */
    uint64_t size_allocated; /* Total allocated size */
    uint64_t min_break_size; /* Minimum size the region may be split
                              * into */
    struct pktgen_mem_region_allocation *allocations; /* Linked list
                                                       * of
                                                       * allocations */
};

/** struct pktgen_mem_layout
 *
 * The structure describing a complete packet generator memory layout
 *
 */
struct pktgen_mem_layout {
    const char *dirname; /* Directory name to load region files from */
    void *handle;        /* Callback handle */
    pktgen_mem_alloc_callback alloc_callback; /* Allocation callback,
                                               * invoked to allocate
                                               * memory for the
                                               * region */
    pktgen_mem_load_callback load_callback;  /* Load callback, invoked
                                              * to load a region
                                              * allocation */
    struct pktgen_mem_alloc_hints *alloc_hints; /* Hints for
                                                 * allocation */
    struct pktgen_mem_region regions[MAX_REGIONS];
};

/** no_alloc_hints
 *
 * If no allocation hints are provided by the client, then this hint
 * set is used. It will preferentially place all the data in memory 0.
 *
 */
static struct pktgen_mem_alloc_hints no_alloc_hints[]={
    {PKTGEN_ALLOC_HINT_END,{{0,0}}},
};

/** layout_default_filenames
 *
 * Default filenames for the allocation regions
 *
 */
static const char *layout_default_filenames[]={
    "sched", "script", "data",
    "data_1", "data_2", "data_3",
    "data_4", "data_5", "data_6",
};

/** Statics
 */
static int verbose=0;

/** open_file
 *
 * Open a file given a directory name and filename.
 *
 * @param dirname    Directory name (ending with '/')
 * @param filename   Filename to load within directory
 *
 * Returns a file handle, or NULL on failure
 *
 */
static FILE *
open_file(const char *dirname, const char *filename)
{
    char *buf;
    FILE *f;

    if (dirname == NULL ) {
        buf = (char *)filename;
    } else {
        buf = malloc(strlen(dirname) + strlen(filename) + 2);
        sprintf(buf, "%s/%s", dirname, filename);
    }
    f = fopen(buf, "r");
    if (dirname != NULL) { free(buf); }
    return f;
}

/** file_size
 *
 * Find the size of an open file
 *
 * @param f
 *
 * Returns the size of an open file, of 0 if the file is closed
 *
 */
static uint64_t file_size(FILE *f)
{
    long l;
    if (!f) return 0L;
    fseek(f,0L,SEEK_END);
    l = ftell(f);
    fseek(f,0L,SEEK_SET);
    return l;
}

/** region_open
 *
 * Open a region from a file and sets it up for later allocation
 *
 * @param layout   Memory layout to open a region of
 * @param region   Region to open
 *
 */
static int
region_open(struct pktgen_mem_layout *layout,
            struct pktgen_mem_region *region)
{
    region->file = open_file(layout->dirname, region->filename);
    region->data_size = file_size(region->file);
    region->size_allocated = 0;
    region->allocations = NULL;
    if ((region->file == NULL) && (region->required)) {
        fprintf(stderr,"Failed to open data file %s %s\n",layout->dirname, region->filename);
        return 1;
    }
    VERBOSE("Region %s has size %"PRIx64"\n", region->filename, region->data_size);
    return 0;
}

/** region_load_data
 *
 * Load part (or all) of a region into memory, using malloc to
 * allocate space
 *
 * @param region   Region to load
 * @param offset   Offset from start of region to load
 * @param size     Size to load - if 0 then load the whole region
 *
 * Return NULL on error (if the region does not need to be loaded, or
 * if the malloc fails).
 * Return a malloc'ed buffer filled with the data on success.
 *
 */
static char *
region_load_data(struct pktgen_mem_region *region,
                 uint64_t offset,
                 uint64_t size )
{
    char *mem;

    VERBOSE("Loading region data %s:%"PRIx64":%"PRIx64"\n", region->filename, offset, size);
    if (region->file == NULL) return NULL;

    if (size == 0)
        size = region->data_size;

    mem = malloc(size);
    if (mem == NULL)
        return NULL;

    if (size + offset > region->data_size)
        size = region->data_size - offset;

    if (size<=0)
        return mem;

    fseek(region->file,offset,SEEK_SET);
    if (fread(mem,1,size,region->file) < size) {
        free(mem);
        return NULL;
    }
    return mem;
}

/** region_close
 *
 * Close a region, including its file
 *
 * @param layout   Memory layout to close a region of
 * @param region   Region to close
 *
 */
static void
region_close(struct pktgen_mem_layout *layout,
             struct pktgen_mem_region *region)
{
    if (region->file != NULL) {
        fclose(region->file);
        region->file = NULL;
    }
}

/** add_region_allocation
 *
 * Add an NFP memory allocation to a region (at the end of the current
 * allocations).
 *
 * @param region   Region to add allocation to
 * @param mem_data Allocation to add to the region
 *
 */
static int
add_region_allocation(struct pktgen_mem_region *region,
                      struct pktgen_mem_data *mem_data)
{
    struct pktgen_mem_region_allocation *alloc;
    struct pktgen_mem_region_allocation **prev;

    if (mem_data->size == 0)
        return 0;

    VERBOSE("Adding allocation for region %s of size %"PRIx64" base %08x00\n", region->filename, mem_data->size, mem_data->mu_base_s8);

    alloc = malloc(sizeof(*alloc));
    if (alloc == NULL) return 1;

    prev = &(region->allocations);
    while ((*prev) != NULL) {
        prev = &( (*prev)->next );
    }
    *prev = alloc;
    alloc->next = NULL;
    alloc->mu_base_s8 = mem_data->mu_base_s8;
    alloc->size = mem_data->size;
    region->size_allocated += mem_data->size;
    return 0;
}

/** alloc_regions_with_hint
 *
 * Allocate regions using one allocation hint
 *
 * @param layout  Memory layout to allocate regions for
 * @param hint    Hint to use
 *
 * Return non-zero on error, zero on success
 *
 * Allocate the regions specified by the hint in the memories
 * specified, and the size specified.
 *
 * A PKTGEN_ALLOC_HINT_END implies allocating all regions in all
 * memories with 16GB per memory.
 *
 */
static int
alloc_regions_with_hint(struct pktgen_mem_layout *layout,
                        struct pktgen_mem_alloc_hints *hint)
{
    int region_mask = -1;
    int memory_mask = -1;
    uint64_t size = 1ULL << 34;
    int i, j;
    int err;

    if (hint->hint_type == PKTGEN_ALLOC_HINT_BALANCE_PACKETS) {
        region_mask = 1 << hint->balance.region;
        memory_mask = hint->balance.memory_mask;
        size = hint->balance.size;
    }

    err = 0;
    memory_mask &= (1 << MAX_MEMORIES) - 1;
    for (i=0; i<MAX_REGIONS; i++) {
        if ((region_mask >> i) & 1) {
            struct pktgen_mem_region *region;
            struct pktgen_mem_data mem_data[MAX_MEMORIES];
            uint64_t size_to_alloc;

            region = &layout->regions[i];
            size_to_alloc = region->data_size - region->size_allocated;
            if (size_to_alloc <= 0)
                continue;
            if (size_to_alloc > size)
                size_to_alloc = size;

            for (j=0; j<MAX_MEMORIES; j++) {
                mem_data[j].size = 0;
            }
            layout->alloc_callback(layout->handle,
                                   size_to_alloc,
                                   region->min_break_size,
                                   memory_mask,
                                   mem_data);
            for (j=0; j<MAX_MEMORIES; j++) {
                if ((memory_mask >> j) & 1) {
                    err |= add_region_allocation(region, &mem_data[j]);
                }
            }
        }
        if (err != 0) return err;
    }
    return 0;
}

/** pktgen_mem_get_mu
 */
uint64_t
pktgen_mem_get_mu(struct pktgen_mem_layout *layout,
                  int region,
                  uint64_t ofs)
{
    struct pktgen_mem_region_allocation *alloc;

    if (region >= MAX_REGIONS) {
        ERROR("Region %d out of range when finding its allocation\n", region);
        return 0;
    }
    if (layout->regions[region].data_size == 0) {
        ERROR("Region %d empty when looking for allocation\n", region);
        return 0;
    }

    alloc = layout->regions[region].allocations;
    while (alloc) {
        if (alloc->size > ofs)
            break;
        ofs = ofs - alloc->size;
        alloc = alloc->next;
    }
    if (!alloc) {
        ERROR("Region %d has not got enough allocation for %08"PRIx64"\n",
              region,
              ofs);
        return 0;
    }
    return (((uint64_t)alloc->mu_base_s8) << 8) + ofs;
}

/** find_data_region_allocation
 */
static uint64_t
find_data_region_allocation(struct pktgen_mem_layout *layout,
                            int data_region,
                            uint32_t region_offset_s8)
{
    return pktgen_mem_get_mu(layout, data_region+REGION_DATA, region_offset_s8<<8);
}

/** patch_schedule
 *
 * Patch up a schedule's packet pointers based on alloacted memory. 
 *
 * @param layout  Memory layout previously allocated
 * @param region  Schedule region to patch
 * @param mem     Memory containing the schedule
 *
 * Invoked just prior to loading the schedule region.
 *
 */
static int
patch_schedule(struct pktgen_mem_layout *layout,
               struct pktgen_mem_region *region,
               char *mem)
{
    int i;
    struct pktgen_sched_entry *sched_entry;
    for (i=64; i<region->data_size; i+=sizeof(*sched_entry)) {
        int data_region;
        uint32_t region_offset_s8;
        uint32_t mu_base_s8;
        sched_entry = (struct pktgen_sched_entry *)(mem + i);
        if (sched_entry->mu_base_s8 != 0) {
            data_region   = sched_entry->mu_base_s8 >> 28;
            region_offset_s8 = sched_entry->mu_base_s8 & 0xfffffff;
            mu_base_s8 = find_data_region_allocation(layout, data_region, region_offset_s8) >> 8;
            if (mu_base_s8 == 0)
                return 1;
            VERBOSE("%d: %d %d %08x00 %08x00\n", i,
                   sched_entry->tx_time_lo,
                   sched_entry->length,
                   sched_entry->mu_base_s8,
                   mu_base_s8);
            sched_entry->mu_base_s8 = mu_base_s8;
        }
    }
    return 0;
}

/** load_allocation
 *
 * Load an allocatin of a region into NFP memory
 *
 * @param layout      Memory layout
 * @param region      Region of which the allocation is for
 * @param allocation  Allocation of region of layout to load
 * @param offset      Offset within region that allocation starts at
 * @param mem         Memory containing region data, or NULL if not
 *                    loaded as yet
 *
 * Return non-zero on error, zero on success
 *
 */
static int
load_allocation(struct pktgen_mem_layout *layout,
                struct pktgen_mem_region *region,
                struct pktgen_mem_region_allocation *allocation,
                uint64_t offset,
                char *mem )
{
    uint64_t alloc_size;

    alloc_size = allocation->size;
    VERBOSE("Loading allocation %s:%"PRIx64"\n", region->filename, allocation->size);

    while (alloc_size > 0) {
        char *mem_to_load;
        uint64_t size_to_load;
        struct pktgen_mem_data mem_data;
        int err;

        size_to_load = alloc_size;
        if (size_to_load > MAX_SIZE_TO_LOAD)
            size_to_load = MAX_SIZE_TO_LOAD;

        if (mem==NULL) {
            mem_to_load = region_load_data(region,
                                           offset,
                                           size_to_load);
            if (mem_to_load == NULL)
                return 1;
        } else {
            mem_to_load = mem + offset;
        }
        mem_data.base = mem_to_load;
        mem_data.size = size_to_load;
        mem_data.mu_base_s8 = allocation->mu_base_s8 + (offset >> 8);

        err = layout->load_callback(layout->handle, layout, &mem_data);
        if (err != 0)
            return err;

        if (mem == NULL) {
            free(mem_to_load);
        }
        alloc_size -= size_to_load;
        offset += size_to_load;
    }
    return 0;
}

/** load_region
 *
 * Load a region into NFP memory, patching as necessary first
 *
 * @param layout      Memory layout
 * @param region_number  Number of region within layout to load
 *
 * Return non-zero on error, zero on success
 *
 */
static int
load_region(struct pktgen_mem_layout *layout,
            int region_number)
{
    struct pktgen_mem_region *region;
    char *mem;
    uint64_t offset;
    struct pktgen_mem_region_allocation *allocation;
    int err;

    region = &layout->regions[region_number];
    mem = NULL;
    if (region_number==REGION_SCHED) {
        mem = region_load_data(region, 0, 0);
        VERBOSE("Loading scheduler data returned %p\n", mem);
        if (mem == NULL)
            return 1;

        err = patch_schedule(layout, region, mem);
        if (err != 0) {
            free(mem);
            return err;
        }
    }

    offset = 0;
    allocation = region->allocations;
    while (offset<region->data_size) {
        VERBOSE("Loading allocation %s %"PRIx64"\n",
                region->filename, offset);

        if (allocation==NULL) {
            return 1;
        }
        err = load_allocation(layout, region, allocation, offset, mem);
        if (err != 0)
            return err;

        offset += allocation->size;
        allocation = allocation->next;
    }
    if (mem != NULL) {
        free(mem);
    }
    return 0;
}

/** pktgen_mem_alloc
 *
 * Allocate a packet generator memory layout structure
 *
 * @param handle          Handle for callbacks
 * @param alloc_callback  Callback invoked to allocate MU for a structure
 * @param load_callback   Callback invoked to load data into the NFP
 * @param alloc_hints     Allocation hint array used to split up packet data
 *
 * Return an allocated memory layout
 *
 */
extern struct pktgen_mem_layout *pktgen_mem_alloc(void *handle,
                                                  pktgen_mem_alloc_callback alloc_callback,
                                                  pktgen_mem_load_callback load_callback,
                                                  struct pktgen_mem_alloc_hints *alloc_hints)
{
    struct pktgen_mem_layout *layout;
    int i;

    layout = malloc(sizeof(struct pktgen_mem_layout));
    if (layout == NULL)
        return NULL;

    layout->dirname = NULL;
    layout->handle = handle;
    layout->alloc_callback = alloc_callback;
    layout->load_callback  = load_callback;
    layout->alloc_hints = alloc_hints;

    if (alloc_hints == NULL) {
        layout->alloc_hints = no_alloc_hints;
    }

    for (i=0; i<MAX_REGIONS; i++) {
        layout->regions[i].filename = layout_default_filenames[i];
        layout->regions[i].file = NULL;
        layout->regions[i].data_size = 0;
        layout->regions[i].size_allocated = 0;
        layout->regions[i].required = 0;
    }
    layout->regions[REGION_SCHED].required = 1;
    layout->regions[REGION_DATA].required = 1;
    return layout;
}

/** pktgen_mem_open_directory
 *
 * @param layout   Memory layout previously allocated
 * @param dirname  Directory name containing files to load
 *
 * Returns 0 on success, non-zero on error
 *
 * Open a packet generator memory contents directory, and determine
 * the NFP memory requirements for it
 *
 */
extern int
pktgen_mem_open_directory(struct pktgen_mem_layout *layout,
                          const char *dirname)
{
    int err;
    int i;

    layout->dirname = dirname;

    err = 0;
    for (i=0; i<MAX_REGIONS; i++) {
        err |= region_open(layout, &layout->regions[i]);
    }
    return err;
}

/** pktgen_mem_load
 *
 * @param layout   Memory layout previously allocated
 *
 * Allocate memory required for the layout, and load memory onto the NFP
 *
 */
extern int
pktgen_mem_load(struct pktgen_mem_layout *layout)
{
    int hint;
    int err;
    int i;

    hint = 0;
    for (;;) {
        err = alloc_regions_with_hint(layout, &layout->alloc_hints[hint]);
        VERBOSE("Alloc regions returned %d\n",err);
        if (err != 0) return err;
        if (layout->alloc_hints[hint].hint_type == PKTGEN_ALLOC_HINT_END)
            break;
        hint++;
    }

    for (i=0; i<MAX_REGIONS; i++) {
        err = load_region(layout, i);
        VERBOSE("Load region returned %d\n",err);
        if (err != 0) return err;
    }
    return err;
}

/** pktgen_mem_close
 *
 * @param layout   Memory layout previously allocated
 *
 * Close and deallocate a layout
 *
 */
extern void
pktgen_mem_close(struct pktgen_mem_layout *layout)
{
    int i;
    for (i=0; i<MAX_REGIONS; i++) {
        region_close(layout, &layout->regions[i]);
    }
    free(layout);
}
