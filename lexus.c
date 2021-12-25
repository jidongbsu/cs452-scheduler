#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/uaccess.h> /* for copy_to_user and copy_from_user */
#include <linux/random.h> /* for random number generator */
#include <linux/miscdevice.h> /* for misc devices */
#include <linux/pid.h> /* for pid_task */
#include "lexus.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jidong Xiao"); /* Note: change this line to your name! */
MODULE_DESCRIPTION("CS452 Lexus");

//#define DEBUG 1 /* Note: uncomment this line so you can see debug messages in /var/log/messages, or when you run dmesg */

unsigned long nTickets = 0;
static struct timer_list dispatch_timer;

/* each lexus_task_struct represents one process in the lottery scheduling system */
struct lexus_task_struct {
        struct list_head list; /* Kernel's list structure */
        struct task_struct* task;
        unsigned long pid;
        unsigned long tickets;
        task_state state;
};

/* use this global variable to track all registered tasks, by adding into its list */
static struct lexus_task_struct lexus_task_struct;

/* the currently running task */
static struct task_struct *lexus_current;

/* used for creating and traversing the linked list */
static struct list_head *head, *next;
static struct lexus_task_struct *tmp;

/* spinlock to protect the linked list */
static spinlock_t list_lock;

/* cache for lexus_task_struct slab allocation */
static struct kmem_cache *task_cache;

/* dispatch kernel thread */
static struct task_struct *dispatch_kthread;

/* given a pid, returns its task_struct */
struct task_struct* find_task_by_pid(unsigned int pid)
{
   struct task_struct* task;
   rcu_read_lock();
   task=pid_task(find_vpid(pid), PIDTYPE_PID);
   rcu_read_unlock();

   return task;
}

/* given a pid, returns its lexus_task_struct */
struct lexus_task_struct* find_lexus_task_by_pid(unsigned int pid) {
   list_for_each(head, &lexus_task_struct.list) {
      tmp = list_entry(head, struct lexus_task_struct, list);
      if(tmp->pid == pid) {
         return tmp;
      }
   }
   return NULL;
}

/* free all the lexus_task_struct instances: delete its list, and free its memory allocated via kmem_cache_alloc() */
void free_lexus_list(void) {
   list_for_each_safe(head, next, &lexus_task_struct.list) {
      tmp = list_entry(head, struct lexus_task_struct, list);
      list_del(head);
      kmem_cache_free(task_cache, tmp);
   }
}

/* register a process into the lottery scheduling system */
void lexus_register(struct lottery_struct lottery){
}

/* unregister a process from the lottery scheduling system */
void lexus_unregister(struct lottery_struct lottery){
}

void dispatch_timer_callback(unsigned long data)
{
}

/* executes a context switch */
int lexus_schedule(void *data)
{
	return 0;
}

static long lexus_dev_ioctl(struct file *filp,
                          unsigned int ioctl, unsigned long arg)
{
	return 0;
}

static const struct file_operations lexus_chardev_ops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = lexus_dev_ioctl,
	.compat_ioctl   = lexus_dev_ioctl,
	.llseek         = noop_llseek,
};

/* once we call misc_reigster(lexus_dev), a file called /dev/lexus will be created.
 * any ioctl commands sent to this device will be handled by lexus_dev.fops.unlocked_ioctl, 
 * which is lexus_dev_ioctl(). */
static struct miscdevice lexus_dev = {
	.minor = LEXUS_MINOR,
	.name = "lexus",
	.fops = &lexus_chardev_ops,
	.mode = 0666,
};

/* called when module is loaded */
int __init lexus_init(void)
{
	int r;

	#ifdef DEBUG
	printk("<1> lexus: loading.\n");
	#endif

	r = misc_register(&lexus_dev);
	if (r) {
		printk(KERN_ERR "lexus: misc device register failed\n");
		return r;
	}

	/* the lexus version of "current" - in Linux kernel, the global variable "current" points to the currently running process - its task_struct */
	lexus_current = NULL;

	/* initialize the list_head to be empty */
	INIT_LIST_HEAD(&lexus_task_struct.list);
	/* allocate a memory pool from the slab memory management system to accommodate all lexus_task_struct instances */
	task_cache = kmem_cache_create("lexus_task_cache",
									sizeof(struct lexus_task_struct),
									0, SLAB_HWCACHE_ALIGN,
									NULL);
	/* a kernel thread named lexus_dispatch will be running at the background, which calls lexus_schedule().
 	 * We don't need to pass any parameter lexus_schedule(), thus here the 2nd parameter is NULL. */
	dispatch_kthread = kthread_create(lexus_schedule, NULL, "lexus_dispatch");

	/* initialize the spin lock */
	spin_lock_init(&list_lock);

	/* setup your timer to call dispatch_timer_callback */
	setup_timer(&dispatch_timer, dispatch_timer_callback, 0);
	/* setup timer interval to 200 msecs */
	mod_timer(&dispatch_timer, jiffies + msecs_to_jiffies(200));

	#ifdef DEBUG
	printk("<1> lexus: loaded.\n");
	#endif

	return 0;
}

/* called when module is unloaded */
void __exit lexus_exit(void)
{
	#ifdef DEBUG
	printk("<1> lexus: unloading.\n");
	#endif

	/* now it's time to exit the dispatch kthread, we wake it up so that it runs and checks the stop flag at the beginning of the while loop */
	wake_up_process(dispatch_kthread);
	kthread_stop(dispatch_kthread);

	/* remove kernel timer when unloading module */
	del_timer(&dispatch_timer);

	/* free the memory allocated for each lexus_task_struct */
	free_lexus_list();

	/* free the memory pool */
	kmem_cache_destroy(task_cache);

	misc_deregister(&lexus_dev);
	#ifdef DEBUG
	printk("<1> lexus: unloaded.\n");
	#endif
}

// register init and exit funtions
module_init(lexus_init);
module_exit(lexus_exit);

/* vim: set ts=4: */