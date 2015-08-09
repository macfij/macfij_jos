#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";
	int val;
	envid_t from;

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	while (1) {
		val = ipc_recv(&from, &nsipcbuf, 0);
		if (val != NSREQ_OUTPUT)
			continue;
		if (from != ns_envid)
			panic("from != ns");
		sys_tx_data(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
	}
}
