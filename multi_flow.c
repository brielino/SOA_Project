#include<linux/capability.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/list.h>
#include<linux/fs.h>
#include<linux/string.h>
#include <linux/kref.h>
#include <linux/seq_file.h>
#include<linux/init.h>
#include<linux/ioctl.h>
#include<linux/kdev_t.h>
#include<linux/module.h>
#include<linux/moduleparam.h>
#include<linux/sched.h>
#include<linux/semaphore.h>
#include<linux/errno.h>
#include<linux/types.h>
#include<linux/uaccess.h>
#include<linux/wait.h>
#include<linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>	
#include <linux/pid.h>		/* For pid types */
#include <linux/tty.h>		/* For the tty declarations */
#include <linux/version.h>	/* For LINUX_VERSION_CODE */
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/kern_levels.h>

#define MINORS 128
#define DEVICE_NAME "my_device"


static int major;

typedef struct _device{
    struct mutex low_mutex;
    struct mutex high_mutex;
    char * low_stream;
    char * hig_stream;
    wait_queue_head_t low_queue;
    wait_queue_head_t high_queue;
    struct workqueue_struct * workqueue;

} device;

device devices[MINORS];

MODULE_LICENSE("Dual BSD/GPL");
/* Declaration of multi_flow_device.c functions */
static int device_open(struct inode *inode, struct file *filp);
static int device_release(struct inode *inode, struct file *filp);
static ssize_t device_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
static ssize_t device_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
static void device_exit(void);
static int device_init(void);

/* Structure that declares the usual file */
/* access functions */
struct file_operations file_operations = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};


int init_function(void){

    int i;
    for(i=0;i<MINORS;i++){
        char name_device[15] = DEVICE_NAME;
        devices[i].low_stream = NULL;
        devices[i].hig_stream = NULL;

        mutex_init(&(devices[i].low_mutex));
        mutex_init(&(devices[i].high_mutex));

        init_waitqueue_head(&(devices[i].low_queue));
        init_waitqueue_head(&(devices[i].high_queue));
        memset(name_device,0,15);
        sprintf( name_device, "%s%d", "mfdev_wq_", i );
        devices[i].workqueue = alloc_workqueue(name_device, WQ_HIGHPRI | WQ_UNBOUND , 0);

    }
    major = __register_chrdev(0, 0, 128,DEVICE_NAME,&file_operations);

    if(major < 0){

        printk("Registration of device Failed\n");
        return major;

    }
    printk("The Device Driver was registred with the Major Number %d\n",major);
    return 0;
}

void exit_function(void)
{
    int i;
    for(i = 0; i< MINORS ;i++){
        flush_workqueue(devices[i].workqueue);
        destroy_workqueue(devices[i].workqueue);
        kfree(devices[i].low_stream);
        kfree(devices[i].hig_stream);
    }

    unregister_chrdev(major, DEVICE_NAME);
}
module_init(init_function);
module_exit(exit_function);