#ifndef CPU_H
#define CPU_H

#include <stdio.h>
#include <stdbool.h>
#include <signal.h>

#include <commons/log.h>

#include <math.h>
double floor(double arg);

//#include "network.h"
//#include "serialize.h"
#include "shared.h"

#include <pthread.h> 
#include <semaphore.h>

#include <sys/time.h>
#include <time.h>

/*--------------- variables de archivo configuracion ---------------*/
typedef struct {
    int ENTRADAS_TLB; 
    char* REEMPLAZO_TLB;
    char* RETARDO_INSTRUCCION; 
    char* IP_MEMORIA;
    char* PUERTO_MEMORIA; 
    char* IP_CPU; 
    char* CLAVE; 
    char* PUERTO_ESCUCHA_DISPATCH;
    char* PUERTO_ESCUCHA_INTERRUPT;
} CPU_config_struct; 

CPU_config_struct cpu_config_struct;

t_config* config_cpu;
t_log* logger;
t_log* mandatory_logger;

t_config* init_config(char*);
void init_memory_config();
void load_config();

/*--------------- CONFIG QUE NECESITAMOS TRAERNOS DE MEMORIA ---------------*/
typedef struct {
    int TAM_PAGINA;
    int ENTRADAS_POR_TABLA;
} Config_memory_in_cpu;

Config_memory_in_cpu config_memory_in_cpu;

/*--------------- CONEXIONES CON MODULOS ---------------*/

void handshake_memory();
int connect_with_memory();
int receive_memory_socket(int client_socket);

/*-------PROCESOS--------*/
void process_dispatch();
void process_interrupt();


int check_interruption;
int page_fault;
int sigsegv;
int numeroSegmentoGlobalPageFault;
int numeroPaginaGlobalPageFault;
pthread_mutex_t m_execute_instruct;
//pthread_mutex_t mutex_interruption; //creo el mutex para la interrucion

char* fetch_next_instruction_to_execute(t_pcb* pcb);
char** decode(char* linea);
void update_program_counter(t_pcb* pcb);
void update_pcb_status(t_pcb* pcb, pcb_status Status);

void execute_instruction(char** instruccion_a_ejecutar, t_pcb* pcb);
void execute_process(t_pcb* pcb);

void save_context_pcb(t_pcb* pcb);

// REGISTROS 
uint32_t registers[4];

void init_registers();
void set_registers(t_pcb* pcb);
void init_semaphores();
void add_value_to_register(char* registerToModify, char* valueToAdd);

uint32_t findValueOfRegister(char* register_to_find_value);

void add_two_registers(char* registerToModify, char* registroParaSumarleAlOtroRegistro);

void send_pcb_package(int connection, t_pcb* pcb, operation_code code);


// --------------------- MMU ---------------------//
typedef struct {
    int max_segment_size; // tamaño maximo de segmento
    int segment_number; // numero de segmento
    int segment_offset; // desplazamiento de segmento
    int page_num; // numero de pagina
    int page_offset;// desplazamiento de pagina
} Logical_address_parameters;

// este  t_traslation_locacion_buffer lo usamos?
typedef struct{
    int pid;
    int segment_number;
    int page_number;
    int marco;
    //int cantidad_entradas;
    //char* aglforitmos_reemplazo;

} t_traslation_locacion_buffer; // tlb

Logical_address_parameters* translate_logical_address(int logical_address, t_pcb* pcb);

uint32_t fetch_value_in_memory(int physical_adress, t_pcb* pcb, Logical_address_parameters* logical_params);

void store_value_in_register(char* register_mov_in, uint32_t value);

int socket_memory; // lo hice global porque otras funciones lo necesitan
int socket_kernel;


typedef struct{ //estructura [ pid | segmento | página | marco ] 
    int process_id;
    int segment; 
    int page;
    int frame; // o marco en memoria
    // esto estaba en otro tp lo dejo comentado porque no se como se usa pero lo vamos a usar
    struct timeval init_time; //fifo hora minuto y segundo de cuando el proceso entro por primera vez
    struct timeval last_reference; //lru 
} row_tlb; // tlb : [fila_tlb1,fila_tlb2,fila_tlb3] y los elementos q tiene adentro el struct son la columna

t_list* TLB; // la tlb es una lista :D que adentro tiene structs de row_tlb



bool compare_last_reference(row_tlb* first_row, row_tlb* second_row);

void loggear_TLB();
void add_to_tlb(int process_id, int segment, int page_number, int frame);

void add_row_to_TLB(int process_id, int segment, int page_number, int frame);

void destroy_row_TLB(row_tlb* row_to_destroy);

void write_value(int physical_adress,uint32_t register_value_mov_out, int segment_index, int pid);

Logical_address_parameters* translate_logical_address(int logical_address, t_pcb* pcb);
int present_in_tlb(int page_number, int segment_number, int pcb_id);


void init_tlb();


int calculate_physical_address(int marco, int offset, int page_size);
int build_physical_address(Logical_address_parameters* logical_address_params, t_pcb* pcb);
int find_frame(Logical_address_parameters* logical_address_params, t_pcb* pcb);

#endif