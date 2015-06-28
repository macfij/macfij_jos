// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
// dst addr must be PTE_COW; it will be replaced with created
// page within this function;
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r, perm = PTE_P | PTE_U | PTE_W;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if (!(uvpt[PGNUM(addr)] & PTE_P))
		panic("PTE not present in uvpt");
	if (!(err & FEC_WR) || (!(PGOFF(uvpt[PGNUM(addr)]) & (PTE_W |PTE_COW))))
		panic("faulting access was not write or not to COW\n");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	addr = ROUNDDOWN(addr, PGSIZE);
	// allocate space for our page at PFTEMP
	r = sys_page_alloc(0, PFTEMP, perm);
	if (r < 0)
		panic("pgfault()'s sys_page_alloc: %e", r);
	// move old contents to new page
	memmove(PFTEMP, addr, PGSIZE);
	// map created page at old addr
	r = sys_page_map(0, PFTEMP, 0, addr, perm);
	if (r < 0)
		panic("pgfault()'s sys_page_map: %e", r);
	// unmap temporary page (which was used as buffer) from currenv;
	// if refcount of page at PFTEMP == 0, then it will be avail as
	// free page (and added to pg_free_list or smth); this step is
	// crucial for proper working of pgfault() on next invocations;
	r = sys_page_unmap(0, PFTEMP);
	if (r < 0)
		panic("pgfault()'s sys_page_map: %e", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	void *addr = (void *)(pn << PGSHIFT);
	pte_t pte = uvpt[pn];
	int perm = PTE_P | PTE_U;

	// LAB 4: Your code here.
	if (pte & PTE_SYSCALL && pte & PTE_SHARE) {
		perm |= PTE_W | PTE_SHARE;
		if ((r = sys_page_alloc(envid, addr, perm)) < 0)
			panic("duppage: sys_page_alloc: %e", r);
		if ((r = sys_page_map(0, addr, envid, addr, perm)) < 0)
			panic("duppage: sys_page_map: %e", r);
		return 0;
	}
	if (pte & PTE_W || pte & PTE_COW)
		perm |= PTE_COW;
	// src va onto dst va (thisenv ---> envid)
	r = sys_page_map(0, addr, envid, addr, perm);
	if (r < 0)
		panic("duppage()'s sys_page_map: %e", r);
	r = sys_page_map(0, addr, 0, addr, perm);
	if (r < 0)
		panic("duppage()'s sys_page_map: %e", r);
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	envid_t e;
	int r, i, j;
	uintptr_t addr;

	set_pgfault_handler(pgfault);
	e = sys_exofork();
	if (e < 0)
		panic("sys exofork error %e\n", e);
	if (!e) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	for (addr = UTEXT; addr < UTOP; addr += PGSIZE) {
		if (!(uvpd[PDX(addr)] & PTE_P))
			continue;
		if (addr == UXSTACKTOP - PGSIZE) {
			sys_page_alloc(e, (void *)UXSTACKTOP - PGSIZE,
					PTE_P | PTE_U | PTE_W);
			continue;
		}
		if (uvpt[PGNUM(addr)] & PTE_P)
			duppage(e, PGNUM(addr));
	}

	// copy parent's upcall to child's
	r = sys_env_set_pgfault_upcall(e, thisenv->env_pgfault_upcall);
	if (r < 0)
		panic("sys_env_set_pgfault_upcall: %e", r);
	if ((r = sys_env_set_status(e, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);
	return e;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
