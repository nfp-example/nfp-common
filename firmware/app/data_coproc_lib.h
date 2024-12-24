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


/*a Open guard
 */
#ifndef _DATA_COPROC_LIB_H_
#define _DATA_COPROC_LIB_H_

/*a Includes */
#include "firmware/data_coproc.h"

/*a Initialization */
/* DMA config */
#ifndef STRINGIFY
#define __STRINGIFY(x) #x
#define STRINGIFY(x) __STRINGIFY(x)
#endif

#ifdef DCPRC_INIT_CSRS
#define __PCIE_DMA_CSR(r) pcie:i4.PcieInternalTargets.DMAController.r
#define __PCIE_DMA_CFG(f) __PCIE_DMA_CSR(PCIE_DMA_CFG_CSR).f
__asm {
    .init_csr __PCIE_DMA_CFG(CppTargetIDEven)     0x7 const;
    .init_csr __PCIE_DMA_CFG(Target64bitEven)     1   const;
    .init_csr __PCIE_DMA_CFG(NoSnoopEven)         0   const;
    .init_csr __PCIE_DMA_CFG(RelaxedOrderingEven) 0   const;
    .init_csr __PCIE_DMA_CFG(IdBasedOrderingEven) 0   const;
    .init_csr __PCIE_DMA_CFG(StartPaddingEven)    0   const;
    .init_csr __PCIE_DMA_CFG(EndPaddingEven)      0   const;
    .init_csr __PCIE_DMA_CFG(SignalOnlyEven)      0   const;
}
#undef __PCIE_DMA_CFG
#undef __PCIE_DMA_CSR

/* Memory to ensure this is done at least once
 */
__asm {
    .alloc_mem dcprc_init_csrs_included ctm global 8 8;
}

/* End guard */
#endif // DCPRC_INIT_CSRS

/*a Types */
/*t dcprc_worker_me */
/**
 * Structure used to contain global data needed for any data coproc worker, filled out by dcprc_worker_init
 */
struct dcprc_worker_me {
    uint32_t muq_mu_workq;
    uint32_t mu_work_buffer_s8;
};

/*t dcprc_mu_work_entry */
/**
 * Structure used by data_coproc internally, needed by workers not
 * because of its contents, just so that one can be instantiated for
 * getting work and returning result (it must remain untouched between
 * dcprc_worker_get_work and dcprc_worker_write_results)
 */
struct dcprc_mu_work_entry {
    uint32_t host_physical_address_lo;
    uint32_t host_physical_address_hi;
    uint32_t mu_ofs;
    uint32_t pad;
};

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

/*f dcprc_worker_get_work */
/**
 * @brief Get work for a data coprocessor worker
 *
 * @param dcprc_worker_me Data structure initialized when worker ME
 * started with @p dcprc_worker_init()
 *
 * Add worker as a thread to MU workq, and get struct dcprc_mu_work_entry
 * delivered.
 *
 * Read work from MU work buffer that was DMAed by the @p
 * data_coproc_work_gatherer thread(s).
 *
 */
void
dcprc_worker_get_work(const struct dcprc_worker_me *restrict dcprc_worker_me,
                      __xread struct dcprc_mu_work_entry *restrict mu_work_entry,
                      struct dcprc_workq_entry *restrict workq_entry);

/*f dcprc_worker_claim_dma */
__intrinsic void
dcprc_worker_claim_dma(int to_pcie, int poll_interval);

/*f dcprc_worker_release_dma */
__intrinsic void
dcprc_worker_release_dma(int to_pcie);

/*f dcprc_worker_write_results */
/**
 * @brief Write results back to the host work queue for work done
 *
 * @param dcprc_worker_me Data structure initialized when worker ME
 * started with @p dcprc_worker_init()
 *
 */
void
dcprc_worker_write_results(const struct dcprc_worker_me *restrict dcprc_worker_me,
                           const struct dcprc_mu_work_entry *restrict mu_work_entry,
                           const struct dcprc_workq_entry *restrict workq_entry);
/*f dcprc_worker_init */
/**
 */
__intrinsic void dcprc_worker_init(struct dcprc_worker_me *restrict dcprc_worker_me);

/*a Close guard
 */
#endif /*_DATA_COPROC_LIB_H_ */
