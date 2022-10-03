
#define EXPORT_SYMTAB
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/tty.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include "structs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gabriele Tummolo");


static int Major;
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



/*
 * Descrizione Funzione: aggiorna le variabili di stato del modulo
 * ----------------------------
 *   Valore ritorno : VOID
 *
 *   Parametri:  
 *       priorita: valore per indicare il tipo di stream da utilizzare
 *                 0 Alta priorità --- 1 Bassa Priorità
 *       minor: minor del device
 *       operazione: valore per indicare il tipo di operazione che si vuole
 *                   effettuare, 0 Scrittura --- 1 Lettura
 *       tipo: valore che indica il tipo di variabile di stato da aggiornare
 *             0 Thread in attesa --- 1 Byte validi dello stream
 *       bytes: the other real value
 *   
 */
void aggiorna_variabili(int priorita,int minor,int operazione,int tipo,int bytes){
   if(priorita == 0){
      if(operazione == 0){
         if(tipo == 0){
            __sync_fetch_and_add(&thread_in_attesa_alta_priorita[minor],1);
         }else{
            __sync_fetch_and_add(&byte_validi_alta_priorita[minor],bytes);
         }
      }else{
         if(tipo == 0){
            __sync_fetch_and_sub(&thread_in_attesa_alta_priorita[minor],1);
         }else{
            __sync_fetch_and_sub(&byte_validi_alta_priorita[minor],bytes);
         }
      }
   }else{
      if(operazione == 0){
         if(tipo == 0){
            __sync_fetch_and_add(&thread_in_attesa_bassa_priorita[minor],1);
         }else{
            __sync_fetch_and_add(&byte_validi_bassa_priorita[minor],bytes);
         }
      }else{
         if(tipo == 0){
            __sync_fetch_and_sub(&thread_in_attesa_bassa_priorita[minor],1);
         }else{
            __sync_fetch_and_sub(&byte_validi_bassa_priorita[minor],bytes);
         }
      }
   }
}

/*
 * Descrizione Funzione: implementa la scrittura differita per lo stream a bassa priorità
 * ----------------------------
 *   Valore ritorno : VOID
 *
 *   Parametri:  
 *       work: work_struct utilizzata per recuperare i dati della sessione
 *   
 */
void deferred_work(struct work_struct *work){
   int minor;
   int len;
   data_work *data;
   info_device *the_object;
   //Recupero informazioni
   data = container_of(work,data_work,work);
   len = data->len;
   minor = data->minor;
   the_object = objects + minor;
   printk(KERN_INFO "%s:Inizio Deferred Write...\n",MODNAME);
   mutex_lock(&(the_object->mutex_op[1]));
   //Ri-dimensionamento Stream
   the_object->streams[1] = krealloc(the_object->streams[1],the_object->bytes_validi[1] + len,GFP_ATOMIC);
   //Scrittura sul buffer
   strncat(the_object->streams[1],data->buffer,len);
   the_object->bytes_validi[1] += len;
   aggiorna_variabili(0,minor,0,1,len);
   mutex_unlock(&(the_object->mutex_op[1]));
   printk(KERN_INFO "%s:...Fine Deferred Write\n",MODNAME);

   return;
}

/*
 * Descrizione Funzione: funzione per l'acquisizione del lock dei due differenti stream
 * ----------------------------
 *   Valore ritorno : VOID
 *
 *   Parametri:  
 *       sessione_c: struttura della sessione corrente
 *       mutex: mutex utilizzato per lo stream corrente
 *       coda_attesa: wait_queue
 *       priorita: valore per indicare il tipo di stream da utilizzare
 *                 0 Alta priorità --- 1 Bassa Priorità
 *       minor: minor del device
 *   
 */
bool prendi_lock(info_sessione *sessione_c,struct mutex * mutex, wait_queue_head_t * coda_attesa,int priorita,int minor){
   if(sessione_c->tipo_operaz == 1){ 
      //Operazione non bloccante
      if(mutex_trylock(mutex) == 1) 
      //Lock preso
      {  
         printk(KERN_INFO "%s:Lock preso operazione non bloccante\n",MODNAME);
         return true;  
      } 
      else
      //Lock non preso
      {
         printk(KERN_INFO "%s:Lock non preso operazione non bloccante\n",MODNAME);
         return false 
      }
   }
   else
   {
      int timeout = msecs_to_jiffies(sessione_c->timeout);
      aggiorna_variabili(priorita,minor,0,0,0);
      printk(KERN_INFO "%s:Provo a prendere il lock...\n",MODNAME);
      if(wait_event_timeout(*coda_attesa, (mutex_trylock(mutex) == 1), timeout) == 0){
         printk(KERN_INFO "%s:Lock per operazione bloccante non preso\n",MODNAME);
         aggiorna_variabili(priorita,minor,1,0,0);
         return false;
      }else{
         printk(KERN_INFO "%s:Lock preso operazione bloccante\n",MODNAME);
         aggiorna_variabili(priorita,minor,1,0,0);
         return true;
      }
   } 
}

