#include "network/init.h"
#include "nfp/xpb.h"

/** network_dma_init_buffer_list - initialize a buffer list in an NBI DMA
 *
 * @param nbi_island    The NBI island number (8 or 9) to initialize
 * @param buffer_list   The buffer list to be configured
 * @param num_buffers   Number of buffers to provide the list initially
 * @param base          Base address, including MU island and addressing, of
 *                      first buffer to add
 * @param stride        Separation in bytes of the buffers in MU address space
 *
 * 
 */
void
network_dma_init_buffer_list(int nbi_island, int buffer_list, int num_buffers, uint64_t base, uint32_t stride )
{
    SIGNAL sig;
    int address_base_shift_8 = ((nbi_island&3)<<30) | (0<<12); // Magic to address through CPP the DMA memories
    int offset=0;
    int i;
    uint64_t value;
    __xwrite uint64_t data;

    value = (base>>11);
    for (i=0; i<num_buffers; i++) {
        offset = 0x0000 + i*8;
        data = value<<32;
        __asm {
            nbi[ write, data, address_base_shift_8, <<8, offset, 1 ], \
                ctx_swap[sig]
                }
        value += (stride>>11);
    }
    offset = 0x8000 + buffer_list*8; // base of 0x68000 for TMHeadTailSram
    data = ((num_buffers|0LL)<<32) ; // MUST BE LESS THAN 512 SINCE WE ARE SETTING SIZE TO 512
    __asm {
        nbi[ write, data, address_base_shift_8, <<8, offset, 1 ], ctx_swap[sig]
            }
}

/** network_dma_init - Initialize the NBI receive DMA
 *
 * @param nbi_island    The NBI island number (8 or 9) to initialize
 * 
 */
void
network_dma_init(int nbi_island)
{
    int xpb_base;
    int i;

    xpb_base = (1<<31) | (nbi_island<<24) | (0x10<<16); // NBI DMA
    xpb_write( xpb_base, 0, (((nbi_island&3)+1)<<7) | (1<<6) ); // NbiDmaCfg nbi island and enable CTM polling
    xpb_write( xpb_base, 0x18, 0 ); // Clear all BP endpoints
    for (i=0; i<32; i++) {
        xpb_write( xpb_base, 0x40+i*4, 0 ); // BPE disabled
    }
}


/** network_dma_init_bp - Initialize the NBI receive DMA
 *
 * @param nbi_island    The NBI island number (8 or 9) to initialize
 * @param buffer_pool   Buffer pool number (0 to 7)
 * @param bpe_start     First buffer pool entry to use for the buffer pool
 * @param buf_pool_desc Description of how to set up the buffer pool
 * 
 * Returns next available buffer pool entry number (to use with later call of network_dma_init_bp)
 */
int
network_dma_init_bp(int nbi_island, int buffer_pool, int bpe_start,
                    int ctm_offset, int split_length)
{
    int xpb_base;

    xpb_base = (1<<31) | (nbi_island<<24) | (0x10<<16); // NBI DMA

    xpb_write( xpb_base,
               0x20+(buffer_pool<<2),
               ((split_length<<5)|
                (ctm_offset<<12)|
                (0 /*drop enable*/ <<13) |
                (bpe_start<<0)) );

    return bpe_start;
}

/** network_dma_init_bpe - Add a buffer pool entry to a buffer pool
 *
 * @param nbi_island    The NBI island number (8 or 9) to initialize
 * @param buffer_pool   Buffer pool number (0 to 7)
 * @param bpe           Number returned by previous network_dma_init_bpe or network_dma_init_bp
 * @param ctm_island    CTM island to add
 * @param pkt_credit    Packet credits
 * @param buf_credit    Buffer credits
 * 
 * Returns next available buffer pool entry number (to use with later call of network_dma_init_bpe)
 */
int
network_dma_init_bpe(int nbi_island, int buffer_pool, int bpe,
                     int ctm_island, int pkt_credit, int buf_credit)
{
    int i;
    int xpb_base;

    xpb_base = (1<<31) | (nbi_island<<24) | (0x10<<16); // NBI DMA

    xpb_write( xpb_base,
               0x40+(bpe<<2),
               ( (ctm_island<<21) | (pkt_credit<<10) | (buf_credit<<0) )
            );
    return bpe+1;
}


/** network_dma_init_bp_complete - Complete a buffer pool
 *
 * @param nbi_island    The NBI island number (8 or 9) to initialize
 * @param buffer_pool   Buffer pool number (0 to 7)
 * @param bpe           Buffer pool entry returned by network_dma_init_bpe
 */
void
network_dma_init_bp_complete(int nbi_island, int buffer_pool, int bpe)
{
    int xpb_base;
    int r;

    xpb_base = (1<<31) | (nbi_island<<24) | (0x10<<16); // NBI DMA

    r = xpb_read( xpb_base, 0x18 );
    xpb_write( xpb_base, 0x18, r | (1<<(bpe-1)) );
}


