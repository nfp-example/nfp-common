#include "network/init.h"
#include "nfp/xpb.h"

/** network_init_npc - Initialize the NBI preclassifier.
 *
 * @param nbi_island The NBI island number (8 or 9) to initialize
 *
 * The preclassifier requires two devices to be configured,
 * the characterization and the picoengines (PPCs)
 *
 * The characterization needs buffer setup and credits
 * 
 * The picoengines need to be powered up, and enabled
 * 
 */
void network_npc_init(int nbi_island)
{
    int i;
    int xpb_base;
    xpb_base = (1<<31) | (nbi_island<<24) | (0x29<<16); // NBI characterization
    xpb_write( xpb_base,    0, 0x32ff0000 ); // BufferStatus to 50 packets in class, 255 buffers available
    //xpb_write( xpb_base, 0x30, 2 );  DMA credits - not to be used...

    xpb_base = (1<<31) | (nbi_island<<24) | (0x28<<16); // NBI picoengine
    xpb_write( xpb_base,    0, 0x00050007 ); // 48 picoengines, shared the shared mems
    xpb_write( xpb_base,    4, 0x00000040 ); // Sequencer replace 16 bits (may be overwritten by picocode load?)
    xpb_write( xpb_base,    8, 0x3ffffff5 ); // RunControl enable picoengines and smems, allocate ppc to incoming packets, disable forward to DMA
}

/** network_npc_control - Control delivery of packets from an NBI preclassifier
 *
 * @param nbi_island      The NBI island number (8 or 9) to control
 * @param enable_packets  Set to enable reception of packets, clear to disable
 *
 */
void
network_npc_control(int nbi_island, int enable_packets)
{
    int xpb_base;
    int r;
    xpb_base = (1<<31) | (nbi_island<<24) | (0x28<<16); // NBI picoengine run control
    r = xpb_read( xpb_base, 8);
    r |= 4; // Ignore preclassification results (don't tell NBI Rx DMA engine about pkts)
    if (enable_packets) {
        r &= ~4; // If enabled, forward pkts to NBI Rx DMA engine
    }
    xpb_write( xpb_base,    8, r);
}
