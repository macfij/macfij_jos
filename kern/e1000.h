#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>
#include <kern/env.h>
#include <inc/error.h>

// maybe i should even make source files for tx and rx instead of
// keeping it at one place?

/*****************************************************************************
 *
 *                    TX STUFF
 *
 *****************************************************************************
 */
#define TX_DESC_SZ 64
#define ETH_PKT_SZ 1518

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
		uint8_t raw;
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
		uint8_t raw;
	} status;
	uint8_t css;
	uint16_t special;
};

/*****************************************************************************
 *
 *                    RX STUFF
 *
 *****************************************************************************
 */
#define RX_NUM_OF_DESC 128
#define RX_DESC_SZ     4096

#define E1000_RDBAL    (0x02800/4) /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    (0x02804/4) /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    (0x02808/4) /* RX Descriptor Length - RW */
#define E1000_RDH      (0x02810/4) /* RX Descriptor Head - RW */
#define E1000_RDT      (0x02818/4) /* RX Descriptor Tail - RW */
#define E1000_RDTR     (0x02820/4) /* RX Delay Timer - RW */
#define E1000_RCTL     (0x00100/4) /* RX Control - RW */
#define E1000_RAL0     (0x05400/4) /* Receive Address - RW Array */
#define E1000_RAH0     (0x05404/4) /* Receive Address - RW Array */
#define E1000_IMC      (0x000D8/4) /* Interrupt Mask Clear - WO */
#define E1000_ICR      (0x000C0/4)  /* Interrupt Cause Read - R/clr */

#define E1000_RDBAL0   E1000_RDBAL /* RX Desc Base Address Low (0) - RW */
#define E1000_RDBAH0   E1000_RDBAH /* RX Desc Base Address High (0) - RW */
#define E1000_RDLEN0   E1000_RDLEN /* RX Desc Length (0) - RW */
#define E1000_RDH0     E1000_RDH   /* RX Desc Head (0) - RW */
#define E1000_RDT0     E1000_RDT   /* RX Desc Tail (0) - RW */
#define E1000_RDTR0    E1000_RDTR  /* RX Delay Timer (0) - RW */
#define E1000_RDTR1    0x02820  /* RX Delay Timer (1) - RW */
#define E1000_RDBAL1   0x02900  /* RX Descriptor Base Address Low (1) - RW */
#define E1000_RDBAH1   0x02904  /* RX Descriptor Base Address High (1) - RW */
#define E1000_RDLEN1   0x02908  /* RX Descriptor Length (1) - RW */
#define E1000_RDH1     0x02910  /* RX Descriptor Head (1) - RW */
#define E1000_RDT1     0x02918  /* RX Descriptor Tail (1) - RW */
/* Receive Control */
#define E1000_RCTL_RST            0x00000001    /* Software reset */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_SBP            0x00000004    /* store bad packet */
#define E1000_RCTL_UPE            0x00000008    /* unicast promiscuous enable */
#define E1000_RCTL_MPE            0x00000010    /* multicast promiscuous enab */
#define E1000_RCTL_LPE            0x00000020    /* long packet enable */
#define E1000_RCTL_LBM_NO         0x00000000    /* no loopback mode */
#define E1000_RCTL_LBM_MAC        0x00000040    /* MAC loopback mode */
#define E1000_RCTL_LBM_SLP        0x00000080    /* serial link loopback mode */
#define E1000_RCTL_LBM_TCVR       0x000000C0    /* tcvr loopback mode */
#define E1000_RCTL_DTYP_MASK      0x00000C00    /* Descriptor type mask */
#define E1000_RCTL_DTYP_PS        0x00000400    /* Packet Split descriptor */
#define E1000_RCTL_RDMTS_HALF     0x00000000    /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_QUAT     0x00000100    /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_EIGTH    0x00000200    /* rx desc min threshold size */
#define E1000_RCTL_MO_SHIFT       12            /* multicast offset shift */
#define E1000_RCTL_MO_0           0x00000000    /* multicast offset 11:0 */
#define E1000_RCTL_MO_1           0x00001000    /* multicast offset 12:1 */
#define E1000_RCTL_MO_2           0x00002000    /* multicast offset 13:2 */
#define E1000_RCTL_MO_3           0x00003000    /* multicast offset 15:4 */
#define E1000_RCTL_MDR            0x00004000    /* multicast desc ring 0 */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SZ_1024        0x00010000    /* rx buffer size 1024 */
#define E1000_RCTL_SZ_512         0x00020000    /* rx buffer size 512 */
#define E1000_RCTL_SZ_256         0x00030000    /* rx buffer size 256 */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 1 */
#define E1000_RCTL_SZ_16384       0x00010000    /* rx buffer size 16384 */
#define E1000_RCTL_SZ_8192        0x00020000    /* rx buffer size 8192 */
#define E1000_RCTL_SZ_4096        0x00030000    /* rx buffer size 4096 */
#define E1000_RCTL_VFE            0x00040000    /* vlan filter enable */
#define E1000_RCTL_CFIEN          0x00080000    /* canonical form enable */
#define E1000_RCTL_CFI            0x00100000    /* canonical form indicator */
#define E1000_RCTL_DPF            0x00400000    /* discard pause frames */
#define E1000_RCTL_PMCF           0x00800000    /* pass MAC control frames */
#define E1000_RCTL_BSEX           0x02000000    /* Buffer size extension */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */
#define E1000_RCTL_FLXBUF_MASK    0x78000000    /* Flexible buffer size */
#define E1000_RCTL_FLXBUF_SHIFT   27            /* Flexible buffer shift */

