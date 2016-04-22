/*a Copyright */
/**
 Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
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


/*a Defines */

/*a Functions */
/*f data_coproc_work_gatherer */
/**
 * @brief Gather work from PCIe work queues and start transfer to worker threads
 *
 * In conjunction with at least one data_coproc_workq_manager thread
 * on the same ME, which tells the gatherer which workq's are valid
 * and caches the data for them, the gatherers initiate DMAs and
 * deliver appropriate work to the worker threads.
 *
 * The gatherers do not wait for completion of the DMAs; that is the
 * responsiblity of the worker threads (which can tell when the work
 * they have to do is non-zero).
 *
 */
void data_coproc_work_gatherer(void);

/*f data_coproc_workq_manager */
/**
 * @brief Work queue manger
 *
 * @param max_queue Maximum work queue number to poll for work
 *
 * Main loop for the work queue manager
 *
 */
void data_coproc_workq_manager(int max_queue);

/*f data_coproc_init_workq_manager */
/**
 * @brief Perform initialization of the work queue manager
 *
 * @param poll_interval Standard poll interval for workq threads
 *
 * Initialize the work queue manager
 *
 */
void data_coproc_init_workq_manager(int poll_interval);
