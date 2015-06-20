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
 */

/** Includes
 */
#include "pktgen_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> 
#include <string.h> 

/** Defines
 */
#define MAX_MEMORIES 4
#define MAX_SIZE_TO_LOAD (2*1024*1024)

/** Enum regions
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
 */
struct pktgen_mem_region_allocation {
    struct pktgen_mem_region_allocation *next;
    uint32_t mu_base_s8; /* Base address in MU */
    uint64_t size;       /* Size in bytes */
};

/** struct pktgen_mem_region
 */
struct pktgen_mem_region {
    const char *filename;
    FILE *file;
    int required;
    uint64_t data_size;
    uint64_t size_allocated;
    uint64_t min_break_size;
    struct pktgen_mem_region_allocation *allocations;
};

/** struct pktgen_mem_layout
 */
struct pktgen_mem_layout {
    int num_pkt_data;
    const char *dirname;
    void *handle;
    pktgen_mem_alloc_callback alloc_callback;
    pktgen_mem_load_callback load_callback;
    struct pktgen_mem_alloc_hints *alloc_hints;
    struct pktgen_mem_region regions[MAX_REGIONS];
};

/** no_alloc_hints
 */
static struct pktgen_mem_alloc_hints no_alloc_hints[]={
    {PKTGEN_ALLOC_HINT_END,{{0,0}}},
};

/** layout_default_filenames
 */
static const char *layout_default_filenames[]={
    "sched", "script", "data",
    "data_1", "data_2", "data_3",
    "data_4", "data_5", "data_6",
};

/** open_file
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
 * Open a region from a file and set up for later allocation
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
    if ((region->file == NULL) && (region->required)) return 1;
    return 0;
}

/** region_load_data
 */
static char *
region_load_data(struct pktgen_mem_region *region,
                 uint64_t offset,
                 uint64_t size )
{
    char *mem;

    if (region->file == NULL) return NULL;

    if (size < 0)
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
 */
static int
add_region_allocation(struct pktgen_mem_region *region,
                      struct pktgen_mem_data *mem_data)
{
    struct pktgen_mem_region_allocation *alloc;
    struct pktgen_mem_region_allocation **prev;

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

            layout->alloc_callback(layout->handle,
                                   size,
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

/** patch_schedule
 *
 * @param layout   Memory layout previously allocated
 *
 * Patch up a schedule's packet pointers based on alloacted memory
 *
 */
static int
patch_schedule(struct pktgen_mem_layout *layout,
               struct pktgen_mem_region *region,
               char *mem)
{
//    struct pktgen_mem_region *sched;
//    int i;

//    sched = &layout->regions[REGION_SCHED];
    return 0;
}

/** load_region
 *
 * @param layout   Memory layout previously allocated
 *
 * Load a region into NFP memory
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
    while (alloc_size > 0) {
        char *mem_to_load;
        uint64_t size_to_load;
        struct pktgen_mem_data mem_data;

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

        layout->load_callback(layout->handle, layout, &mem_data);
        if (mem == NULL) {
            free(mem_to_load);
        }
        alloc_size -= size_to_load;
        offset += size_to_load;
    }
    return 0;
}

static int
load_region(struct pktgen_mem_layout *layout,
            int region_number)
{
    struct pktgen_mem_region *region;
    char *mem;
    uint64_t offset;
    struct pktgen_mem_region_allocation *allocation;

    region = &layout->regions[region_number];
    mem = NULL;
    if (region_number==REGION_SCHED) {
        mem = region_load_data(region, 0, -1);
        if (mem == NULL)
            return 1;

        patch_schedule(layout, region, mem);
    }

    offset = 0;
    allocation = region->allocations;
    while (offset<region->data_size) {
        int err;

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
                                                  struct pktgen_mem_alloc_hints *alloc_hints)
{
    struct pktgen_mem_layout *layout;
    int i;

    layout = malloc(sizeof(struct pktgen_mem_layout));
    if (layout == NULL)
        return NULL;

    layout->num_pkt_data=0;
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
        if (err != 0) return err;
        if (layout->alloc_hints[hint].hint_type == PKTGEN_ALLOC_HINT_END)
            break;
        hint++;
    }

    for (i=0; i<MAX_REGIONS; i++) {
        err = load_region(layout, i);
        if (err != 0) return err;
    }
    return err;
}

/** pktgen_mem_close
 */
extern void
pktgen_mem_close(struct pktgen_mem_layout *layout)
{
    int i;
    for (i=0; i<MAX_REGIONS; i++) {
        region_close(layout, &layout->regions[i]);
    }
}
