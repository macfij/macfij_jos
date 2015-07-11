#include <kern/e1000.h>
#include <inc/string.h>
#include <kern/pmap.h>

// computes the offset from mmio beginning to the desired register;
// operations on pointers are involved, hence we need to divide the actual
// register address;
// @note: look at lapic.c and decide whether it is better to use registers
//        as array indices of e1000_mmio_beg;
#define MMIO_REG_ADDR(beg, reg) (uint32_t *)(beg + (reg / sizeof(uint32_t *)))

// LAB 6: Your driver code here
struct pci_func e1000_pci_func;
volatile uint32_t *e1000_mmio_beg;

// global definition of tx_desc list for 64 entries
struct tx_desc tx_desc_lst[TX_DESC_SZ];

// 64 entries, each 1518 bytes long
uint8_t pkt_buffer_lst[TX_DESC_SZ][ETH_PKT_SZ];

static void init_tx(void)
{
	int i;

	for (i = 0; i < TX_DESC_SZ; i++) {
		tx_desc_lst[i].addr = PADDR(pkt_buffer_lst[i]);
		// force card to report status; dd bit in status will be
		// updated by card, so SW can rely on it;
		tx_desc_lst[i].cmd.bits.RS = 1;
		// initially set dd bit for the first run through tx ring;
		tx_desc_lst[i].status.bits.DD = 1;
	}
}

int e1000_attach(struct pci_func *f)
{
	pci_func_enable(f);
	memmove(&e1000_pci_func, f, sizeof(struct pci_func));
	e1000_mmio_beg = mmio_map_region(f->reg_base[0],
	                                 f->reg_size[0]);
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

	init_tx();

	return 0;
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
	memmove(pkt_buffer_lst[idx], data, nbytes);
	tx_desc_lst[idx].length = nbytes;
	tx_desc_lst[idx].cmd.bits.RS = 1;
	tx_desc_lst[idx].cmd.bits.EOP = 1;
	// store in TDT the index of the next free descriptor;
	e1000_mmio_beg[E1000_TDT] = next;
	return 0;
}
