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
 * @file          firmware/lib/nfp/_c/mem.c
 * @brief         Memory unit library functions
 *
 */

/** Includes
 */
#include <nfp/mem.h>
#include <stdint.h>
#include <nfp.h>
#include <nfp_intrinsic.h>
#include <nfp_override.h>

/** struct queue_desc
 */
struct queue_desc {
    union {
        struct {
            unsigned int ring_size:4;
            unsigned int reserved1:2;
            unsigned int head_ptr:24;
            unsigned int eop:1;
            unsigned int zero:1;
            unsigned int tail_ptr:30;
            unsigned int ring_type:2;
            unsigned int q_loc:2;
            unsigned int reserved2:4;
            unsigned int q_page:2;
            unsigned int q_count:24;
            uint32_t padding;
        };
        uint32_t __raw[4];
    };
};

/** mem_read64
 *
 * @param data   Transfer registers to read
 * @param addr   Full 40-bit pointer in to memory
 * @param size   Size in bytes to write (must be multiple of 8)
 *
 */
__intrinsic void
mem_read64(__xread void *data, __mem void *addr, const size_t size)
{
    SIGNAL sig;
    uint32_t addr_lo, addr_hi, size_in_uint64;
    size_in_uint64 = size>>3;
    addr_lo = (uint32_t)(((uint64_t)addr)&0xff);
    addr_hi = (uint32_t)(((uint64_t)addr)>>8);
    __asm {
        mem[read, *data, addr_hi, <<8, addr_lo, \
            __ct_const_val(size_in_uint64)], ctx_swap[sig];
    }
}

/** mem_write64
 *
 * @param data   Transfer registers to write
 * @param addr   Full 40-bit pointer in to memory
 * @param size   Size in bytes to write (must be multiple of 8)
 *
 */
__intrinsic void
mem_write64(__xwrite void *data, __mem void *addr, const size_t size)
{
    SIGNAL sig;
    uint32_t addr_lo, addr_hi, size_in_uint64;
    size_in_uint64 = size>>3;
    addr_lo = (uint32_t)(((uint64_t)addr)&0xff);
    addr_hi = (uint32_t)(((uint64_t)addr)>>8);
    __asm {
        mem[write, *data, addr_hi, <<8, addr_lo, \
            __ct_const_val(size_in_uint64)], ctx_swap[sig];
    }
}

/** mem_atomic_read_s8
 */
__intrinsic void
mem_atomic_read_s8(__xread void *xfr, uint32_t base_s8, uint32_t ofs, int size )
{
    SIGNAL sig;
    int size_in_words=size/sizeof(int);
    __asm {
        mem[atomic_read, *xfr, base_s8, <<8, ofs, size_in_words], ctx_swap[sig];
    }
}

/** mem_atomic_write_s8
 */
__intrinsic void
mem_atomic_write_s8(__xwrite void *xfr, uint32_t base_s8, uint32_t ofs, int size )
{
    SIGNAL sig;
    int size_in_words=size/sizeof(int);
    __asm {
        mem[atomic_write, *xfr, base_s8, <<8, ofs, size_in_words], ctx_swap[sig];
    }
}

/** mem_workq_add_work
 */
__intrinsic void
mem_workq_add_work(uint32_t mu_qdesc, __xwrite uint32_t *data, int size)
{
    int size_in_words = size>>2;
    uint32_t qa, mu;
    SIGNAL sig;
    qa = MU_QDESC_QA(mu_qdesc);
    mu = MU_QDESC_MU(mu_qdesc);
    __asm {
        mem[qadd_work, *data, mu,<<8,qa, size_in_words], ctx_swap[sig];
    }
}

/** mem_workq_add_thread
 */
__intrinsic void
mem_workq_add_thread(uint32_t mu_qdesc, __xread uint32_t *data, int size)
{
    int size_in_words = size>>2;
    uint32_t qa, mu;
    SIGNAL sig;
    qa = MU_QDESC_QA(mu_qdesc);
    mu = MU_QDESC_MU(mu_qdesc);
    __asm {
        mem[qadd_thread, *data, mu,<<8,qa, size_in_words], ctx_swap[sig];
    }
}

/** mem_queue_config_write
 *
 * Configure a memory unit queue. Returns a queue handle for use with, e.g. mem_workq_add_thread
 *
 * @param qa       QA index to use within EMEM
 * @param base     40-bit base address
 * @param log_size log2(queue size in words)
 *
 */
__intrinsic uint32_t
mem_queue_config_write(int qa, uint64_t base, int log_size)
{
    uint64_t mu_addr;
    int mu_qa;

    struct queue_desc queue_desc;
    __xwrite struct queue_desc desc_out;
    __xread  struct queue_desc desc_in;
    uint32_t address_hi;
    generic_ind_t override;

    queue_desc.__raw[0] = 0;
    queue_desc.__raw[1] = 0;
    queue_desc.__raw[2] = 0;
    queue_desc.__raw[3] = 0;
    queue_desc.ring_size = log_size-9;
    queue_desc.head_ptr = (((uint32_t) base) >> 2) & 0xffffff;
    queue_desc.tail_ptr = (((uint32_t) base) >> 2) & 0x3fffffff;
    queue_desc.ring_type = 2;
    queue_desc.q_loc = 0;
    queue_desc.q_page = (base>>32) & 0x3;

    desc_out = queue_desc;
    mem_write64(&desc_out, (__mem void *)base, sizeof(queue_desc));
    mem_read64(&desc_in,   (__mem void *)base, sizeof(queue_desc));

    address_hi = base>>8;
    _INTRINSIC_OVERRIDE_DATA_IN_ALU(override, qa);
    __asm {
        alu[ --, --, B, override ];
        mem[rd_qdesc, --, address_hi, <<8, 0], indirect_ref;
    }

    return (address_hi&0xff000000) | qa;
}
