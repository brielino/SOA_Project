
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

bool take_lock(session_info *session,struct mutex * mutex, wait_queue_head_t * wait_queue){
   if(session->type_op == 0){  //non bloccante
      if(mutex_trylock(mutex) == 1) // lock preso
      {  
         return true;  
      } 
      else //lock non preso
      {
         printk("[Non-Blocking op]=> PID: %d; NAME: %s - CAN'T DO THE OPERATION\n", current->pid, current->comm);
         return false;// return to the caller 
      }
   }
   else
   {
      if(wait_event_timeout(*wait_queue, (mutex_trylock(mutex) == 1), (session->timeout * HZ)/1000) == 0){
         printk("[Blocking op]=> PID: %d; NAME: %s - TIMEOUT EXPIRED\n", current ->pid, current->comm); //TIMEOUT EXPIRED
         return false;
      }else{
         return true;
      }
   } 
}


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
   char * tmp_buffer;
   device_info *the_object;
   session_info *session = filp->private_data;
   the_object = objects + minor;
   //printk("%s: somebody called a write on dev with [major,minor] number [%d,%d]\n",MODNAME,get_major(filp),get_minor(filp));

   tmp_buffer  = kzalloc(sizeof(char)*len,GFP_ATOMIC);
   memset(tmp_buffer,0,len); //Pulizia buffer temporaneo

   if(take_lock(session,&(the_object->mutex_op[session->priority]),&(the_object->wait_queue[session->priority]))){

      if(len > the_object->valid_bytes[session->priority])
      { //Verifica se il numero di byte da leggere sono maggiori dei byte disponibilià 
              len = the_object->valid_bytes[session->priority];
      }
      //copy first len bytes to tmp buff
      memmove(tmp_buffer, the_object->stream_content[session->priority],len);
      //clear after reading 
      memmove(the_object->stream_content[session->priority], the_object->stream_content[session->priority] + len,the_object->valid_bytes[session->priority]-len); //shift
      memset(the_object->stream_content[session->priority]+ the_object->valid_bytes[session->priority] - len,0,len); //clear
      the_object->stream_content[session->priority] = krealloc(the_object->stream_content[session->priority],the_object->valid_bytes[session->priority] - len,GFP_ATOMIC);
      //resettig parameters 
      the_object->valid_bytes[session->priority] -= len;
      mutex_unlock(&(the_object->mutex_op[session->priority])); 
      wake_up(&the_object->wait_queue[session->priority]);
   }else{
      return 0;
   }   

   
   ret = copy_to_user(buff,tmp_buffer,len);
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
      init_waitqueue_head(&objects[i].wait_queue[0]);
      init_waitqueue_head(&objects[i].wait_queue[1]);

		objects[i].valid_bytes[0] = 0;
      objects[i].valid_bytes[1] = 0;
		objects[i].stream_content[0] = NULL;
      objects[i].stream_content[1] = NULL;
      
      mutex_init(&(objects[i].mutex_op[0]));
      mutex_init(&(objects[i].mutex_op[0]));
	}

	Major = __register_chrdev(0, 0, 256, DEVICE_NAME, &fops);
	//actually allowed minors are directly controlled within this driver

	if (Major < 0) {
	  printk("%s: registering device failed\n",MODNAME);
	  return Major;
	}

	printk(KERN_INFO "%s: new device registered, it is assigned major number %d\n",MODNAME, Major);

	return 0;
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
