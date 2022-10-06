#define MINORS 128
#define MODNAME "CHAR DEV"
#define DEVICE_NAME "my-dev"

/* Strutture principali utilizzate */
typedef struct info_sessione{
	int priorita; //Priorità Alta 0 (High) , Priorità Bassa 1 (Low) 
	int tipo_operaz; // Operazione Bloccante 0, Operazione non Bloccante 1 
	unsigned long timeout; //Timeout in millisecondi
} info_sessione;


typedef struct info_device{
	struct mutex mutex_op[2]; // Mutex per i due livelli di priorità - 0 Alta Priorità / 1 Bassa Priorità
	wait_queue_head_t coda_attesa[2]; // 0 Alta Priorità - 1 Bassa priorità
	int bytes_validi[2];//rappresenta il numero di byte per ogni stream
	char * streams[2];//Buffer per gli stream di I/O
} info_device;

typedef struct data_work{
	int minor;
	struct work_struct work;
	char *buffer;
	int len;
} data_work;


int stato_devices[MINORS];
int byte_validi_alta_priorita[MINORS];
int byte_validi_bassa_priorita[MINORS];
int thread_in_attesa_alta_priorita[MINORS];
int thread_in_attesa_bassa_priorita[MINORS]; 