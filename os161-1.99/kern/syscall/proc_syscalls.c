#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include <kern/limits.h>
#include "opt-A2.h"
#include <array.h>
#include <synch.h>
#include <kern/fcntl.h>
#include <vfs.h>
#include <limits.h>


  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;
    
/* ========================= opt_A2 ========================= */
    
#if OPT_A2
    
    // lock acquire
    lock_acquire(lockers->running_lk);
    
    unsigned r = array_num(running);
    unsigned n = 0;
    
    while (n < r) {
        // get process
        struct process * pcs = (struct process *) array_get(running, n);

        // if parent dies too, put it in the reusable array
        if (pcs->child_pid == p->p_pid && !pcs->parent) {
            // kill child
            pcs->child = false;
            // acquire reusable_sem
            P(lockers->reusable_sem);
            
            // add it to the list
            pid_t * pid_ptr2 = kmalloc(sizeof(pid_t));
            * pid_ptr2 = pcs->child_pid;
            array_add(reusable, (void *) pid_ptr2, NULL);
            
            // release reusable_sem
            V(lockers->reusable_sem);
            
            // free & remove from list
            kfree(pcs);
            array_remove(running, n);
            n -= 1;
            r -= 1;
        // if parent is still running
        } else if(pcs->child_pid == p->p_pid && pcs->parent){
            // kill child
            pcs->child = false;
            // give exit code to parent
            pcs->exitcode = _MKWAIT_EXIT(exitcode);
            // wake all
            cv_broadcast(lockers->waiting_cv, lockers->running_lk);
        // check relationship with current process
        } else if (pcs->parent_pid == p->p_pid) {
            // kill parent
            pcs->parent = false;
            // if child dies too, put it in the reusable array
            if (!pcs->child) {
                // acquire reusable_sem
                P(lockers->reusable_sem);
                
                // add it to the list
                pid_t * pid_ptr = kmalloc(sizeof(pid_t));
                * pid_ptr = pcs->child_pid;
                array_add(reusable, (void *) pid_ptr, NULL);
                
                // release reusable_sem
                V(lockers->reusable_sem);
                
                // free & remove from list
                kfree(pcs);
                array_remove(running, n);
                n -= 1;
                r -= 1;
            }
        } 
        n += 1;
    }
    lock_release(lockers->running_lk);
    
    
#endif
    
/* ========================= opt_A2 ========================= */

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  //*retval = 1;
    
/* ========================= opt_A2 ========================= */
    
#if OPT_A2

    * retval = curproc->p_pid;
    
#endif
    
/* ========================= opt_A2 ========================= */
    
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus = 0;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

/* ========================= opt_A2 ========================= */
    
#if OPT_A2
    
    struct proc * cp = curproc;
    
    // acquire running_lk
    lock_acquire(lockers->running_lk);
    
    unsigned r = array_num(running);
    // initially not found
    bool f = false;
    
    for (unsigned i = 0; i < r; i++) {
        struct process * pcs = (struct process *) array_get(running, i);

        // if pid is child of proc and if proc and current process are siblings
        if (pcs->child_pid == pid && pcs->parent_pid == cp->p_pid) {
        
          // while child is not dead
          while (pcs->child) {
              cv_wait(lockers->waiting_cv, lockers->running_lk);
          }
          // give exit code
          if (!pcs->child) {
              exitstatus = pcs->exitcode;
          }
          // found now
          f = true;
          break;
        }

        // not siblings
        else if(pcs->child_pid == pid && pcs->parent_pid != cp->p_pid){
          // release running_lk
          lock_release(lockers->running_lk);
          return(ECHILD);
        }
    }
    
    // release running_lk;
    lock_release(lockers->running_lk);
    
    // not found
    if (!f) {
        return (ESRCH);
    }
    
#endif
    
