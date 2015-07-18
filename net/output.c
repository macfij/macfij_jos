#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";
	int val;

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	while (1) {
		val = ipc_recv(0, &nsipcbuf, 0);
		if (val == NSREQ_OUTPUT)
			sys_tx_data(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
	}
}
