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
 * @file        pcap_lib.h
 * @brief       A simple packet capture system
 *
 * This is a library to support a PCAP packet capture to a host x86 system
 * 
 */


/** Defines
 */
#define PKT_CAP_MU_BUF_SHIFT 18
#define PKT_CAP_MU_BUF_SIZE (1<<PKT_CAP_MU_BUF_SHIFT)

/** packet_capture_pkt_rx_dma
 *
 * Handle packet received by CTM; claim next part of MU buffer, DMA
 * the packet in, then pass on to work queue and free the packet
 * 
 * @param  poll_interval   number of ticks to wait when polling
 *
 */
void packet_capture_pkt_rx_dma(int poll_interval);

/** packet_capture_mu_buffer_recycler
 *
 * One thread required per system per MU buffer pool
 *
 * @param  poll_interval   number of ticks to wait when polling
 *
 */
void packet_capture_mu_buffer_recycler(int poll_interval);

/** packet_capture_dma_to_host_slave 
 *
 * Owns a DMA to the host
 *
 * Need at least four threads per active MU buffer
 * Consumes ~5% CPU
 *
 * Can be shared with other thread types, and can run on any island
 */
void packet_capture_dma_to_host_slave(void);

/** packet_capture_dma_to_host_master
 *
 * Owns an MU buffer and its transfer to the host
 *
 * @param  poll_interval   number of ticks to wait when polling
 *
 * Need at least one thread per active MU buffer, possibly 2
 *
 * Can be shared with other thread types, and can run on any island
 * Consumes ~5% CPU
 */
void packet_capture_dma_to_host_master(int poll_interval);

/** packet_capture_fill_mu_buffer_list
 *
 * Fill the MU buffer list with 'num_buf' 256kB buffers starting at
 * given base
 *
 * @param  mu_base_s8   MU buffer base address >> 8
 * @param  num_buf      Number of 256kB buffers to fill with
 *
 */
void packet_capture_fill_mu_buffer_list(uint32_t mu_base_s8,
                                        int num_buf);

/** packet_capture_init_pkt_rx_dma
 *
 * Perform initialization for the packet rx DMA threads
 *
 * Gets queue configuration required by the threads
 *
 */
__intrinsic void packet_capture_init_pkt_rx_dma(void);

/** packet_capture_init_mu_buffer_recycler
 *
 * Perform initialization for the MU buffer recycler
 *
 * Writes configuration of the MU_BUF_ALLOC queue
 * Gets queue configuration required by the threads
 *
 */
__intrinsic void packet_capture_init_mu_buffer_recycler(void);

/** packet_capture_init_dma_to_host_master
 *
 * Perform initialization for the DMA to host master
 *
 * Writes configuration of the MU_BUF_RECYCLE, TO_HOST_DMA and
 * MU_BUF_IN_USE queues
 *
 */
__intrinsic void packet_capture_init_dma_to_host_master(void);

/** packet_capture_init_dma_to_host_slave
 *
 * Perform initialization for the DMA to host slave
 *
 * Gets TO_HOST_DMA queue configuration for the thread
 *
 */
__intrinsic void packet_capture_init_dma_to_host_slave(void);