/* ========================= opt_A2 ========================= */
    
    
  if (options != 0) {
    return(EINVAL);
  }
    
  if (status == NULL) {
    return (EFAULT);
  }
    

  /* for now, just pretend the exitstatus is 0 */
  //exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

/* ========================= opt_A2 ========================= */

#if OPT_A2

int sys_fork(struct trapframe * tf, pid_t * retval) {
    struct proc * cp = curproc;
    struct proc * new_p = proc_create_runprogram(curproc->p_name);
    struct addrspace * new_a;
    
    as_copy(cp->p_addrspace, &new_a);
    
    // copy trapframe
    new_p->p_addrspace = new_a;
    struct trapframe *new_tf = kmalloc(sizeof(* new_tf));
    
    memcpy(new_tf, tf, sizeof(* new_tf));
    
    // create process and add it to running list
    lock_acquire(lockers->running_lk);
    struct process * pcs = process_init(cp->p_pid, new_p->p_pid);
    array_add(running, pcs, NULL);
    lock_release(lockers->running_lk);
    
    thread_fork("fork_thread", new_p, enter_forked_process, (void*)new_tf, (int) new_p->p_pid);

    * retval = new_p->p_pid;
    return 0;
}

#endif

/* ========================= opt_A2 ========================= */

/* ========================= opt_A2b ========================= */

#if OPT_A2

int sys_execv(char * program, char * * args){
  int error;
  
  // Count arguments
  char * * temargs;
  error = copyin((const_userptr_t) args, &temargs, sizeof(char *));

  int num_of_args = 0;
  for (int i = 0; args[i] != NULL; i++) {
    num_of_args += 1;
  }
  num_of_args += 1;

  // Copy args to kernel
  char * * kernal_args = kmalloc(sizeof(char *) * num_of_args);
  for (int i = 0; i < num_of_args - 1; i++) {
    kernal_args[i] = kmalloc(sizeof(char) * (strlen(args[i]) + 1));
    error = copyinstr((const_userptr_t) args[i], 
                       kernal_args[i], 
                       sizeof(char) * (strlen(args[i]) + 1), 
                       NULL);
  }

  kernal_args[num_of_args - 1] = NULL;

  // Copy program to kernel
  char * kernel_program = kmalloc(sizeof(char) * (strlen(program) + 1));
  error = copyinstr((const_userptr_t)program, 
                     kernel_program, 
                     sizeof(char) * (strlen(program) + 1), 
                     NULL);

  struct addrspace * addr_space;
  struct vnode * v;
  vaddr_t entrypnt, stackptr;
  struct addrspace * old_addr;

  // Open the file. 
  error = vfs_open(kernel_program, O_RDONLY, 0, &v);

  // Create a new address space.
  addr_space = as_create();

  // Switch to it and activate it.
  old_addr = curproc_setas(addr_space);
  as_activate();

  // Load the executable.
  error = load_elf(v, &entrypnt);

  // Done with the file now. 
  vfs_close(v);

  // Define the user stack in the address space 
  error = as_define_stack(addr_space, &stackptr);

  userptr_t userargs[num_of_args];
  for (int i = 0; i < num_of_args - 1; i++)
  {
    int l = ROUNDUP(strlen(kernal_args[i]) + 1, 8);
    stackptr -= l;
    userargs[i] = (userptr_t)stackptr;
    error = copyoutstr(kernal_args[i], 
                       (userptr_t)stackptr, 
                       sizeof(char) * l, 
                       NULL);
  }

  userargs[num_of_args - 1] = NULL;

  stackptr -= ROUNDUP(num_of_args * 4, 8);
  error = copyout(userargs,  
                  (userptr_t)stackptr, 
                  (size_t) ROUNDUP (num_of_args * 4, 
                  8));

  as_destroy(old_addr);

  // Warp to user mode. 
  enter_new_process(num_of_args - 1, 
                    (userptr_t) stackptr, 
                    stackptr, 
                    entrypnt);

  return 0;
}

#endif

/* ========================= opt_A2b ========================= */

