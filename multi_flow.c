#include <linux/init.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <asm/system.h> /* cli(), *_flags */
#include <asm/uaccess.h> /* copy_from/to_user */
#include <linux/wait.h>
#include <linux/workqueue.h>

#define MINORS 128
#define DEVICE_NAME "my_device"


static int major;

typedef struct device{
    struct mutex low_mutex;
    struct mutex high_mutex;
    char * low_stream;
    char * hig_stream;
    wait_queue_head_t low_queue;
    wait_queue_head_t high_queue;
    workqueue_struct * workqueue;

} device;

device devices[MINORS];

MODULE_LICENSE("Dual BSD/GPL");
/* Declaration of multi_flow_device.c functions */
static int device_open(struct inode *inode, struct file *filp);
static int device_release(struct inode *inode, struct file *filp);
static ssize_t device_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
static ssize_t device_write(struct file *filp, char *buf, size_t count, loff_t *f_pos);
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
        char name_device[10] = DEVICE_NAME;
        devices[i].low_stream = NULL;
        devices[i].hig_stream = NULL;

        mutex_init(&devices[i].low_mutex);
        mutex_init(&devices[i].high_mutex);

        init_waitqueue_head(&devices[i].low_queue);
        init_waitqueue_head(&devices[i].high_queue);
        memset(name_device,0,15);
        sprintf( name_device, "%s%d", "mfdev_wq_", i );
        devices[i].workqueue = alloc_workqueue(name_device, WQ_HIGHPRI | WQ_UNBOUND , 0);

    }
    major = __register_chrdev(0, 0, 128,DEVICE_NAME,&file_operations);

    if(major < 0){

        printk("Registration of device Failed\n");
        return Major;

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