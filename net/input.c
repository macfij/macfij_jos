#include "ns.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";
	int r;
	int perm = PTE_P|PTE_U|PTE_W;

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	while (1) {
		if ((r = sys_page_alloc(sys_getenvid(), &nsipcbuf, perm)) < 0)
			panic("sys_page_alloc: %e", r);
		r = 0;
		while ((r = sys_rx_data(&nsipcbuf)) == -E_E1000_NOT_RX)
			;
		if (!r)
			continue;
		memcpy(nsipcbuf.pkt.jp_data, &nsipcbuf, r);
		nsipcbuf.pkt.jp_len = r;
		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf.pkt, perm);
	}
}
