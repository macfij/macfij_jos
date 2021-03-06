/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>
#include <kern/e1000.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	user_mem_assert(curenv, (const void *)s, len, PTE_P | PTE_U);

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	if (e == curenv)
		cprintf("[%08x] exiting gracefully\n", curenv->env_id);
	else
		cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 4: Your code here.
	struct Env *e;
	int ret = env_alloc(&e, thiscpu->cpu_env->env_id);
	if (ret < 0) {
		return ret;
	}
	e->env_status = ENV_NOT_RUNNABLE;
	memcpy(&e->env_tf, &thiscpu->cpu_env->env_tf, sizeof(e->env_tf));
	// eax holds return value; newly created child will return 0
	e->env_tf.tf_regs.reg_eax = 0;
	return e->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.
	struct Env *e;
	int ret = envid2env(envid, &e, 1);
	if (ret < 0) {
		return ret;
	}
	if (status != ENV_NOT_RUNNABLE && status != ENV_RUNNABLE) {
		return -E_INVAL;
	}
	e->env_status = status;
	return 0;
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	struct Env *e;
	int ret;

	ret = envid2env(envid, &e, 1);
	if (ret < 0)
		return ret;
	assert(tf);
	tf->tf_eflags |= FL_IOPL_3;
	tf->tf_cs |= 3;
	memcpy((void *)&e->env_tf, tf, sizeof(struct Trapframe));
	return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	struct Env *e;
	int ret = envid2env(envid, &e, 1);
	if (ret < 0)
		return ret;
	e->env_pgfault_upcall = func;
	return 0;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
	// create mask by flipping bits, so bits not allowed == 1
	int mask = ~(PTE_U | PTE_P | PTE_AVAIL | PTE_W);
	if (!(perm & (PTE_U | PTE_P)) || (mask & perm)) {
		return -E_INVAL;
	}

	if ((uint32_t)va >= UTOP || (uint32_t)va % PGSIZE != 0) {
		return -E_INVAL;
	}

	// page_alloc(ALLOC_ZERO) is crucial for spawn() since it uses
	// a sys_page_alloc() for allocating blank pages that are reserved
	// for bss section; otherwise uninited data would not be memsetted to 0
	struct PageInfo *p = page_alloc(1);
	if (!p) {
		return -E_NO_MEM;
	}

	struct Env *e;
	int ret = envid2env(envid, &e, 1);
	if (ret < 0) {
		return ret;
	}

	ret = page_insert(e->env_pgdir, p, va, perm);
	if (ret < 0) {
		page_free(p);
		return ret;
	}

	return 0;
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
//
//	TODO: after finishing lab6 refactor this function (and any other
//	      funcs that are defined here;
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
	// page_lookup returns page that is mapped at particular va addr
	// page_insert does the actual page mapping
	uint32_t src = (uint32_t)srcva;
	uint32_t dst = (uint32_t)dstva;
	int mask = ~(PTE_U | PTE_P | PTE_AVAIL | PTE_W);
	int ret = -1;
	bool write_perm = perm & PTE_W;
	struct Env *srcenv, *dstenv;
	pte_t *pte;
	struct PageInfo *p;

	if (!(perm & (PTE_U | PTE_P)) || (mask & perm)) {
		return -E_INVAL;
	}

	if (src % PGSIZE != 0 || dst % PGSIZE != 0 ||
	    (uint32_t)src >= UTOP || (uint32_t)dst >= UTOP) {
		return -E_INVAL;
	}

	ret = envid2env(srcenvid, &srcenv, 0);
	if (ret < 0) {
		return ret;
	}
	ret = envid2env(dstenvid, &dstenv, 0);
	if (ret < 0) {
		return ret;
	}

	// get source page that will be mapped onto dstenv
	p = page_lookup(srcenv->env_pgdir, srcva, &pte);
	if (!p) {
		return -E_INVAL;
	}

	 if (!(*pte & PTE_W) && write_perm) {
		 return -E_INVAL;
	 }

	ret = page_insert(dstenv->env_pgdir, p, dstva, perm);
	if (ret < 0) {
		return ret;
	}
	return 0;
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
	int ret;
	struct Env *e;
	ret = envid2env(envid, &e, 1);
	if (ret < 0) {
		return ret;
	}
	if ((uint32_t)va >= UTOP || (uint32_t)va % PGSIZE != 0) {
		return -E_INVAL;
	}
	page_remove(e->env_pgdir, va);
	return 0;
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
	struct Env *receiver;
	int ret;
	pte_t *src_page;
	bool transfer_page = false;

	ret = envid2env(envid, &receiver, 0);
	if (ret != 0)
		return ret;  // bad_env

	if ((uintptr_t)srcva < UTOP) {
		if (((uintptr_t)srcva % PGSIZE != 0) ||
		     (!(perm & (PTE_U | PTE_P)))) {
			return -E_INVAL;
		}
		if (page_lookup(curenv->env_pgdir, srcva, &src_page) == NULL)
			return -E_INVAL;
		if ((perm & PTE_W) && !(*src_page & PTE_W))
			return -E_INVAL;
		transfer_page = true;
	}
	if (!receiver->env_ipc_recving)
		return -E_IPC_NOT_RECV;

	receiver->env_ipc_recving = 0;
	receiver->env_ipc_value = value;
	receiver->env_ipc_from = curenv->env_id;
	if (receiver->env_ipc_dstva && transfer_page) {
		receiver->env_ipc_perm = perm;
		ret = sys_page_map(curenv->env_id, srcva, envid,
				receiver->env_ipc_dstva, perm);
		if (ret)
			return ret;
	} else
		receiver->env_ipc_perm = 0;
	receiver->env_status = ENV_RUNNABLE;
	receiver->env_tf.tf_regs.reg_eax = 0;
	return 0;
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	bool recv_pg = false;

	if ((uintptr_t)dstva < UTOP) {
		if ((uintptr_t)dstva % PGSIZE != 0)
			return -E_INVAL;
		else {
			recv_pg = true;
			page_remove(curenv->env_pgdir, dstva);
		}
	}

	curenv->env_ipc_dstva = recv_pg ? dstva : NULL;
	curenv->env_ipc_recving = 1;
	curenv->env_status = ENV_NOT_RUNNABLE;
	sys_yield();  // giving up CPU

	return 0;
}

