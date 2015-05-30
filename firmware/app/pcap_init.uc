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
 * @file          pcap_init.uc
 * @brief         Packet capture initialization
 *
 */

    /* Loopback for now */
#define __PCS xpb:Nbi0IsldXpbMap.NbiTopXpbMap.MacGlbAdrMap.MACEthernet0.MacEthChPcsSeg0
    .init_csr __PCS.EthChPcsCtl1.EthPcsLoopback 1 const
#undef __PCS

#define __ETH xpb:Nbi0IsldXpbMap.NbiTopXpbMap.MacGlbAdrMap.MacEthernet1.MacEthSeg0
    .init_csr __ETH.EthCmdConfig.EthLoopBackEn 1 const
#undef __ETH

    /* Standard getting started for picoengine and characterizer */
#define __CHARACTERIZER xpb:Nbi0IsldXpbMap.NbiTopXpbMap.PktPreclassifier.Characterization
    .init_csr xpbm:0x0048290000 0x32ff0000 32 const
    .init_csr __CHARACTERIZER.PortCfg0     1           const
#undef __CHARACTERIZER

#define __PICOENGINE xpb:Nbi0IsldXpbMap.NbiTopXpbMap.PktPreclassifier.Picoengine
    .init_csr xpbm:0x0048280000 0x00050007 32 const
    .init_csr __PICOENGINE.PicoengineSetup      0x000000c0 const
    .init_csr __PICOENGINE.PicoengineRunControl 0x3ffffff0 const
#undef __PICOENGINE

    /* Set the CTM to use have the memory for packets */
#define __PKTENG xpb:Pcie0IsldXpbMap.CTMXpbMap.MuPacketReg
    .init_csr __PKTENG.MUPEMemConfig.MUPEMemConfig 1 const
#undef __PKTENG

    /* DMA config 0 is for MU (no paddings...) */
#define __PCI pcie:i4.PcieInternalTargets.DMAController
    .init_csr __PCI.DMADescrConfig0.CppTargetIDEven     0x7 const
    .init_csr __PCI.DMADescrConfig0.Target64bitEven     1   const
    .init_csr __PCI.DMADescrConfig0.NoSnoopEven         0   const
    .init_csr __PCI.DMADescrConfig0.RelaxedOrderingEven 0   const
    .init_csr __PCI.DMADescrConfig0.IdBasedOrderingEven 0   const
    .init_csr __PCI.DMADescrConfig0.StartPaddingEven    0   const
    .init_csr __PCI.DMADescrConfig0.EndPaddingEven      0   const
    .init_csr __PCI.DMADescrConfig0.SignalOnlyEven      0   const
#undef __PCI
