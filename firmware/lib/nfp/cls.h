/*a Copyright */
/**
 * Copyright (C) 2015-2016,  Gavin J Stark.  All rights reserved.
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
/*a Open wrapper */
#ifndef _NFP__CLS_H_
#define _NFP__CLS_H_

/*a Includes required */
#include <stdint.h>
#include <nfp.h>

/*a Functions */
/*f cls_read */
/**
 * @brief Read data from the local CLS, waiting for completion
 *
 * @param data   Transfer registers to read
 * @param addr   32-bit CLS island-local address
 * @param ofs    Offset from address
 * @param size   Size in bytes to read (must be multiple of 4)
 *
 */
__intrinsic void cls_read(__xread void *data, __cls void *addr,
                          int ofs, const size_t size);

/*f cls_write */
/**
 * @brief Write data to the local CLS, waiting for completion
 *
 * @param data   Transfer registers to write
 * @param addr   32-bit CLS island-local address
 * @param ofs    Offset from address
 * @param size   Size in bytes to write (must be multiple of 4)
 *
 */
__intrinsic void cls_write(__xwrite void *data, __cls void *addr,
                           int ofs, const size_t size);

/*f cls_add */
/**
 * @brief Add data to the local CLS, waiting for completion
 *
 * @param data   Data to add to CLS memory
 * @param addr   32-bit CLS island-local address
 * @param ofs    Offset from address
 * @param size   Size in bytes to write (must be multiple of 4)
 *
 */
__intrinsic void cls_add(__xwrite void *data, __cls void *addr,
                              int ofs, const size_t size);

/*f cls_sub */
/**
 * @brief Sub data to the local CLS, waiting for completion
 *
 * @param data   Data to sub to CLS memory
 * @param addr   32-bit CLS island-local address
 * @param ofs    Offset from address
 * @param size   Size in bytes to write (must be multiple of 4)
 *
 */
__intrinsic void cls_sub(__xwrite void *data, __cls void *addr,
                              int ofs, const size_t size);

/*f cls_test_add */
/**
 * @brief Read-and-add data to the local CLS, waiting for completion
 *
 * @param data   Data to add to CLS memory, and premodified data return
 * @param addr   32-bit CLS island-local address
 * @param ofs    Offset from address
 * @param size   Size in bytes to write (must be multiple of 4)
 *
 */
__intrinsic void cls_test_add(__xrw void *data, __cls void *addr,
                              int ofs, const size_t size);

/*f cls_test_sub */
/**
 * @brief Read-and-sub data to the local CLS, waiting for completion
 *
 * @param data   Data to sub to CLS memory, and premodified data return
 * @param addr   32-bit CLS island-local address
 * @param ofs    Offset from address
 * @param size   Size in bytes to write (must be multiple of 4)
 *
 */
__intrinsic void cls_test_sub(__xrw void *data, __cls void *addr,
                              int ofs, const size_t size);

/*f cls_incr */
/**
 * @brief Increment a (32-bit) value in the local CLS - no waiting required
 *
 * @param addr   32-bit CLS island-local address
 * @param ofs    Offset from address
 *
 */
__intrinsic void cls_incr(__cls void *addr, int ofs);

/*f cls_incr_rem */
/**
 * @brief Increment a (32-bit) value in a remote CLS - no waiting required
 *
 * @param cls_base_s8  40-bit CLS address >> 8
 * @param ofs          Offset from base
 *
 */
__intrinsic void cls_incr_rem(uint32_t cls_base_s8,
                              uint32_t ofs);

/*f cls_ring_journal_rem */
/**
 * @brief Journal data in a remote CLS ring
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