static int
sys_tx_data(const char *data, uint8_t nbytes)
{
	bool finished = false;
	int ret;

	if ((uintptr_t)data >= UTOP)
		return -E_INVAL;

	while (!finished) {
		ret = tx_pkt(data, nbytes);
		switch (ret) {
		case 0:
			finished = true;
			break;
		case -E_INVAL:
			panic("sys_tx_data: nbytes >= 1518 (eth pkt size)");
			break;
		case -E_E1000_NOT_TX:
			continue;
		}
	}
	return 0;
}

static int
sys_rx_data(void *addr)
{
	curenv->env_net_dstva = addr;
	// if there was something to receive, then we are good to
	// go with returning the env_net_value, which holds the number
	// of bytes copied onto VA addr;
	if (rx_pkt(curenv) != -E_E1000_NOT_RX)
		return curenv->env_net_value;
	// otherwise, mark environment as the one that waits for packet
	// receival and give up the CPU;
	curenv->env_status = ENV_NOT_RUNNABLE;
	curenv->env_net_recving = 1;
	sys_yield();
	return 0;
}

// Return the current time.
static int
sys_time_msec(void)
{
	// LAB 6: Your code here.
	return time_msec();
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.
	int32_t ret = 0;
	if (syscallno >= NSYSCALLS)
		return -E_INVAL;

	switch (syscallno) {
	case SYS_cputs:
		sys_cputs((const char *)a1, a2);
		ret = a2;
		break;
	case SYS_cgetc:
		ret = sys_cgetc();
		break;
	case SYS_getenvid:
		ret = sys_getenvid();
		break;
	case SYS_env_destroy:
		ret = sys_env_destroy(a1);
		break;
	case SYS_yield:
		sys_yield();
		ret = 0;
		break;
	case SYS_exofork:
		ret = sys_exofork();
		break;
	case SYS_env_set_status:
		ret = sys_env_set_status(a1, a2);
		break;
	case SYS_page_alloc:
		ret = sys_page_alloc(a1, (void *)a2, a3);
		break;
	case SYS_page_map:
		ret = sys_page_map(a1, (void *)a2, a3, (void *)a4, a5);
		break;
	case SYS_page_unmap:
		ret = sys_page_unmap(a1, (void *)a2);
		break;
	case SYS_env_set_pgfault_upcall:
		ret = sys_env_set_pgfault_upcall(a1, (void *)a2);
		break;
	case SYS_ipc_try_send:
		ret = sys_ipc_try_send(a1, a2, (void *)a3, a4);
		break;
	case SYS_ipc_recv:
		ret = sys_ipc_recv((void *)a1);
		break;
	case SYS_env_set_trapframe:
		ret = sys_env_set_trapframe((envid_t)a1, (struct Trapframe *)a2);
		break;
	case SYS_time_msec:
		ret = sys_time_msec();
		break;
	case SYS_tx_data:
		ret = sys_tx_data((const char *)a1, a2);
		break;
	case SYS_rx_data:
		ret = sys_rx_data((void *)a1);
		break;
	default:
		return -E_INVAL;
	}
	return ret;
}
