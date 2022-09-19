
/*  
 *  baseline char device driver with limitation on minor numbers - configurable in terms of concurrency 
 */

#define EXPORT_SYMTAB
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/pid.h>		/* For pid types */
#include <linux/tty.h>		/* For the tty declarations */
#include <linux/version.h>	/* For LINUX_VERSION_CODE */
#include "structs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gabriele Tummolo");

#define MINORS 128

int devices_state[MINORS];  //initially : 0 (ALL ENABLED)
int bytes_high[MINORS];
int bytes_low[MINORS];
int thread_waiting_high[MINORS];
int thread_waiting_low[MINORS]; 

module_param_array(devices_state,int,NULL,0644);
MODULE_PARM_DESC(devices_state,"Questo parametro da informazioni sullo stato dei device file! Se il valore = 0 è abilitato - Se il valore = 1 è disabilitato");

module_param_array(bytes_high,int,NULL,0644);
MODULE_PARM_DESC(bytes_high,"Questo parametro indica il numero di byte presenti sullo stream a alta priorità");

module_param_array(bytes_low,int,NULL,0644);
MODULE_PARM_DESC(bytes_low,"Questo parametro indica il numero di byte presenti sullo stream a bassa priorità");

module_param_array(thread_waiting_high,int,NULL,0644);
MODULE_PARM_DESC(thread_waiting_high,"Questo parametro il # di thread in attesa sulla coda ad alta priorità");

module_param_array(thread_waiting_low,int,NULL,0644);
MODULE_PARM_DESC(thread_waiting_low,"Questo parametro il # di thread in attesa sulla coda ad bassa priorità");


#define MODNAME "CHAR DEV"



static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

#define DEVICE_NAME "my-new-dev"  /* Device file name in /dev/ - not mandatory  */


static int Major;            /* Major number assigned to broadcast device driver */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session)	MAJOR(session->f_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_inode->i_rdev)
#else
#define get_major(session)	MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_dentry->d_inode->i_rdev)
#endif




device_info objects[MINORS];

#define OBJECT_MAX_SIZE  (4096) //just one page


static int dev_open(struct inode *inode, struct file *file) {

   int minor;
   session_info *session;
   minor = get_minor(file);

   if(minor >= MINORS){
	   return -ENODEV;
   }
   session = kzalloc(sizeof(session), GFP_ATOMIC);
   session->priority = 0;
   session->type_op = 1;
   session->timeout = 0.0;
   file->private_data = session;

   printk("%s: device file successfully opened for object with minor %d\n",MODNAME,minor);
   //device opened by a default nop
   return 0;


}


static int dev_release(struct inode *inode, struct file *file) {
   int minor;
   minor = get_minor(file);

   kfree(file->private_data);

   printk("%s: device file closed\n",MODNAME);

   return 0;

}



static ssize_t dev_write(struct file *filp, const char *buff, size_t len, loff_t *off) {
   printk("Non ancora implementato\n");
   return 0;

}

static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off) {
   int minor = get_minor(filp);
   int ret;
   device_info *the_object;
   the_object = objects + minor;
   //printk("%s: somebody called a write on dev with [major,minor] number [%d,%d]\n",MODNAME,get_major(filp),get_minor(filp));

   session_info *session = filp->private_data;
   tmp_buffer  = kzalloc(sizeof(char)*len,GPL_ATOMIC);
   memset(tmp_buff,0,len); //Pulizia buffer temporaneo

   if(session->priority == 0){


   }else{

   }
   ret = copy_to_user(buff,tmp_buff,len);
   kfree(tmp_buffer);
   return len-ret;

}

static long dev_ioctl(struct file *filp, unsigned int command, unsigned long param) {

  printk("non ancora implementato\n");
  return 0;

}

static struct file_operations fops = {
  .owner = THIS_MODULE,//do not forget this
  .write = dev_write,
  .read = dev_read,
  .open =  dev_open,
  .release = dev_release,
  .unlocked_ioctl = dev_ioctl
};



int init_module(void) {

	int i;

	//initialize the drive internal state
	for(i=0;i<MINORS;i++){

      //initialize wait queue
      init_waitqueue_head(&objects[i].queue[0]);
      init_waitqueue_head(&objects[i].queue[1]);

		objects[i].valid_bytes[0] = 0;
      objects[i].valid_bytes[1] = 0;
		objects[i].stream_content[0] = (char*)__get_free_page(GFP_KERNEL);
      objects[i].stream_content[1] = (char*)__get_free_page(GFP_KERNEL);

		if(objects[i].stream_content == NULL || objects[i].stream_content == NULL) 
            goto revert_allocation;
      
      mutex_init(&(objects[i].operation_synchronizer[0]));
      mutex_init(&(objects[i].operation_synchronizer[0]));
	}

	Major = __register_chrdev(0, 0, 256, DEVICE_NAME, &fops);
	//actually allowed minors are directly controlled within this driver

	if (Major < 0) {
	  printk("%s: registering device failed\n",MODNAME);
	  return Major;
	}

	printk(KERN_INFO "%s: new device registered, it is assigned major number %d\n",MODNAME, Major);

	return 0;

revert_allocation:
	for(;i>=0;i--){
		free_page((unsigned long)objects[i].stream_content[0]);
      free_page((unsigned long)objects[i].stream_content[1]);
	}
	return -ENOMEM;
}

void cleanup_module(void) {

	int i;
	for(i=0;i<MINORS;i++){
		free_page((unsigned long)objects[i].stream_content[0]);
      free_page((unsigned long)objects[i].stream_content[1]);
	}

	unregister_chrdev(Major, DEVICE_NAME);

	printk(KERN_INFO "%s: new device unregistered, it was assigned major number %d\n",MODNAME, Major);

	return;

}
