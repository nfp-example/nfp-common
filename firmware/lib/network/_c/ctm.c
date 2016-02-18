#include "nfp/me.h"
#include <nfp6000/nfp_override_6000.h>

/** network_ctm_init - Initialize a CTM in an island
 * @param ctm_island   Island whose CTM is to be initialized
 * @param pe_config    Packet engine config for the CTM
 * A value of 0 in pe_config uses the whole CTM for packet buffers
 */
void network_ctm_init(int ctm_island, int pe_config)
{
    int xpb_base;

    xpb_base = (1<<31) | (ctm_island<<24) | (0x07<<16); // CTM packet engine
    xpb_write( xpb_base,  0x0, pe_config );
}

/** ctm_pkt_free
 * Free a packet in a CTM island
 * Can be used in normal operation, or in the CTM reinit process to
 * flush the CTM packet/work queue
 * 
 * @param ctm_island      CTM island in which to free packet
 * @param pkt             packet number in CTM to free
 * 
 */
__intrinsic void
ctm_pkt_free(int ctm_island, int pkt)
{
    int addr_hi;
    addr_hi = ctm_island<<24;

    __asm {
        mem[packet_free, --, addr_hi, <<8, pkt];
    }
}

/** ctm_fake_add_thread
 * Add a fake thread to the CTM work queue
 *
 * Use this to tidy up the CTM work queue at the end
 */
__intrinsic void
ctm_fake_add_thread(int ctm_island, int signal_num)
{
    uint32_t sig_num_override;
    uint32_t override;
    __xread uint32_t nbi_meta[6];
    uint32_t zero=0;
    SIGNAL sig;

    int addr_hi;
    addr_hi = ctm_island<<24;

    /* Force override of signal number within context
     */
    sig_num_override = signal_num<<9;
    local_csr_write(local_csr_cmd_indirect_ref0, sig_num_override);
    override = ovr_signal_number_bit;

    /* Add the thread
     */
    __asm {
        alu[ --, --, B, override ];
        mem[packet_add_thread, nbi_meta, addr_hi, <<8, 0, (sizeof(nbi_meta)>>2)], indirect_ref;
    }
    __implicit_read(&nbi_meta);
}

/** ctm_alloc_pkt
 * Alloc a packet
 *
 * Always allocates a 2kB packet buffer
 */
__intrinsic int
ctm_alloc_pkt(int ctm_island)
{
    SIGNAL sig;
    __xread uint32_t palloc;
    uint32_t pnum;
    int credit_bucket = 0; // ME uses credit bucket 0

    int addr_hi;
    addr_hi = ctm_island<<24;

    __asm {
        mem[ packet_alloc_poll, palloc, addr_hi, <<8, credit_bucket, 3], ctx_swap[sig] ;
    }

    if (palloc==0xffffffff) {
        __asm ctx_arb[bpt];
    }

    pnum = (palloc>>20) &0x1ff;
    pnum = palloc;
    return pnum;
}

/** ctm_fake_packet_rx
 * Fake a CTM packet receive
 *
 * Use this to tidy up the CTM work queue at the end
 */
