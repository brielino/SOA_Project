
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
#include <linux/workqueue.h>
#include "structs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gabriele Tummolo");


static int Major;            /* Major number assigned to broadcast device driver */
info_device objects[MINORS];
struct workqueue_struct * workqueue;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session)	MAJOR(session->f_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_inode->i_rdev)
#else
#define get_major(session)	MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_dentry->d_inode->i_rdev)
#endif


void deferred_work(struct work_struct *work);

module_param_array(stato_devices,int,NULL,0644);
MODULE_PARM_DESC(stato_devices,"Questo parametro da informazioni sullo stato dei device file! Se il valore = 0 è abilitato - Se il valore = 1 è disabilitato");

module_param_array(byte_validi_alta_priorita,int,NULL,0644);
MODULE_PARM_DESC(byte_validi_alta_priorita,"Questo parametro indica il numero di byte presenti sullo stream a alta priorità");

module_param_array(byte_validi_bassa_priorita,int,NULL,0644);
MODULE_PARM_DESC(byte_validi_bassa_priorita,"Questo parametro indica il numero di byte presenti sullo stream a bassa priorità");

module_param_array(thread_in_attesa_alta_priorita,int,NULL,0644);
MODULE_PARM_DESC(thread_in_attesa_alta_priorita,"Questo parametro il # di thread in attesa sulla coda ad alta priorità");

module_param_array(thread_in_attesa_bassa_priorita,int,NULL,0644);
MODULE_PARM_DESC(thread_in_attesa_bassa_priorita,"Questo parametro il # di thread in attesa sulla coda ad bassa priorità");




static int apertura_device(struct inode *, struct file *);
static int rilascio_device(struct inode *, struct file *);
static ssize_t lettura_device(struct file *, char *, size_t, loff_t *);
static ssize_t scrittura_device(struct file *, const char *, size_t, loff_t *);
static long operazione_ioctl(struct file *, unsigned int , unsigned long );

static struct file_operations fops = {
  .owner = THIS_MODULE,
  .write = scrittura_device,
  .read = lettura_device,
  .open =  apertura_device,
  .release = rilascio_device,
  .unlocked_ioctl = operazione_ioctl
};

void deferred_work(struct work_struct *work){
   int minor;
   int len;
   data_work *data;
   info_device *the_object;
   data = container_of(work,data_work,work);
   len = data->len;
   minor = data->minor;
   the_object = objects + minor;
   mutex_lock(&(the_object->mutex_op[1])); 
   the_object->streams[1] = krealloc(&the_object->streams[1],the_object->bytes_validi[1] + len,GFP_ATOMIC);
   memset(&the_object->streams[1]+ the_object->bytes_validi[1],0,len); //clear
   strncat(the_object->streams[1],data->buffer,len);
   the_object->bytes_validi[1] += len;
   mutex_unlock(&(the_object->mutex_op[1])); 

   return;
}

void chiama_deferred_work(char** temp_buff, int len, data_work *data,int minor){
   data->minor = minor;
   data->buffer =*temp_buff;
   data->len = len;
   INIT_WORK(&data->work,deferred_work);
   queue_work(workqueue, &data->work);
}

bool prendi_lock(info_sessione *sessione_c,struct mutex * mutex, wait_queue_head_t * coda_attesa){
   if(sessione_c->tipo_operaz == 0){  //non bloccante
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
      if(wait_event_timeout(*coda_attesa, (mutex_trylock(mutex) == 1), (sessione_c->timeout * HZ)/1000) == 0){
         printk("[Blocking op]=> PID: %d; NAME: %s - TIMEOUT EXPIRED\n", current ->pid, current->comm); //TIMEOUT EXPIRED
         return false;
      }else{
         return true;
      }
   } 
}

static ssize_t scrittura_device(struct file *filp, const char *buff, size_t len, loff_t *off) {
   int minor = get_minor(filp);
   int ret;
   char * buffer_temporaneo;
   int pr_c;
   int bytes_validi;
   data_work *data;
   info_device *the_object;
   info_sessione *sessione_c = filp->private_data;
   the_object = objects + minor;
   pr_c = sessione_c->priorita;
   bytes_validi = the_object->bytes_validi[pr_c];
   printk("%s: somebody called a read on dev with [major,minor] number [%d,%d]\n",MODNAME,get_major(filp),get_minor(filp));
   
   buffer_temporaneo  = kzalloc(sizeof(char)*len,GFP_ATOMIC);
   memset(buffer_temporaneo,0,len); //Pulizia buffer temporaneo
   ret = copy_from_user(buffer_temporaneo, buff, len);
   if(pr_c == 1) //Bassa priorità
   {
      chiama_deferred_work(&buffer_temporaneo,len,data,minor);
   }else if(prendi_lock(sessione_c,&(the_object->mutex_op[pr_c]),&(the_object->coda_attesa[pr_c]))){
      the_object->streams[pr_c] = krealloc(&the_object->streams[pr_c],the_object->bytes_validi[pr_c] + len,GFP_ATOMIC);
      memset(&the_object->streams[pr_c]+ the_object->bytes_validi[pr_c],0,len); //clear
      strncat(the_object->streams[pr_c],buffer_temporaneo,len);
      the_object->bytes_validi[pr_c] += len;
      mutex_unlock(&(the_object->mutex_op[pr_c])); 
      wake_up(&the_object->coda_attesa[pr_c]);
   }else{
      return 0;
   }


   return len - ret;

}



