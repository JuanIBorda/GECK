#ifndef SHARED_H
#define SHARED_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
//#include <commons/bitarray.h>
#include <commons/config.h>

#include <sys/stat.h>
#include <pthread.h> 	// Para crear hilos! :D
#include <semaphore.h>	// Para crear semaforos! :D
#include <time.h>
#include <math.h>

/*---------------------------------- INSTRUCTIONS ----------------------------------*/
// https://stackoverflow.com/questions/4014827/how-can-i-compare-strings-in-c-using-a-switch-statement (Gracias por tanto, perdon por tan poco)

#define BADKEY -1
#define I_SET 1
#define I_ADD 2
#define I_IO 3
#define I_MOV_IN 4
#define I_MOV_OUT 5
#define I_EXIT 6

int keyfromstring(char *key);

/*---------------------------------- STRUCTS ----------------------------------*/
typedef struct {
	int size;
	void* stream;
} t_buffer;

typedef enum {
	PACKAGE,
	INPUT_OUTPUT_INTERRUPTION,
	IO_SCREEN,
	IO_KEYBOARD,
	IO_DEFAULT, //Para los casos de IO desconocidos (ej: DISCO).
	IO_SCREEN_RESPONSE,
	IO_KEYBOARD_RESPONSE,
	IO_DEFAULT_RESPONSE, //Para los casos de IO desconocidos (ej: DISCO).
	INIT_PROCESS,
	INIT_STRUCTURES,
	CONFIG_MEMORY,
	EXECUTE_INTERRUPTION, 
	EXECUTE_PCB,		
	SEND_CONFIG,
	END_CONSOLE,
	END_PROCESS,
	END_STRUCTURES,
	DIR_FISICA,
	TABLA_PAGS,
	REQUEST_VALUE_FROM_PHYSICS_ADDR,
	RESPONSE_VALUE_FROM_PHYSICS_ADDR,
	INIT_PROCESS_STRUCTURES_MEMORY,
	INIT_PROCESS_STRUCTURES_MEMORY_RESPONSE,
	INIT_STRUCTURES_MEMORY, // general
	INIT_STRUCTURES_MEMORY_RESPONSE,
	SEG_FAULT,// cpu le avisa a kernel que el proceso tiene seg fault y lo tiene q matar
	END_CONSOLE_SEG_FAULT,  // matar consola por porque tiene codigo de segfault
	FRAME_REQUEST, 
	FRAME_RESPONSE,
	REQUEST_PAGE_FAULT, // Este lo usa kernel para pedirle a memoria que solucione un page foult
	RESPONSE_PAGE_FAULT,// Este lo usa memoria para avisarle a kernel que soluciono un page foult
	PAGE_FAULT, // este lo usa memoria para avisarle a cpu q tiene un page foult
	MOV_OUT,	//Este lo usamos para mandarle a memoria
	MOV_OUT_OK,	// Este lo usamos para confirmar la finalizacion de mov_out
	MOV_OUT_ERROR, // para confirmar la finlaizacion de mov out pero caso de error
	MOV_IN,
	MOV_IN_OK,
	MOV_IN_ERROR
} operation_code;

typedef struct {
	operation_code op_code; 
	t_buffer* buffer;
} t_package;

typedef enum { // Los estados que puede tener un PCB
    NEW,
    READY,
    BLOCKED,
    RUNNING,
    EXIT,
} pcb_status;

// Preguntar a ayudante
#define AX 0
#define BX 1
#define CX 2
#define DX 3

#define REQUEST 0
#define RESPONSE 1


typedef struct {
	int index_page; // Indice de la tabla de paginas que esta en memoria!!

	int number; // Numero de segmento
	int size; // Tamaño de segmento
	 
} t_segment_table; // esto es SEGMENTO, NO LA TABLA DE SEGMENTOS --> CAMBIAR A SEGMENTO_ROW 

typedef struct {
    int process_id; //process id identificador del proceso.
    int size;
    int program_counter; // número de la próxima instrucción a ejecutar.
	int client_socket;
    uint32_t cpu_registers[4]; //Estructura que contendrá los valores de los registros de uso general de la CPU. mas adelante hay que pensarla
    char** instructions; // lista de instrucciones a ejecutar. char**
	pcb_status status;
	t_list* segment_table; // va a contener elementos de tipo t_segment_table
    //t_segment_table** segment_table; //[numero_Segmento,tamaño_segmento,numero/indice tabla de paginas]
	// int cantidad_segmentos ----> 
} t_pcb;


typedef enum {
	FIFO,
	RR,
    FEEDBACK
} algoritmh;


/*---------------------------------- NETWORK.H ----------------------------------*/

// Conexion servidor -> cliente
int server_init(char*, char*, t_log*);
int wait_client(int, t_log*);
int receive_operation(int client_socket);

// Conexion cliente -> servidor
int create_connection(char* ip, char* port, t_log* logger); 
void close_connection(int socket);

/*---------------------------------- PCB_UTILS.H ----------------------------------*/
t_pcb* pcb_create(char** instructions, int client_socket, int pid, char** segments);
char** generate_instructions_list(char* instructions);

char* get_state_name(int state);
void logguar_state(t_log* logger, int state);

t_pcb* receive_pcb(int socket, t_log* logger);

void change_pcb_state_to(t_pcb* pcb, pcb_status newState);
void loggear_pcb(t_pcb* pcb, t_log* logger);
void loggear_registers(t_log* logger, uint32_t* registers);



/*---------------------------------- PROCESS_UTILS.H ----------------------------------*/
typedef struct { // Paquete enviado de Consola -> Kernel, con el que generamos un PCB.
	char* instrucciones;
 	int tamanio;
} t_proceso_consola; 

/*---------------------------------- SERIALIZE.H ----------------------------------*/
t_package* create_package(operation_code);
void create_buffer(t_package*);
void* serialize_package(t_package* package, int bytes);
void send_package(t_package* package, int client_socket);
void send_pcb_package(int connection, t_pcb* pcb, operation_code code);
void send_pcb_io_package(int client_socket, t_pcb* pcb, char* device, char* parameter, int status);
void delete_package(t_package* package);

void* receive_buffer(int*, int);
void* receive_after_pcb_buffer(int* size, int client_socket);

int read_int(char* buffer, int* desp);
char* read_string(char* buffer, int* desp); 
char** read_string_array(char* buffer, int* desp);
uint32_t read_uint32(char* buffer, int* desp);
t_pcb* read_pcb(void* buffer, int* desp);

void add_registers_to_package(t_package* package, uint32_t* cpu_registers);
void add_uint32_to_package(t_package* package, uint32_t x);
void add_pcb_to_package(t_package* package, t_pcb* pcb);
void add_int_to_package(t_package* package, int x);
void add_float_to_package(t_package* package, float x);
void add_string_array_to_package(t_package* package, char** arr);
void add_value_to_package(t_package* package, void* value, int size);
void add_segment_table_to_package(t_package* package, t_list* segment_table_list);
// Manejo de pcb con parametros -> Hay que testearlo bien


#endif