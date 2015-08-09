#include <kern/e1000.h>
#include <inc/string.h>
#include <kern/pmap.h>
#include <kern/env.h>
#include <kern/picirq.h>
#include <kern/sched.h>

// TODO: try to separate tx/rx code at the final stage;

// LAB 6: Your driver code here
struct pci_func e1000_pci_func;
volatile uint32_t *e1000_mmio_beg;

// global definition of tx_desc list for 64 entries
struct tx_desc tx_desc_lst[TX_DESC_SZ];
// 64 entries, each 1518 bytes long
uint8_t tx_pkt_buffer_lst[TX_DESC_SZ][ETH_PKT_SZ]
__attribute__ ((aligned(PGSIZE)));

struct rx_desc rx_desc_lst[RX_NUM_OF_DESC];
uint8_t rx_pkt_buffer_lst[RX_NUM_OF_DESC][RX_DESC_SZ]
__attribute__ ((aligned(PGSIZE)));

// hard coded mac address for qemu; will be stored in RAL[0] and RAH[0]
// @note: info about those registers was somehow hidden for me in
//        documentation ;) i thought that mac addr should be stored in
//        RDBAH[0], even example e1000.h had defines for those regs;
//        however, documentation says that those regs are at 0x5400 and
//        0x5404 offsets, so these are the Receive Address Registers that
//        i was looking for;
// TODO: read mac from EEPROM;
uint8_t mac_addr[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };

static void init_tx(void)
{
	int i;

	// init e1000's tx registers (according to manual);
	// we need to operate on bare metal mem addresses - PADDR
	e1000_mmio_beg[E1000_TDBAL] = PADDR(&tx_desc_lst);
	e1000_mmio_beg[E1000_TDBAH] = 0;
	e1000_mmio_beg[E1000_TDLEN] = sizeof(tx_desc_lst);
	e1000_mmio_beg[E1000_TDH] = 0;
	e1000_mmio_beg[E1000_TDT] = 0;

	e1000_mmio_beg[E1000_TCTL] &= ~E1000_TCTL_CT;
	e1000_mmio_beg[E1000_TCTL] |=
				(E1000_TCTL_CT_VAL << E1000_TCTL_CT_SHIFT);
	e1000_mmio_beg[E1000_TCTL] &= ~E1000_TCTL_COLD;
	e1000_mmio_beg[E1000_TCTL] |=
				(E1000_TCTL_COLD_VAL << E1000_TCTL_COLD_SHIFT);
	e1000_mmio_beg[E1000_TCTL] |= (E1000_TCTL_EN | E1000_TCTL_PSP);

	// writing to this reg does not have any effect
	e1000_mmio_beg[E1000_TIPG] &= ~E1000_TIPG_IPGR1;
	e1000_mmio_beg[E1000_TIPG] |=
				(E1000_TIPG_IPGR1_VAL << E1000_TIPG_IPGR1_SHIFT);
	e1000_mmio_beg[E1000_TIPG] &= ~E1000_TIPG_IPGR2;
	e1000_mmio_beg[E1000_TIPG] |=
				(E1000_TIPG_IPGR2_VAL << E1000_TIPG_IPGR2_SHIFT);
	e1000_mmio_beg[E1000_TIPG] |= E1000_TIPG_IPGT_VAL;

	for (i = 0; i < TX_DESC_SZ; i++) {
		tx_desc_lst[i].addr = PADDR(tx_pkt_buffer_lst[i]);
		// force card to report status; dd bit in status will be
		// updated by card, so SW can rely on it;
		tx_desc_lst[i].cmd.bits.RS = 1;
		// initially set dd bit for the first run through tx ring;
		tx_desc_lst[i].status.bits.DD = 1;
	}
}

static void write_mac_addr(void)
{
	int offset, byte;

	for (offset = 0, byte = 0; offset < 32; offset += 8, byte++) {
		// RAL0 holds the lower 32 bits
		e1000_mmio_beg[E1000_RAL0] |= (mac_addr[byte] << offset);
		if (offset < 16)
			e1000_mmio_beg[E1000_RAH0] |=
				(mac_addr[byte + 4] << offset);
	}
}

