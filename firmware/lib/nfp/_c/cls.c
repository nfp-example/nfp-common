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
 * @file          firmware/lib/nfp/_c/cls.c
 * @brief         CLS library functions
 *
 */

/** Includes
 */
#include <nfp/cls.h>
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
__intrinsic void
cls_read(__xread void *data, __cls void *addr, int ofs,
         const size_t size)
{
    uint32_t size_in_uint32 = (size >> 2);
    SIGNAL sig;

    __asm {
        cls[read, *data, addr, ofs, size_in_uint32],\
            ctx_swap[sig];
    }
}

/** cls_write
 * Write a number of words of data to the local cluster scratch
 *
 * @param data   Transfer registers to write
 * @param addr   32-bit CLS island-local address
 * @param ofs    Offset from address
 * @param size   Size in bytes to write (must be multiple of 4)
 *
 */
__intrinsic void
cls_write(__xwrite void *data, __cls void *addr, int ofs,
          const size_t size)
{
    uint32_t size_in_uint32 = (size >> 2);
    SIGNAL sig;

    __asm {
        cls[write, *data, addr, ofs, size_in_uint32],\
            ctx_swap[sig];
    }
}

/** cls_incr
 *
 * @param addr   32-bit CLS island-local address
 * @param ofs    Offset from address
 *
 */
__intrinsic void
cls_incr(__cls void *addr, int ofs)
{
    __asm {
        cls[incr, --, addr, ofs, 1];
    }
}

/** cls_incr_rem
 *
 * @param cls_base_s8  40-bit CLS address >> 8
 * @param ofs          Offset from base
 *
 */
__intrinsic void
cls_incr_rem(uint32_t cls_base_s8,
             uint32_t ofs)
{
    __asm {
        cls[incr, --, cls_base_s8, <<8, ofs, 1];
    }
}

/** cls_ring_journal_rem
 *
 * @param data   Transfer registers to write
 * @param addr   32-bit CLS island-local address
 * @param ofs    Offset from address
 * @param size   Size in bytes to write (must be multiple of 4)
 *
 */
__intrinsic void
cls_ring_journal_rem(__xwrite void *data, uint32_t cls_base_s8,
                     int ring_s2, const size_t size)
{
    uint32_t size_in_uint32 = (size >> 2);
    SIGNAL sig;

    __asm {
        cls[ring_journal, *data, cls_base_s8, <<8, ring_s2, size_in_uint32], ctx_swap[sig];
    }
}