#define E1000_RAH_AV              0x80000000    /* RAH addres valid 31 */

/* Interrupt Cause Read */
#define E1000_ICR_TXDW          0x00000001 /* Transmit desc written back */
#define E1000_ICR_TXQE          0x00000002 /* Transmit Queue empty */
#define E1000_ICR_LSC           0x00000004 /* Link Status Change */
#define E1000_ICR_RXSEQ         0x00000008 /* rx sequence error */
#define E1000_ICR_RXDMT0        0x00000010 /* rx desc min. threshold (0) */
#define E1000_ICR_RXO           0x00000040 /* rx overrun */
#define E1000_ICR_RXT0          0x00000080 /* rx timer intr (ring 0) */
#define E1000_ICR_MDAC          0x00000200 /* MDIO access complete */
#define E1000_ICR_RXCFG         0x00000400 /* RX /c/ ordered set */
#define E1000_ICR_GPI_EN0       0x00000800 /* GP Int 0 */
#define E1000_ICR_GPI_EN1       0x00001000 /* GP Int 1 */
#define E1000_ICR_GPI_EN2       0x00002000 /* GP Int 2 */
#define E1000_ICR_GPI_EN3       0x00004000 /* GP Int 3 */
#define E1000_ICR_TXD_LOW       0x00008000
#define E1000_ICR_SRPD          0x00010000
#define E1000_ICR_ACK           0x00020000 /* Receive Ack frame */
#define E1000_ICR_MNG           0x00040000 /* Manageability event */
#define E1000_ICR_DOCK          0x00080000 /* Dock/Undock */
#define E1000_ICR_INT_ASSERTED  0x80000000 /* If this bit asserted, the driver should claim the interrupt */
#define E1000_ICR_RXD_FIFO_PAR0 0x00100000 /* queue 0 Rx descriptor FIFO parity error */
#define E1000_ICR_TXD_FIFO_PAR0 0x00200000 /* queue 0 Tx descriptor FIFO parity error */
#define E1000_ICR_HOST_ARB_PAR  0x00400000 /* host arb read buffer parity error */
#define E1000_ICR_PB_PAR        0x00800000 /* packet buffer parity error */
#define E1000_ICR_RXD_FIFO_PAR1 0x01000000 /* queue 1 Rx descriptor FIFO parity error */
#define E1000_ICR_TXD_FIFO_PAR1 0x02000000 /* queue 1 Tx descriptor FIFO parity error */
#define E1000_ICR_ALL_PARITY    0x03F00000 /* all parity error bits */
#define E1000_ICR_DSW           0x00000020 /* FW changed the status of DISSW bit in the FWSM */
#define E1000_ICR_PHYINT        0x00001000 /* LAN connected device generates an interrupt */
#define E1000_ICR_EPRST         0x00100000 /* ME handware reset occurs */

