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
 * Header files required to bluff out the NFP
 *
 */

/** Includes
 */
#include <stddef.h> 
#include <stdint.h> 

#define SHM_HUGETLB 0
#define GHP_DEFAULT 0
#define NFP_CPP_ISLAND_ID(a,b,c,d) 0
struct nfp_rtsym {
    const char *name;
    int target;
    int domain;
    uint64_t addr;
};
int gethugepagesize(void);
void *get_huge_pages(size_t size, int flags);
void free_huge_pages(void *ptr);
struct nfp_device *nfp_device_open(int dev);
void nfp_device_close(struct nfp_device *nfp);
struct nfp_cpp *nfp_device_cpp(struct nfp_device *nfp);
int nfp_nffw_load(struct nfp_device *nfp, char *nffw, int nffw_size, uint8_t *fwid);
int nfp_nffw_unload(struct nfp_device *nfp, uint8_t fwid);
int nfp_nffw_start(struct nfp_device *nfp, uint8_t fwid);
int nfp_nffw_info_acquire(struct nfp_device *nfp);
int nfp_nffw_info_fw_loaded(struct nfp_device *nfp);
int nfp_nffw_info_release(struct nfp_device *nfp);
void nfp_rtsym_reload(struct nfp_device *nfp);
int nfp_rtsym_count(struct nfp_device *nfp);
const struct nfp_rtsym *nfp_rtsym_get(struct nfp_device *nfp, int id);
const struct nfp_rtsym *nfp_rtsym_lookup(struct nfp_device *nfp, const char *symname);
int nfp_cpp_write(struct nfp_cpp *cpp, int cppid, uint64_t addr, void *data, int size);
int nfp_cpp_read(struct nfp_cpp *cpp, int cppid, uint64_t addr, void *data, int size);
