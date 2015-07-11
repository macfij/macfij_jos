#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>
#include <inc/error.h>
#define TX_DESC_SZ 64
#define ETH_PKT_SZ 1518

//82540EM-A
#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100e

// divide by 4 in order to use them as array indices
#define E1000_TCTL     (0x00400/4) /* TX Control - RW */
#define E1000_TCTL_EXT (0x00404/4) /* Extended TX Control - RW */
#define E1000_TIPG     (0x00410/4) /* TX Inter-packet gap -RW */

#define E1000_TDBAL    (0x03800/4) /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    (0x03804/4) /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    (0x03808/4) /* TX Descriptor Length - RW */
#define E1000_TDH      (0x03810/4) /* TX Descriptor Head - RW */
#define E1000_TDT      (0x03818/4) /* TX Descripotr Tail - RW */

/* Transmit Control */
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000    /* Multiple request support */

// TCTL register's layout:
// 31         26 25         22 21    12 11   4 3            0
// |  reserved  |  cntl bits  |  COLD  |  CT  |  cntl bits  |
#define E1000_TCTL_CT_SHIFT 3
#define E1000_TCTL_CT_VAL 0x10
#define E1000_TCTL_COLD_SHIFT 11
#define E1000_TCTL_COLD_VAL 0x40

// TIPG register's layout:
// 31         30 29         20 19         10 9            0
// |  reserved  |    IPGR2    |    IPGR1    |    IPGT     |
#define E1000_TIPG_IPGR1   0x0007fe00
#define E1000_TIPG_IPGR2   0x3ff00000
#define E1000_TIPG_IPGR1_SHIFT 9
#define E1000_TIPG_IPGR1_VAL 0x4
#define E1000_TIPG_IPGR2_SHIFT 19
#define E1000_TIPG_IPGR2_VAL 0x6
#define E1000_TIPG_IPGT_VAL 0xa

struct tx_desc {
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	// | IDE | VLE | DEXT | RSV | RS | IC | IFCS | EOP |
	union {
		struct {
			uint8_t EOP:1;
			uint8_t IFCS:1;
			uint8_t IC:1;
			uint8_t RS:1;
			uint8_t RSV:1;
			uint8_t DEXT:1;
			uint8_t VLE:1;
			uint8_t IDE:1;
		} bits;
		uint8_t byte;
	} cmd;
	// | TU | LC | EC | DD |
	union {
		struct {
			uint8_t DD:1;
			uint8_t EC:1;
			uint8_t LC:1;
			uint8_t TU:1;
			uint8_t rsvd:4;
		} bits;
		uint8_t byte;
	} status;
	uint8_t css;
	uint16_t special;
};

int e1000_attach(struct pci_func *pcif);
int tx_pkt(const char *data, uint8_t nbytes);
extern struct pci_func e1000_pci_func;
extern struct tx_desc tx_desc_lst[];
extern uint8_t pkt_buffer_lst[][ETH_PKT_SZ];

#endif	// JOS_KERN_E1000_H
