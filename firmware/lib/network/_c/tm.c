#include "nfp6000/nfp_nbi_tm.h"

/** init_tm_head_tail_sram - Initialize the head/tail SRAM in the TM
 *
 * @param nbi_island NBI island to configure
 * 
 * Initialized the head/tail SRAM to provide 1024 queues of 16 entries
 *
 * The code writes the head/tail SRAM for all 1024 queues to be queue*16
 */
void init_tm_head_tail_sram( int nbi_island )
{
    SIGNAL sig;
    int address_base_shift_8 = ((nbi_island&3)<<30) | (2<<12); // Magic to address through CPP the TM memories
    int offset=0;
    int i;
    uint64_t value;
    __xwrite uint64_t data;
    for (i=0; i<1024; i++) {
        offset = 0x68000 + i*8; // base of 0x68000 for TMHeadTailSram
        value = 16*i;
        value = (value<<14) | value;
        /* Cope with the big-endian 64-bit compiler / LWBE hardware */
        data = value<<32;
        __asm {
            nbi[ write, data, address_base_shift_8, <<8, offset, 1 ], \
                ctx_swap[sig]
                }
    }
}

/** init_tm_config - Set the TM config XPB registers
 *
 * @param nbi_island NBI island to configure
 * 
 * Set the configuration of the TM generically as required
 * The numbers in here are currently magic
 */
#define NBI_XPB_TM_REG 0x14
#define NBI_XPB_TM_REG__TrafficManagerConfig NFP_NBI_TMX_CSR_TRAFFIC_MANAGER_CONFIG
#define NBI_XPB_TM_REG__BlqEvent             NFP_NBI_TMX_CSR_BLQ_EVENT
#define NBI_XPB_TM_REG__MiniPktCreditConfig  NFP_NBI_TMX_CSR_MINIPKT_CREDIT_CONFIG
#define MiniPktCreditConfig_MagicNumber 0x1200014
#define TrafficMangerConfig_MagicNumber 0x1d40
#define BlqEvent_MagicNumber 0xf

static void init_tm_config( nbi_island )
{
    int xpb_base = (1<<31) | (nbi_island<<24) | (NBI_XPB_TM_REG<<16);
    xpb_write( xpb_base, NBI_XPB_TM_REG__TrafficManagerConfig,
               TrafficMangerConfig_MagicNumber );
    xpb_write( xpb_base, NBI_XPB_TM_REG__MiniPktCreditConfig,
               MiniPktCreditConfig_MagicNumber );
    xpb_write( xpb_base, NBI_XPB_TM_REG__BlqEvent,
               BlqEvent_MagicNumber );
}

/** init_tm_queue - Initialize a TM queue
 *
 * @param nbi_island NBI island to configure
 * @param queue      Queue to configure
 * @param size       log2 of size of queue, e.g. 4 for size of 16 entries 
 * @param enable     1 if queue should be enabled, 0 otherwise
 * 
 * Initialize a queue configuration to be a certain size, and enabled or not
 *
 */
static void init_tm_queue_config( nbi_island, queue, size, enable )
{
    int xpb_base = (1<<31) | (nbi_island<<24) | (0x15<<16);
    xpb_write( xpb_base, 0x1000 | (queue*4), (size<<6)|enable );
}

/** init_tm - Initialize the TM in an NBI to 1024 queues
 *
 * @param nbi_island NBI island to configure
 * 
 * Simple configuration of 1024 queues of 16 entries each
 * 
 * Also configures the TM registers needed to get things off the ground
 */
void init_tm( int nbi_island )
{
    int i;
    init_tm_config( nbi_island );
    init_tm_head_tail_sram( nbi_island );
    //for (i=0; i<1024; i++)
    //{
    //    init_tm_queue_config( nbi_island, i, 4, 1 );
    //}
    // PXE
    for (i=0; i<1024; i++)
    {
        init_tm_queue_config( nbi_island, i, 4, 0 );
    }
    init_tm_queue_config( nbi_island, 0, 12, 1 );
}

/*a Receive side initialize functions
 */
