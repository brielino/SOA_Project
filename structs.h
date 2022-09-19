/* Strutture principali utilizzate */
typedef struct _session_info{
	int priority; //Priorità Alta 0 (High) , Priorità Bassa 1 (Low) 
	int type_op; // Operazione Bloccante 0, Operazione non Bloccante 1 
	unsigned long timeout; //Timeout in millisecondi
} session_info;


typedef struct _device_info{
	struct mutex operation_synchronizer[2]; // Mutex per il due livelli di priorità
	wait_queue_head_t queue[2];
	int valid_bytes[2];
	char * stream_content[2];//the I/O node is a buffer in memory
} device_info;


int devices_state[MINORS];  //initially : 0 (ALL ENABLED)
int bytes_high[MINORS];
int bytes_low[MINORS];
int thread_waiting_high[MINORS];
int thread_waiting_low[MINORS]; 