static int apertura_device(struct inode *inode, struct file *file) {

   int minor;
   info_sessione *sessione_c;
   minor = get_minor(file);

   if(minor >= MINORS){
	   return -ENODEV;
   }
   sessione_c = kzalloc(sizeof(sessione_c), GFP_ATOMIC);
   sessione_c->priorita = 0;
   sessione_c->tipo_operaz = 1;
   sessione_c->timeout = 0.0;
   file->private_data = sessione_c;

   printk(KERN_INFO "%s: Device file aperto con successo per l'oggetto con minor number %d\n",MODNAME,minor);
   //device opened by a default nop
   return 0;


}


static int rilascio_device(struct inode *inode, struct file *file) {
   int minor;
   minor = get_minor(file);

   kfree(file->private_data);

   printk(KERN_INFO "%s: Device file chiuso\n",MODNAME);

   return 0;

}




static ssize_t lettura_device(struct file *filp, char *buff, size_t len, loff_t *off) {
   int minor = get_minor(filp);
   int ret;
   char * buffer_temporaneo;
   int pr_c;
   int bytes_validi;
   info_device *the_object;
   info_sessione *session = filp->private_data;
   the_object = objects + minor;
   pr_c = session->priorita;
   bytes_validi = the_object->bytes_validi[pr_c];
   printk(KERN_INFO "%s: Lettura chiamata per il device con [major,minor] number [%d,%d]\n",MODNAME,get_major(filp),get_minor(filp));
   
   buffer_temporaneo  = kzalloc(sizeof(char)*len,GFP_ATOMIC);

   memset(buffer_temporaneo,0,len); //Pulizia buffer temporaneo

   if(prendi_lock(session,&(the_object->mutex_op[pr_c]),&(the_object->coda_attesa[pr_c]))){

      if(len > bytes_validi)
      { //Verifica se il numero di byte da leggere sono maggiori dei byte disponibilià 
              len = bytes_validi;
      }
      //Copio il contenuto dello stream nel buffer temporaneo
      memmove(&buffer_temporaneo, &the_object->streams[pr_c],len);
      //Le 3 operazioni successive mi permetto di eliminare dallo stream i bytes che sono stati letti
      memmove(&the_object->streams[pr_c], &the_object->streams[pr_c] + len,bytes_validi -len); //shift
      memset(&the_object->streams[pr_c]+ (bytes_validi - len),0,len);
      //Ri-dimensionamento dello stream dopo la lettura
      the_object->streams[pr_c] = krealloc(&the_object->streams[pr_c],bytes_validi - len,GFP_ATOMIC);
      //Aggiornamento dei bytes validi per lo stream considerato
      the_object->bytes_validi[pr_c] -= len;
      mutex_unlock(&(the_object->mutex_op[pr_c])); 
      wake_up(&the_object->coda_attesa[pr_c]);
   }else{
      return 0;
   }   

   
   ret = copy_to_user(buff,buffer_temporaneo,len);
   kfree(buffer_temporaneo);
   return len-ret;

}

static long operazione_ioctl(struct file *filp, unsigned int command, unsigned long param) {

  printk("non ancora implementato\n");
  return 0;

}



int inizializzazione_modulo(void) {

	int i;

	//initialize the drive internal state
   workqueue = create_workqueue("workqueue");
	for(i=0;i<MINORS;i++){

      //initialize wait queue
      init_waitqueue_head(&objects[i].coda_attesa[0]);
      init_waitqueue_head(&objects[i].coda_attesa[1]);

		objects[i].bytes_validi[0] = 0;
      objects[i].bytes_validi[1] = 0;
		objects[i].streams[0] = NULL;
      objects[i].streams[1] = NULL;
      
      mutex_init(&(objects[i].mutex_op[0]));
      mutex_init(&(objects[i].mutex_op[0]));
	}

	Major = __register_chrdev(0, 0, 256, DEVICE_NAME, &fops);
	//actually allowed minors are directly controlled within this driver

	if (Major < 0) {
	  printk(KERN_ERR "%s: Registrazione device fallita\n",MODNAME);
	  return Major;
	}

	printk(KERN_INFO "%s: Device registrato con successo, il major number e' %d\n",MODNAME, Major);

	return 0;
}

void rilascio_modulo(void) {

	int i;
	for(i=0;i<MINORS;i++){
		free_page((unsigned long)objects[i].streams[0]);
      free_page((unsigned long)objects[i].streams[1]);
	}

	unregister_chrdev(Major, DEVICE_NAME);

	printk(KERN_INFO "%s: Cancellazione Device effettuata con successo! Il Major number era %d\n",MODNAME, Major);

	return;

}