__intrinsic void
ctm_fake_packet_rx(int ctm_island)
{
    int island_and_dm;
    int override;
    int zero=0;
    volatile __xread uint32_t xfer[2]; /* xfer has to be volatile to stop the compiler
                                          removing the assembler */

    int pnum;
    int i;

    int addr_hi;
    addr_hi = ctm_island<<24;

    /* Free packet 0 and allocate a packet (will be packet 0)
       If packet number 0 is NOT allocated then the fake pkt receive DOES NOT WORK
    */
    ctm_pkt_free(ctm_island, 0);
    pnum = ctm_alloc_pkt(ctm_island);
    ctm_pkt_free(ctm_island, 0);

    /* Set data master/ref to be the CLS' island CTM
     */
    island_and_dm = ( (0<<24) |
                      /*(1<<16) | /* sig master of 4 bits, pnum=16 */
                      (0<<16) | /* sig master of 4 bits, pnum=0 */
                      (3<<9)  | /* sig ref of 3 to indicate last*/
                      (0<<0) ); /* byte mask not used */
    local_csr_write(local_csr_cmd_indirect_ref0, island_and_dm );

    /* Force override of data master (to be 2) ref (to be 1), signal master/ref
     */
    override = ( (((2<<14)|(1<<0))<<16) |
                 ovr_data_16bit_imm_bit |
                 ovr_signal_ctx_bit |
                 ovr_signal_number_bit |
                 ovr_signal_master_bit  );
    __asm {
        alu[ --, --, B, override ];
        cls[read, xfer, addr_hi, <<8, 0, 2], indirect_ref;
    }

    /* Sleep to make sure the CTM has received the push data from the CLS before we move on */
    me_poll_sleep(200);
}

/** ctm_empty_pkt_queue
 * Empty the CTM island packet queue by waiting for packets
 *
 * Use this to tidy up the CTM work queue at the end
 * Leaves at least one thread on the CTM work queue
 */
static int
ctm_empty_pkt_queue(int ctm_island, int timeout)
{
    int signal_num=6;
    int count=0; /* Number of packets removed from CTM */
    for (;;) {
        me_clear_all_signals();
        ctm_fake_add_thread(ctm_island, signal_num);
        if (!me_poll_wait_for_sig_with_timeout(signal_num, timeout)) {
            break;
        }
        count++;
    }
    return count;
}

/** ctm_empty_work_queue
 * Assuming CTM has only threads in its work queue, clear them out
 *
 * Use this to tidy up the CTM work queue at the end
 * It is clean to do this by adding a packet then this thread signal 1 len(workq) times
 * and then adding a packet and this thread signal 2
 * and then checking signal 2 with timeout, and adding a packet on timeout, repeating
 * until signal 2 is set.
 */
/* HW has 256+4+2 */
#define CTM_WORKQ_LENGTH (270)
int
ctm_empty_work_queue(int ctm_island, int timeout)
{
    int i, count;

    /* This may run too fast at present - may want to pace it
     */
    /* Keep the CTM thread queue the same length but make it known
     * (non signal 2) threads */
    for (i=0; i<CTM_WORKQ_LENGTH; i++) {
        ctm_fake_packet_rx(ctm_island);
        ctm_fake_add_thread(ctm_island, 1);
    }

    /* Clear signals in case this thread signal 2 had been on the CTM
     * thread queue */
    me_clear_all_signals();

    /* Keep CTM thread queue the same length but have this thread
     * signal 2 on the end */
    ctm_fake_packet_rx(ctm_island);
    ctm_fake_add_thread(ctm_island, 2);

    /* Add packets as necessary until doing so returns signal 2 */
    count=0;
    while (me_poll_wait_for_sig_with_timeout(2, timeout)==0) {
        ctm_fake_packet_rx(ctm_island);
        count++;
    }

    return count;
}

/** network_ctm_cleanup
 * @param ctm_island   Island whose CTM packet/work queue is to be cleared
 * @param timeout      Value of 2000 is good...
 *
 * Expects network_npc_control() to have been called to disable packet delivery to NBI DMA
 */
void network_ctm_cleanup(int ctm_island, int timeout)
{
    int pkts_freed;
    int threads_freed;
    int i, j;

    /* Free all the packets in the CTM
       Does not barf if the packets are already freed
       Repeat, rinse, repeat in case the NBI DMA wants to give more packets during this process
    */
    for (j=0; j<3; j++) {
        for (i=256; i>=0; i--) {
            ctm_pkt_free(ctm_island, i);
        }
    }

    pkts_freed    = ctm_empty_pkt_queue(ctm_island, timeout);
    threads_freed = ctm_empty_work_queue(ctm_island, timeout);
}

