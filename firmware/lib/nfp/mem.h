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
 * @file          firmware/lib/nfp/mem.h
 * @brief         Memory unit library functions
 *
 */
#ifndef _NFP__MEM_H_
#define _NFP__MEM_H_

/** Includes required
 */
#include <stdint.h>
#include <nfp.h>

/** Defines
 */
#define QSTR(s) #s
#define QSYM(sym,size,qa,mem) sym
#define QSIZE(sym,size,qa,mem) size
#define QQA(sym,size,qa,mem) qa
#define QMEM(sym,size,qa,mem) mem
#define QSTRSYM(sym,size,qa,mem) QSTR(sym)

#define MU_QUEUE_ALLOC(qdef) \
    __asm { .alloc_mem QSYM(qdef) QMEM(qdef) global (4<<QSIZE(qdef)) (4<<QSIZE(qdef)) };

#define MU_QUEUE_CONFIG_GET(qdef) \
    mem_queue_config_get(QQA(qdef),__link_sym(QSTRSYM(qdef)),QSIZE(qdef))

#define MU_QUEUE_CONFIG_WRITE(qdef) \
    mem_queue_config_write(QQA(qdef),__link_sym(QSTRSYM(qdef)),QSIZE(qdef))

#define MU_QDESC_QA(mu_qdesc) ((mu_qdesc)&0x3ff)
#define MU_QDESC_MU(mu_qdesc) ((mu_qdesc)&0xff000000)

/** mem_read64
 *
 * @param data   Transfer registers to read
 * @param addr   Full 40-bit pointer in to memory
 * @param size   Size in bytes to read (must be multiple of 8)
 *
 */
__intrinsic void mem_read64(__xread void *data, __mem void *addr,
                            const size_t size);

/** mem_read64_s8
 *
 * @param data     Transfer registers to read
 * @param base_s8  Base address in MU >> 8
 * @param ofs      Offset in bytes from MU base
 * @param size     Size in bytes to write (must be multiple of 8)
 *
 */
__intrinsic void mem_read64_s8(__xread void *data, uint32_t base_s8,
                               uint32_t ofs, const size_t size);

/** mem_write64
 *
 * @param data   Transfer registers to read
 * @param addr   Full 40-bit pointer in to memory
 * @param size   Size in bytes to write (must be multiple of 8)
 *
 */
__intrinsic void mem_write64(__xwrite void *data, __mem void *addr,
                             const size_t size);

/** mem_write64_s8
 *
 * @param data     Transfer registers to write
 * @param base_s8  Base address in MU >> 8
 * @param ofs      Offset in bytes from MU base
 * @param size     Size in bytes to write (must be multiple of 8)
 *
 */
__intrinsic void mem_write64_s8(__xwrite void *data, uint32_t base_s8,
                                uint32_t ofs, const size_t size);

/** mem_atomic_read_s8
 *
 * @param data     Transfer registers to read
 * @param base_s8  Base address in memory>>8
 * @param ofs      Offset from base to read
 * @param size     Size in bytes to read (must be multiple of 4)
 *
 */
__intrinsic void mem_atomic_read_s8(__xread void *data, uint32_t base_s8,
                                    uint32_t ofs, int size );

/** mem_atomic_write_s8
 *
 * @param data     Transfer registers to write
 * @param base_s8  Base address in memory>>8
 * @param ofs      Offset from base to write
 * @param size     Size in bytes to write (must be multiple of 4)
 *
 */
__intrinsic void mem_atomic_write_s8(__xwrite void *data,
                                     uint32_t base_s8, uint32_t ofs,
                                     int size );
/** mem_ring_journal
 */
__intrinsic void mem_ring_journal(uint32_t mu_qdesc,
                                    __xwrite uint32_t *data, int size);

/** mem_workq_add_work
 */
__intrinsic void mem_workq_add_work(uint32_t mu_qdesc,
                                    __xwrite uint32_t *data, int size);

/** mem_workq_add_work_async
 */
__intrinsic void mem_workq_add_work_async(uint32_t mu_qdesc,
                                          __xwrite uint32_t *data,
                                          int size,
                                          SIGNAL *sig);

/** mem_workq_add_thread
 */
__intrinsic void mem_workq_add_thread(uint32_t mu_qdesc,
                                      __xread uint32_t *data, int size);

/** mem_queue_config_write
 *
 * Configure a memory unit queue. Returns a queue handle for use with,
 * e.g. mem_workq_add_thread
 *
 * @param qa       QA index to use within EMEM
 * @param base     40-bit base address
 * @param log_size log2(queue size in words)
 *
 */
__intrinsic uint32_t mem_queue_config_write(int qa, uint64_t base, int log_size);

/** mem_queue_config_get
 *
 * Configure a memory unit queue. Returns a queue handle for use with,
 * e.g. mem_workq_add_thread
 *
 * @param qa       QA index to use within EMEM
 * @param base     40-bit base address
 * @param log_size log2(queue size in words)
 *
 */
__intrinsic uint32_t
mem_queue_config_get(int qa, uint64_t base, int log_size)
{
    return ((base>>8)&0xff000000) | qa;
}
/** Close guard
 */
#endif /*_NFP__MEM_H_ */
