#ifndef KERNEL_H
#define KERNEL_H


#include <stdio.h>
#include <signal.h>

#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/temporal.h>
#include <stdbool.h>

#include <sys/stat.h>

//#include "network.h"
//#include "serialize.h"
//#include "pcb_utils.h"
#include "shared.h"


#include <pthread.h> 
#include <semaphore.h>

t_log* logger;
t_log* mandatory_logger;

int pid_counter = 1;
int quantum_round_robin;
int ignore_exit = 0;
int flag_out_of_Running;
char* ultimo;

typedef struct
{
    char* IP_KERNEL;
	char* PUERTO_KERNEL;
	char* IP_MEMORIA;
	char* PUERTO_MEMORIA;
    char* IP_CPU; 		
    char* PUERTO_CPU_DISPATCH;
    char* PUERTO_CPU_INTERRUPT;
    char* PUERTO_ESCUCHA;
    char* ALGORITMO_PLANIFICACION; 
    int GRADO_MAX_MULTIPROGRAMACION;
    char** DISPOSITIVOS_IO; 
    char** TIEMPOS_IO; 
    char* QUANTUM_RR;
} t_kernel_config;

typedef struct{
    char* device_name; // Esto esta TOTALMENTE AL PEDO
    int device_locked_time;
    t_list* list_processes;
    sem_t sem_devices; 
} t_device;

t_kernel_config* config_kernel;

t_config* config;

// conexiones
int memory_connection;
int connect_with_memory();

int connection_cpu_dispatch;
int connection_cpu_interrupt;

int connect_with_cpu_interrupt();
int connect_with_cpu_dispatch();


void load_kernel_config();

void receive_console(int client_socket);
t_pcb* pcb_init(int socket);
t_config* init_config(char* path);

/*---------------------- Listas de estados de tipo de planificacion ----------------------*/
t_list* list_NEW;        // NEW
t_list* list_READY;         // READY

t_list* list_READY_FIFO;
t_list* list_READY_RR;

t_list* list_BLOCKED;    // BLOCKED

t_list* list_RUNNING;    // RUNNING (EXEC)
t_list* list_EXIT;   // EXIT   
t_list* list_IO;

// DICCIONARIO PARA LOS PERIFERICOS ESTANDAR
t_dictionary* dictionary_io;

//semaforos
sem_t sem_grado_multiprogamacion; // planificarCP y sig_to_Ready
sem_t sem_process_in_ready;
sem_t sem_io_arrived;
sem_t sem_cpu_free_to_execute;
sem_t sem_pcb_create;
sem_t sem_interruption_quantum;
sem_t sem_interruption_quantum_finish;
sem_t sem_beggin_quantum;
sem_t sem_init_process_memory;
sem_t sem_schedule_ready;

pthread_mutex_t m_list_NEW;
pthread_mutex_t m_list_READY;

pthread_mutex_t m_list_READY_FIFO;
pthread_mutex_t m_list_READY_RR;

pthread_mutex_t m_list_BLOCKED;

pthread_mutex_t m_list_RUNNING;
pthread_mutex_t m_list_EXIT;
pthread_mutex_t m_list_IO;

pthread_mutex_t m_ignore_exit;
pthread_mutex_t m_out_of_Running;

pthread_mutex_t m_resolve_page_fault;





// semaforos
pthread_t short_term_scheduler;
pthread_t thread_Dispatch;
pthread_t thread_Interrupt;
pthread_t thread_memory;
pthread_t thread_IO;
// pthread_t execute_io;

void init_global_lists();
void init_semaphores();
void init_scheduling();

void schedule_next_to(t_pcb* pcb, pcb_status state, t_list* list, pthread_mutex_t mutex);
void add_to_list_with_sems(t_pcb* pcb_to_add, t_list* list, pthread_mutex_t m_sem);
void schedule_next_to_new(t_pcb* pcb);
// NACHO DEJA DE ARRUINAR EL TP POR FAVOR
void schedule_next_to_ready();
void schedule_next_to_running();


void turn_on_switcher_clock_RR();
void count_quantum_of_RoundRobin();


void manage_dispatch();
void manage_interrupt();
void manage_memory();

void send_end_to_console(int console); 

void execute_io_default(t_pcb* pcb, char* device, int parameter);
void simulate_io_default(t_device* device);
void create_queue_for_io(char* io_name, char* time);
void init_device_config_dictionary();

void resolve_page_fault(t_list* arguments_list);

// KERNEL ------> MEMORIA 
void init_structures_memory(t_pcb* pcb);

// KERNEL  <------ MEMORIA


//FOR LOGS

void loggear_cola_ready(char* algoritmo);

char* pids_on_ready_FIFO();

char* pids_on_ready_RR();

char* pids_on_ready();

#endif