/* Interrupt Mask Set */
#define E1000_IMS_TXDW      E1000_ICR_TXDW      /* Transmit desc written back */
#define E1000_IMS_TXQE      E1000_ICR_TXQE      /* Transmit Queue empty */
#define E1000_IMS_LSC       E1000_ICR_LSC       /* Link Status Change */
#define E1000_IMS_RXSEQ     E1000_ICR_RXSEQ     /* rx sequence error */
#define E1000_IMS_RXDMT0    E1000_ICR_RXDMT0    /* rx desc min. threshold */
#define E1000_IMS_RXO       E1000_ICR_RXO       /* rx overrun */
#define E1000_IMS_RXT0      E1000_ICR_RXT0      /* rx timer intr */
#define E1000_IMS_MDAC      E1000_ICR_MDAC      /* MDIO access complete */
#define E1000_IMS_RXCFG     E1000_ICR_RXCFG     /* RX /c/ ordered set */
#define E1000_IMS_GPI_EN0   E1000_ICR_GPI_EN0   /* GP Int 0 */
#define E1000_IMS_GPI_EN1   E1000_ICR_GPI_EN1   /* GP Int 1 */
#define E1000_IMS_GPI_EN2   E1000_ICR_GPI_EN2   /* GP Int 2 */
#define E1000_IMS_GPI_EN3   E1000_ICR_GPI_EN3   /* GP Int 3 */
#define E1000_IMS_TXD_LOW   E1000_ICR_TXD_LOW
#define E1000_IMS_SRPD      E1000_ICR_SRPD
#define E1000_IMS_ACK       E1000_ICR_ACK       /* Receive Ack frame */
#define E1000_IMS_MNG       E1000_ICR_MNG       /* Manageability event */
#define E1000_IMS_DOCK      E1000_ICR_DOCK      /* Dock/Undock */
#define E1000_IMS_RXD_FIFO_PAR0 E1000_ICR_RXD_FIFO_PAR0 /* queue 0 Rx descriptor FIFO parity error */
#define E1000_IMS_TXD_FIFO_PAR0 E1000_ICR_TXD_FIFO_PAR0 /* queue 0 Tx descriptor FIFO parity error */
#define E1000_IMS_HOST_ARB_PAR  E1000_ICR_HOST_ARB_PAR  /* host arb read buffer parity error */
#define E1000_IMS_PB_PAR        E1000_ICR_PB_PAR        /* packet buffer parity error */
#define E1000_IMS_RXD_FIFO_PAR1 E1000_ICR_RXD_FIFO_PAR1 /* queue 1 Rx descriptor FIFO parity error */
#define E1000_IMS_TXD_FIFO_PAR1 E1000_ICR_TXD_FIFO_PAR1 /* queue 1 Tx descriptor FIFO parity error */
#define E1000_IMS_DSW       E1000_ICR_DSW
#define E1000_IMS_PHYINT    E1000_ICR_PHYINT
#define E1000_IMS_EPRST     E1000_ICR_EPRST

