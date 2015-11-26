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
 * @file          nfp_dummy.c
 * @brief         NFP dummy library
 *
 * Code files required to bluff out the NFP
 *
 */

/** Includes
 */
#include <stddef.h> 
#include <stdint.h> 
#include "nfp_dummy.h"

struct nfp_device {
    int dummy;
};
struct nfp_device dummy_nfp_device;
struct nfp_cpp {
    int dummy;
};
struct nfp_cpp dummy_nfp_cpp;

/** gethugepagesize
 */
int
gethugepagesize(void)
{
    return 1<<20;
}

/** get_huge_pages
 */
void *
get_huge_pages(size_t size, int flags)
{
    return NULL;
}

/** free_huge_pages
 */
void
free_huge_pages(void *ptr)
{
}

/** nfp_device_open
 */
struct nfp_device *
nfp_device_open(int dev)
{
    return &dummy_nfp_device;
}

/** nfp_device_close
 */
void
nfp_device_close(struct nfp_device *nfp)
{
}

/** nfp_device_cpp
 */
struct nfp_cpp *
nfp_device_cpp(struct nfp_device *nfp)
{
    return &dummy_nfp_cpp;
}

/** nfp_nffw_load
 */
int
nfp_nffw_load(struct nfp_device *nfp, char *nffw, int nffw_size, uint8_t *fwid)
{
    return 0;
}

/** nfp_nffw_unload
 */
int
nfp_nffw_unload(struct nfp_device *nfp, uint8_t fwid)
{
    return 0;
}

/** nfp_nffw_start
 */
int nfp_nffw_start(struct nfp_device *nfp, uint8_t fwid)
{
    return 0;
}

/** nfp_nffw_info_acquire
 */
int nfp_nffw_info_acquire(struct nfp_device *nfp)
{
    return 0;
}

/** nfp_nffw_info_fw_loaded
 */
int nfp_nffw_info_fw_loaded(struct nfp_device *nfp)
{
    return 0;
}

/** nfp_nffw_info_release
 */
int nfp_nffw_info_release(struct nfp_device *nfp)
{
    return 0;
}

/** nfp_rtsym_reload
 */
void nfp_rtsym_reload(struct nfp_device *nfp)
{
}

/** nfp_rtsym_count
 */
int nfp_rtsym_count(struct nfp_device *nfp)
{
    return 0;
}

/** nfp_rtsym_get
 */
const struct nfp_rtsym *nfp_rtsym_get(struct nfp_device *nfp, int id)
{
    return 0;
}

/** nfp_rtsym_lookup
 */
const struct nfp_rtsym *nfp_rtsym_lookup(struct nfp_device *nfp, const char *symname)
{
    return 0;
}

/** nfp_cpp_write
 */
int nfp_cpp_write(struct nfp_cpp *cpp, int cppid, uint64_t addr, void *data, int size)
{
    return 0;
}

/** nfp_cpp_read
 */
int nfp_cpp_read(struct nfp_cpp *cpp, int cppid, uint64_t addr, void *data, int size)
{
    return 0;
}

