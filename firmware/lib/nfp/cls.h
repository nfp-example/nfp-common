/*
 * Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
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
 * @file          firmware/lib/nfp/cls.h
 * @brief         Cluster scratch assist library functions
 *
 * This is a library of functions used by application firmware to
 * utilize the CLS features
 * 
 */
#ifndef _NFP__CLS_H_
#define _NFP__CLS_H_

/** Includes required
 */
#include <stdint.h>
#include <nfp.h>

/** cls_read
 *
 * @param data   Transfer registers to read
 * @param addr   32-bit CLS island-local address
 * @param ofs    Offset from address
 * @param size   Size in bytes to read (must be multiple of 4)
 *
 */
__intrinsic void cls_read(__xread void *data, __cls void *addr,
                          int ofs, const size_t size);

/** cls_write
 *
 * @param data   Transfer registers to write
 * @param addr   32-bit CLS island-local address
 * @param ofs    Offset from address
 * @param size   Size in bytes to write (must be multiple of 4)
 *
 */
__intrinsic void cls_write(__xwrite void *data, __cls void *addr,
                           int ofs, const size_t size);

/** cls_incr
 *
 * @param addr   32-bit CLS island-local address
 * @param ofs    Offset from address
 *
 */
__intrinsic void cls_incr(__cls void *addr, int ofs);

/** cls_ring_journal_rem
 *
 * @param data         Transfer registers to write
 * @param cls_base_s8  40-bit CLS address >> 8
 * @param ring_s2      Ring number << 2
 * @param size         Size in bytes to write (must be multiple of 4)
 *
 */
__intrinsic void cls_ring_journal_rem(__xwrite void *data, uint32_t cls_base_s8,
                                      int ring_s2, const size_t size);

/** Close guard
 */
#endif /*_NFP__CLS_H_ */