/*
 * Descrizione Funzione: funzione che effettua la strittura sul device
 * ----------------------------
 *   Valore ritorno : ssize_t
 *
 *   Parametri:  
 *       filp: struttura dati
 *       buff: buffer che contiene i byte che si vogliono scrivere
 *       len: numero di byte che si vogliono scrivere
 *       off: offset
 *   
 */
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
   printk(KERN_INFO "%s: Scrittura chiamata per il device con [major,minor] number [%d,%d]\n",MODNAME,get_major(filp),get_minor(filp));
   
   buffer_temporaneo  = kzalloc(sizeof(char)*len,GFP_ATOMIC);
   if(buffer_temporaneo == NULL){
      printk(KERN_INFO "%s:Allocazione buffer temporaneo per scrittura FALLITA\n",MODNAME);
      return -1;
   }


   ret = copy_from_user(buffer_temporaneo, buff, len);


   if(pr_c == 1)
   //Bassa priorità
   {
      data = kzalloc(sizeof(data_work),GFP_KERNEL);
      data->minor = minor;
      data->buffer = buffer_temporaneo;
      data->len = len;
      INIT_WORK(&data->work,deferred_work);
      queue_work(workqueue, &data->work);
   }else if(prendi_lock(sessione_c,&(the_object->mutex_op[pr_c]),&(the_object->coda_attesa[pr_c]),pr_c,minor)){
      //Alta Priorità
      //Ri-dimensionamento stream
      the_object->streams[pr_c] = krealloc(the_object->streams[pr_c],the_object->bytes_validi[pr_c] + len,GFP_ATOMIC);
      //Scrittura sullo stream
      strncat(the_object->streams[pr_c],buffer_temporaneo,len);
      the_object->bytes_validi[pr_c] += len;
      aggiorna_variabili(pr_c,minor,0,1,len);
      mutex_unlock(&(the_object->mutex_op[pr_c])); 
      wake_up(&the_object->coda_attesa[pr_c]);
   }else{
      return 0;
   }


   return len - ret;

}


/*
 * Descrizione Funzione: funzione che effettua l'apertura della sessione per il device
 * ----------------------------
 *   Valore ritorno : int
 *
 *   Parametri:  
 *       inode: struttura inode
 *       file: struttura dati
 *   
 */
static int apertura_device(struct inode *inode, struct file *file) {

   int minor;
   info_sessione *sessione_c;
   minor = get_minor(file);

   if(stato_devices[minor] == 1){
      printk(KERN_ERR "%s: Errore: Impossibile aprire una sessione su un Dispositivo Disabilitato!\n",MODNAME);
      return 0;
   }

   if(minor >= MINORS){
	   return -ENODEV;
   }
   sessione_c = kzalloc(sizeof(sessione_c), GFP_ATOMIC);
   sessione_c->priorita = 0;
   sessione_c->tipo_operaz = 1;
   sessione_c->timeout = 3000;
   file->private_data = sessione_c;

   printk(KERN_INFO "%s: Device file aperto con successo per l'oggetto con minor number %d\n",MODNAME,minor);
   return 0;


}

/*
 * Descrizione Funzione: funzione che effettua la chiusura della sessione per il device
 * ----------------------------
 *   Valore ritorno : int
 *
 *   Parametri:  
 *       inode: struttura inode
 *       file: struttura dati
 *   
 */
static int rilascio_device(struct inode *inode, struct file *file) {
   int minor;
   minor = get_minor(file);

   kfree(file->private_data);

   printk(KERN_INFO "%s: Device file chiuso\n",MODNAME);

   return 0;

}