static void init_rx(void)
{
	int i;

	// Head should point to the first valid receive descriptor in the
	// descriptor ring and tail should point to one descriptor beyond
	// the last valid descriptor in the descriptor ring?
	e1000_mmio_beg[E1000_RDH] = 0;
	e1000_mmio_beg[E1000_RDT] = RX_NUM_OF_DESC - 1;
	e1000_mmio_beg[E1000_RDBAL] = PADDR(&rx_desc_lst);
	e1000_mmio_beg[E1000_RDBAH] = 0;
	e1000_mmio_beg[E1000_RDLEN] = sizeof(rx_desc_lst);
	e1000_mmio_beg[E1000_RAH0] = 0;
	e1000_mmio_beg[E1000_RAL0] = 0;
	write_mac_addr();
	// address valid
	e1000_mmio_beg[E1000_RAH0] |= E1000_RAH_AV;

	// interrupt mask set
	e1000_mmio_beg[E1000_IMS] |= E1000_IMS_RXT0;
	/* e1000_mmio_beg[E1000_IMS] |= E1000_IMS_RXO; */
	/* e1000_mmio_beg[E1000_IMS] |= E1000_IMS_RXSEQ; */
	// long packet enable
	e1000_mmio_beg[E1000_RCTL] &= ~E1000_RCTL_LPE;
	// loppback mode
	e1000_mmio_beg[E1000_RCTL] |= E1000_RCTL_LBM_NO;
	// broadcast accept mode
	e1000_mmio_beg[E1000_RCTL] |= E1000_RCTL_BAM;
	// buffer size extended
	e1000_mmio_beg[E1000_RCTL] &= ~E1000_RCTL_BSEX;
	e1000_mmio_beg[E1000_RCTL] |= E1000_RCTL_SZ_4096;
	// strip ethernet crc
	e1000_mmio_beg[E1000_RCTL] |= E1000_RCTL_SECRC;
	// receiver enable
	e1000_mmio_beg[E1000_RCTL] |= E1000_RCTL_EN;

	// multicast table array
	/* for (i = 0; i < 128; i++) */
	/* 	e1000_mmio_beg[E1000_MTA + (i * 4)] = 0; */

	for (i = 0; i < RX_NUM_OF_DESC; i++)
		rx_desc_lst[i].addr = PADDR(rx_pkt_buffer_lst[i]);
}

int e1000_attach(struct pci_func *f)
{
	pci_func_enable(f);
	memmove(&e1000_pci_func, f, sizeof(struct pci_func));
	e1000_mmio_beg = mmio_map_region(f->reg_base[0],
	                                 f->reg_size[0]);

	init_tx();
	init_rx();

	return 0;
}

static void clear_nic_irqs(void)
{
	e1000_mmio_beg[E1000_ICR] |= E1000_IMS_RXT0;
	// watch out for calling irq_eoi() before clearing rx irq in reg
	lapic_eoi();
	irq_eoi();
}

void nic_irq_handler(void)
{
	struct Env *recver;

	// if currently there is no environment waiting for packets,
	// silently clear the irq and continue JOS's working;
	if ((recver = env_net_recver()) == NULL) {
		clear_nic_irqs();
		return;
	}
	// found some env that is receiving packet -- let's check
	// whether there is some address for receiving packet and then
	// call the receive function;
	if (recver->env_net_dstva)
		rx_pkt(recver);
}

// points to the packet ready for processing
static int rx_idx_ready;

int rx_pkt(struct Env *e)
{
	int idx_next;
	int ret;

	// check the DD bit in the next descriptor
	if (!rx_desc_lst[rx_idx_ready].status.bits.DD)
		return -E_E1000_NOT_RX;

	// note: for now, ignore the status.EOP, since we do not
	//       accept jumbo frames (RCTL.LPE = 0);
	ret = rx_desc_lst[rx_idx_ready].length;
	// clear status field
	rx_desc_lst[rx_idx_ready].status.raw = 0;

	// actual copy of packet surrounded with lcr3 calls in order to
	// have access to the dst page and then going back to curenv's page dir;
	lcr3(PADDR(e->env_pgdir));
	memmove(e->env_net_dstva, rx_pkt_buffer_lst[rx_idx_ready], ret);
	lcr3(PADDR(curenv->env_pgdir));

	// make rx_idx_ready point to next descriptor in rx ring
	rx_idx_ready = (rx_idx_ready + 1) % RX_NUM_OF_DESC;
	//
	// compute the next index value that will be stored in RDT register;
	idx_next = (e1000_mmio_beg[E1000_RDT] + 1) % RX_NUM_OF_DESC;
	e1000_mmio_beg[E1000_RDT] = idx_next;

	// mark env as runnable, not receiving packets and store number of
	// copied bytes in eax register;
	e->env_status = ENV_RUNNABLE;
	e->env_net_recving = 0;
	e->env_net_value = ret;
	e->env_tf.tf_regs.reg_eax = ret;

	clear_nic_irqs();

	return ret;
}

int tx_pkt(const char *data, uint8_t nbytes)
{
	int idx, next;

	idx = e1000_mmio_beg[E1000_TDT];
	// compute the index of the next descriptor in tx ring
	next = (idx + 1) % TX_DESC_SZ;

	// check the DD bit in the next descriptor
	if (!tx_desc_lst[next].status.bits.DD)
		return -E_E1000_NOT_TX;

	if (nbytes >= ETH_PKT_SZ)
		return -E_INVAL;

	// DD was set? next descriptor is free, we are good to go with
	// initializing the current descriptor and then incrementing TDT;
	// then, TDT will point to the next free descriptor;
	memmove(tx_pkt_buffer_lst[idx], data, nbytes);
	tx_desc_lst[idx].length = nbytes;
	tx_desc_lst[idx].cmd.bits.RS = 1;
	tx_desc_lst[idx].cmd.bits.EOP = 1;
	// store in TDT the index of the next free descriptor;
	e1000_mmio_beg[E1000_TDT] = next;
	return 0;
}
