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
 * @file          pktgen_init.uc
 * @brief         Packet generator initialization
 *
 */

    /* DMA config 8 is for CLS */
#define __PCI pcie:i4.PcieInternalTargets.DMAController
    .init_csr __PCI.DMADescrConfig8.CppTargetIDEven     0xf const
    .init_csr __PCI.DMADescrConfig8.Target64bitEven     0   const
    .init_csr __PCI.DMADescrConfig8.NoSnoopEven         0   const
    .init_csr __PCI.DMADescrConfig8.RelaxedOrderingEven 0   const
    .init_csr __PCI.DMADescrConfig8.IdBasedOrderingEven 0   const
    .init_csr __PCI.DMADescrConfig8.StartPaddingEven    0   const
    .init_csr __PCI.DMADescrConfig8.EndPaddingEven      0   const
    .init_csr __PCI.DMADescrConfig8.SignalOnlyEven      0   const
#undef __PCI