/*
 * Descrizione Funzione: funzione che effettua la strittura sul device
 * ----------------------------
 *   Valore ritorno : ssize_t
 *
 *   Parametri:  
 *       filp: struttura dati
 *       buff: buffer -- non utilizzato
 *       len: numero di byte che si vogliono leggere
 *       off: offset
 *   
 */
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
   //Allocazione buffer temporaneo per copiare i dati da leggere
   buffer_temporaneo  = kzalloc(sizeof(char)*len,GFP_ATOMIC);
   if(buffer_temporaneo == NULL){
      printk(KERN_ERR "%s:Allocazione buffer temporaneo per lettura FALLITA\n",MODNAME);
      return -1;
   }

   if(prendi_lock(session,&(the_object->mutex_op[pr_c]),&(the_object->coda_attesa[pr_c]),pr_c,minor)){

      if(len > bytes_validi)
      { //Verifica se il numero di byte da leggere sono maggiori dei byte disponibilià 
              len = bytes_validi;
      }
      printk(KERN_INFO "%s:Inizio Lettura di %d bytes per lo stream [%d]",MODNAME);
      //Copio il contenuto dello stream nel buffer temporaneo
      memmove(buffer_temporaneo, the_object->streams[pr_c],len);
      printk(KERN_INFO "%s:1...2...3\n",MODNAME);
      //Le 2 operazioni successive mi permetto di eliminare dallo stream i bytes che sono stati letti
      memmove(the_object->streams[pr_c], the_object->streams[pr_c] + len,bytes_validi -len);
      //Ri-dimensionamento dello stream dopo la lettura
      the_object->streams[pr_c] = krealloc(the_object->streams[pr_c],bytes_validi - len,GFP_ATOMIC);
      //Aggiornamento dei bytes validi per lo stream considerato
      the_object->bytes_validi[pr_c] -= len;
      aggiorna_variabili(pr_c,minor,1,1,len);
      mutex_unlock(&(the_object->mutex_op[pr_c])); 
      wake_up(&the_object->coda_attesa[pr_c]);
   }else{
      return 0;
   }   

   
   ret = copy_to_user(buff,buffer_temporaneo,len);
   kfree(buffer_temporaneo);
   return len-ret;

}

/*
 * Descrizione Funzione: funzione che permette di modificare la priorità,timeout e tipologia
                         di operazione (Bloccante/Non Bloccante)
 * ----------------------------
 *   Valore ritorno : long
 *
 *   Parametri:  
 *       filp: struttura dati
 *       commandd: tipo di comando passato
 *       param: parametro che permette di modificare il timeout
 *   
 */
static long operazione_ioctl(struct file *filp, unsigned int command, unsigned long param) {

  int minor = get_minor(filp);
  info_device *the_object;
  info_sessione *session = filp->private_data;
  the_object = objects + minor;
  switch(command){
      case 0: // modifica la priorità in alta
         session->priorita = 0;
         printk(KERN_INFO "%s:Cambio priorità ALTA\n",MODNAME);
         break;
      case 1: // modifica priorità in bassa
         session->priorita = 1;
         printk(KERN_INFO "%s:Cambio priorità BASSA\n",MODNAME);
         break;
      case 2: // modifica operazione in bloccante
         session->tipo_operaz = 0;
         printk(KERN_INFO "%s:Cambio tipo operazione BLOCCANTE\n",MODNAME);
         break;
      case 3: // modifica operazione in non-bloccante
         session->tipo_operaz = 1;
         printk(KERN_INFO "%s:Cambio tipo operazione NON BLOCCANTE\n",MODNAME);
         break;
      case 4: // modifica timeout espresso in millisecondi
         session->timeout = param;
         printk(KERN_INFO "%s:Cambio TIMEOUT = %d\n",MODNAME,(int)param);
         break;
      case 5: // abilita Device
         stato_devices[minor] = 0;
         printk(KERN_INFO "%s:Abiltazione Device con Minor %d\n",MODNAME,minor);
         break;
      case 6: // disabilita Device
         stato_devices[minor] = 1;
         printk(KERN_INFO "%s:Disabiltazione Device con Minor %d\n",MODNAME,minor);
         break;
      default:
         printk(KERN_INFO "%s:Comando errato\n",MODNAME);

  }
  return 0;

}



int inizializzazione_modulo(void) {
   int i;
   //Creazione Workqueue
   workqueue = create_workqueue("workqueue");
   //Inizializzazione strutture per i minors
   for(i=0;i<MINORS;i++){
      init_waitqueue_head(&objects[i].coda_attesa[0]);
      init_waitqueue_head(&objects[i].coda_attesa[1]);
      objects[i].bytes_validi[0] = 0;
      objects[i].bytes_validi[1] = 0;
      objects[i].streams[0] = NULL;
      objects[i].streams[1] = NULL;
      mutex_init(&(objects[i].mutex_op[0]));
      mutex_init(&(objects[i].mutex_op[0]));
   }
   //Registrazione device
   Major = __register_chrdev(0, 0, 256, DEVICE_NAME, &fops);
   if (Major < 0) {
      printk(KERN_ERR "%s: Registrazione device fallita\n",MODNAME);
      return Major;
   }
   printk(KERN_INFO "%s: Device registrato con successo, il major number e' %d\n",MODNAME, Major);
   return 0;
}

void rilascio_modulo(void) {
   int i;
   //Flush e cancellazione della workqueue
   flush_workqueue(workqueue);
   destroy_workqueue(workqueue);
   for(i=0;i<MINORS;i++){
      kfree(objects[i].streams[0]);
      kfree(objects[i].streams[1]);
   }

   unregister_chrdev(Major, DEVICE_NAME);
   
   printk(KERN_INFO "%s: Cancellazione Device effettuata con successo! Il Major number era %d\n",MODNAME, Major);


	return;

}

module_init(inizializzazione_modulo);
module_exit(rilascio_modulo);
