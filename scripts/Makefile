#a Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# @file        scripts/Makefile
# @brief       Makefile for scripting and debugging
#

DBGQ := @

trace_packets_in:
	@echo "Trace packets in from the outside world"
	@echo "Packets first arrive at the MAC..."
	@echo ""
	@echo "Packets then hit the characterizer and will be counted - this does not auto-clear"
	$(DBGQ)nfp-reg xpb:Nbi0IsldXpbMap.NbiTopXpbMap.PktPreclassifier.Characterization.Sequence.Number
	@echo ""
	@echo "Packets then come through the picoengines - separately as large/small packets (small<=256B)"
	@echo "If these counters are not going up but the characterizer has seen packets, is the preclassifier running?"
	@echo "Note that these counters are clear-on-read"
	$(DBGQ)nfp-reg xpb:Nbi0IsldXpbMap.NBITopXpbMap.PktPreclassifier.Picoengine.ClassifiedSmall
	$(DBGQ)nfp-reg xpb:Nbi0IsldXpbMap.NBITopXpbMap.PktPreclassifier.Picoengine.ClassifiedLarge
	@echo ""
	@echo "Check to see if the preclassifier picoengines are delivering packets (IgnoreResult should be 0)"
	$(DBGQ)nfp-reg xpb:Nbi0IsldXpbMap.NBITopXpbMap.PktPreclassifier.Picoengine.RunControl.IgnoreResult
	@echo ""
	@echo "Then packets are DMAed, and counted again - but they may be dropped here and counted separately..."
	@echo "Note that these counters are clear-on-read"
	$(DBGQ)nfp-reg xpb:Nbi0IsldXpbMap.NBITopXpbMap.NbiDmaXpb.NbiDmaCsr.NbiDmaPktCntLo
	$(DBGQ)nfp-reg xpb:Nbi0IsldXpbMap.NBITopXpbMap.NbiDmaXpb.NbiDmaCsr.NbiDmaDropPktCntLo
	@echo ""
	@echo "for information, there are also byte counts"
	@echo "Note that these counters are clear-on-read"
	$(DBGQ)nfp-reg xpb:Nbi0IsldXpbMap.NBITopXpbMap.NbiDmaXpb.NbiDmaCsr.NbiDmaByteCntLo
	$(DBGQ)nfp-reg xpb:Nbi0IsldXpbMap.NBITopXpbMap.NbiDmaXpb.NbiDmaCsr.NbiDmaDropByteCntLo

mac_stats:
	nfp -m mac show port stats 0 0-11

dpdk_cbtest3:
	(cd ~/pablo/pktgen-dpdk; ./app/app/build/pktgen -c 0xf -n 3 -- -P -p 0x01 -m "[0:1].0")

mailboxes:
	nfp-reg mecsr:i4.me0.Mailbox0 mecsr:i4.me0.Mailbox1 mecsr:i4.me0.Mailbox2 mecsr:i4.me0.Mailbox3
	nfp-reg mecsr:i4.me1.Mailbox0 mecsr:i4.me1.Mailbox1 mecsr:i4.me1.Mailbox2 mecsr:i4.me1.Mailbox3
	nfp-reg mecsr:i4.me2.Mailbox0 mecsr:i4.me2.Mailbox1 mecsr:i4.me2.Mailbox2 mecsr:i4.me2.Mailbox3
	nfp-reg mecsr:i4.me3.Mailbox0 mecsr:i4.me3.Mailbox1 mecsr:i4.me3.Mailbox2 mecsr:i4.me3.Mailbox3
	nfp-reg mecsr:i32.me0.Mailbox0 mecsr:i32.me0.Mailbox1 mecsr:i32.me0.Mailbox2 mecsr:i32.me0.Mailbox3
	nfp-reg mecsr:i33.me0.Mailbox0 mecsr:i33.me0.Mailbox1 mecsr:i33.me0.Mailbox2 mecsr:i33.me0.Mailbox3
	nfp-reg mecsr:i34.me0.Mailbox0 mecsr:i34.me0.Mailbox1 mecsr:i34.me0.Mailbox2 mecsr:i34.me0.Mailbox3

pcie_me_status:
	@nfp-reg mecsr:i4.me0.ActCtxStatus.ActiveContextPC mecsr:i4.me0.ActCtxStatus.ActiveContextNo mecsr:i4.me0.CtxEnables.CtxEnables mecsr:i4.me0.CtxEnables.Breakpoint
	@nfp-reg mecsr:i4.me1.ActCtxStatus.ActiveContextPC mecsr:i4.me1.ActCtxStatus.ActiveContextNo mecsr:i4.me1.CtxEnables.CtxEnables mecsr:i4.me1.CtxEnables.Breakpoint
	@nfp-reg mecsr:i4.me2.ActCtxStatus.ActiveContextPC mecsr:i4.me2.ActCtxStatus.ActiveContextNo mecsr:i4.me2.CtxEnables.CtxEnables mecsr:i4.me2.CtxEnables.Breakpoint
	@nfp-reg mecsr:i4.me3.ActCtxStatus.ActiveContextPC mecsr:i4.me3.ActCtxStatus.ActiveContextNo mecsr:i4.me3.CtxEnables.CtxEnables mecsr:i4.me3.CtxEnables.Breakpoint

me_trace:
	@echo "Getting ME trace for ME $(ME) (e.g. make ME=pcie0.me0 me_trace)"
	@(cd $(NFP_COMMON)/../nfp-devtools-0.1; PYTHONPATH=$(NFP_COMMON)/../nfp-devtools-0.1/module ./tools/nfp-pa -m $(ME) -r -f $(NFP_COMMON)/firmware/nffw/pktgencap.nffw meinst)
