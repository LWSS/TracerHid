#include <linux/ftrace.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#include "kallsyms.h"

MODULE_DESCRIPTION("");
MODULE_AUTHOR("");
MODULE_LICENSE("GPL");
MODULE_INFO(livepatch, "Y");

#define log_print(fmt, ...) printk( (KBUILD_MODNAME ": "fmt), ##__VA_ARGS__ );

#ifndef CONFIG_X86_64
    #error Only x86_64 architecture is supported!
#endif

/*
 * There are two ways of preventing vicious recursive loops when hooking:
 * - detect recusion using function return address (USE_FENTRY_OFFSET = 0)
 * - avoid recusion by jumping over the ftrace call (USE_FENTRY_OFFSET = 1)
 */
#define USE_FENTRY_OFFSET 0


/*
 * Tail call optimization can interfere with recursion detection based on
 * return address on the stack. Disable it to avoid machine hangups.
 */
#if !USE_FENTRY_OFFSET
    #pragma GCC optimize("-fno-optimize-sibling-calls")
#endif

struct ftrace_hook {
    const char *name;
    void *function;
    void *original;

    unsigned long address;
    struct ftrace_ops ops;
};

static struct ftrace_hook proc_pid_status_hook;

static void init_hook( struct ftrace_hook *hook )
{
#if USE_FENTRY_OFFSET
    *((unsigned long*) hook->original) = hook->address + MCOUNT_INSN_SIZE;
#else
    *((unsigned long*) hook->original) = hook->address;
#endif
}

static void notrace ftrace_thunk(unsigned long ip, unsigned long parent_ip,
                                    struct ftrace_ops *ops, struct pt_regs *regs)
{
    struct ftrace_hook *hook = container_of(ops, struct ftrace_hook, ops);

#if USE_FENTRY_OFFSET
    regs->ip = (unsigned long) hook->function;
#else
    if (!within_module(parent_ip, THIS_MODULE))
        regs->ip = (unsigned long) hook->function;
#endif
}

static asmlinkage int (* orig_proc_pid_status)(struct seq_file *m, struct pid_namespace *ns,
                                               struct pid *pid, struct task_struct *task);

static asmlinkage int hooked_proc_pid_status(struct seq_file *m, struct pid_namespace *ns,
                                             struct pid *pid, struct task_struct *task)
{
    if( !task ){
        return orig_proc_pid_status( m, ns, pid, task ); // might happen, idk
    }

    char *pathname, *p;
    bool more_info = false;
    int ret;
    unsigned int backup_ptrace;

    if( task->mm ){
        down_read(&task->mm->mmap_sem);
        if (task->mm->exe_file) {
            pathname = kmalloc(PATH_MAX, GFP_ATOMIC);
            if (pathname) {
                p = d_path(&task->mm->exe_file->f_path, pathname, PATH_MAX);
                log_print("Hiding TracerPid on Process(%s)\n", p);
                more_info = true;
                kfree(pathname);
            }
        }
        up_read(&task->mm->mmap_sem);
    }
    if( !more_info ){
        log_print("Hiding TracerPid on Process(%d)", task->pid);
    }
    backup_ptrace = task->ptrace;
    task->ptrace = 0;
    ret = orig_proc_pid_status( m, ns, pid, task );
    task->ptrace = backup_ptrace;
    return ret;
}

static int cart_startup(void)
{
    int ret = 0;

    if( init_kallsyms() ){
        log_print( "Error initing kallsyms hack.\n" );
        return -EAGAIN;
    }
    proc_pid_status_hook.name = "proc_pid_status";
    proc_pid_status_hook.address = kallsyms_lookup_name( "proc_pid_status" );

    if( !proc_pid_status_hook.address ){
        log_print( "Error iterating through modules!\n" );
        return -EAGAIN;
    }
    if( !proc_pid_status_hook.address ){
        log_print( "Error Finding the Address\n" );
        return -ENXIO;
    }

    proc_pid_status_hook.function = hooked_proc_pid_status;
    proc_pid_status_hook.original = &orig_proc_pid_status;
    init_hook( &proc_pid_status_hook );

    proc_pid_status_hook.ops.func = ftrace_thunk;
    proc_pid_status_hook.ops.flags = FTRACE_OPS_FL_SAVE_REGS
                                  | FTRACE_OPS_FL_RECURSION_SAFE
                                  | FTRACE_OPS_FL_IPMODIFY;

    ret = ftrace_set_filter_ip(&proc_pid_status_hook.ops, proc_pid_status_hook.address, 0, 0);
    if( ret ){
        log_print("ftrace_set_filter_ip() failed: %d\n", ret);
        return ret;
    }

    ret = register_ftrace_function(&proc_pid_status_hook.ops);
    if (ret) {
        log_print("register_ftrace_function() failed: %d\n", ret);
        ftrace_set_filter_ip(&proc_pid_status_hook.ops, proc_pid_status_hook.address, 1, 0);
        return ret;
    }

    log_print("TracerHid Loading complete.\n");
    return 0;
}

static void cart_shutdown(void)
{
    int ret;
    ret = unregister_ftrace_function(&proc_pid_status_hook.ops);
    if( ret )
        log_print("unregister_ftrace_function() failed: %d\n", ret);

    ret = ftrace_set_filter_ip(&proc_pid_status_hook.ops, proc_pid_status_hook.address, 1, 0);
    if( ret )
        log_print("ftrace_set_filter_ip() failed: %d\n", ret);


    log_print("TracerHid UnLoaded.\n");
}

module_init(cart_startup);
module_exit(cart_shutdown);