/* Interrupt Mask Clear */
#define E1000_IMC_TXDW      E1000_ICR_TXDW      /* Transmit desc written back */
#define E1000_IMC_TXQE      E1000_ICR_TXQE      /* Transmit Queue empty */
#define E1000_IMC_LSC       E1000_ICR_LSC       /* Link Status Change */
#define E1000_IMC_RXSEQ     E1000_ICR_RXSEQ     /* rx sequence error */
#define E1000_IMC_RXDMT0    E1000_ICR_RXDMT0    /* rx desc min. threshold */
#define E1000_IMC_RXO       E1000_ICR_RXO       /* rx overrun */
#define E1000_IMC_RXT0      E1000_ICR_RXT0      /* rx timer intr */
#define E1000_IMC_MDAC      E1000_ICR_MDAC      /* MDIO access complete */
#define E1000_IMC_RXCFG     E1000_ICR_RXCFG     /* RX /c/ ordered set */
#define E1000_IMC_GPI_EN0   E1000_ICR_GPI_EN0   /* GP Int 0 */
#define E1000_IMC_GPI_EN1   E1000_ICR_GPI_EN1   /* GP Int 1 */
#define E1000_IMC_GPI_EN2   E1000_ICR_GPI_EN2   /* GP Int 2 */
#define E1000_IMC_GPI_EN3   E1000_ICR_GPI_EN3   /* GP Int 3 */
#define E1000_IMC_TXD_LOW   E1000_ICR_TXD_LOW
#define E1000_IMC_SRPD      E1000_ICR_SRPD
#define E1000_IMC_ACK       E1000_ICR_ACK       /* Receive Ack frame */
#define E1000_IMC_MNG       E1000_ICR_MNG       /* Manageability event */
#define E1000_IMC_DOCK      E1000_ICR_DOCK      /* Dock/Undock */
#define E1000_IMC_RXD_FIFO_PAR0 E1000_ICR_RXD_FIFO_PAR0 /* queue 0 Rx descriptor FIFO parity error */
#define E1000_IMC_TXD_FIFO_PAR0 E1000_ICR_TXD_FIFO_PAR0 /* queue 0 Tx descriptor FIFO parity error */
#define E1000_IMC_HOST_ARB_PAR  E1000_ICR_HOST_ARB_PAR  /* host arb read buffer parity error */
#define E1000_IMC_PB_PAR        E1000_ICR_PB_PAR        /* packet buffer parity error */
#define E1000_IMC_RXD_FIFO_PAR1 E1000_ICR_RXD_FIFO_PAR1 /* queue 1 Rx descriptor FIFO parity error */
#define E1000_IMC_TXD_FIFO_PAR1 E1000_ICR_TXD_FIFO_PAR1 /* queue 1 Tx descriptor FIFO parity error */
#define E1000_IMC_DSW       E1000_ICR_DSW
#define E1000_IMC_PHYINT    E1000_ICR_PHYINT
#define E1000_IMC_EPRST     E1000_ICR_EPRST

struct rx_desc {
	uint64_t addr;
	uint16_t length;
	uint16_t packet_chksum;
	// | PIF | IPCS | TCPCS | RSV | VP | IXSM | EOP | DD |
	union {
		uint8_t raw;
		struct {
			uint8_t DD:1;
			uint8_t EOP:1;
			uint8_t IXSM:1;
			uint8_t VP:1;
			uint8_t RSV:1;
			uint8_t TCPCS:1;
			uint8_t IPCS:1;
			uint8_t PIF:1;
		} bits;
	} status;
	// | RXE | IPE | TCPE | RSV_CXE | RSV | SEQ_RSV | SE_RSV | CE |
	union {
		uint8_t raw;
		struct {
			uint8_t CE:1;
			uint8_t SE_RSV:1;
			uint8_t SEQ_RSV:1;
			uint8_t RSV:1;
			uint8_t RSV_CXE:1;
			uint8_t TCPE:1;
			uint8_t IPE:1;
			uint8_t RXE:1;
		} bits;
	} errors;
	uint16_t special;
};

/*****************************************************************************
 *
 *                    MISC
 *
 *****************************************************************************
 */
//82540EM-A
#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100e
#define E1000_IMS      (0x000D0/4) /* Interrupt Mask Set - RW */
#define E1000_MTA      (0x05200/4) /* Multicast Table Array - RW Array */

int e1000_attach(struct pci_func *pcif);
extern struct pci_func e1000_pci_func;

int tx_pkt(const char *data, uint8_t nbytes);
extern struct tx_desc tx_desc_lst[];
extern uint8_t pkt_buffer_lst[][ETH_PKT_SZ];

void nic_irq_handler(void);
int rx_pkt(struct Env *);
#endif	// JOS_KERN_E1000_H
