#ifndef MEMORIA_H
#define MEMORIA_H

#include <stdio.h>
#include <signal.h>
#include <stdbool.h>

#include <commons/log.h>
#include <commons/bitarray.h>


//#include "network.h"
//#include "serialize.h"
#include "shared.h"

#include <pthread.h> 
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/stat.h>

t_log* logger;
t_log* mandatory_logger;
FILE* file_swap;

/* -------- configuracion --------*/

// esquema de memoria: SEGMENTACION PAGINADA 
typedef struct {
    char* PUERTO_ESCUCHA; //Puerto en el cual se escuchará la conexión de módulo.
    int TAM_MEMORIA; // Tamaño expresado en bytes del espacio de usuario de la memoria.
    int TAM_PAGINA; // Tamaño de las páginas en bytes.
    int ENTRADAS_POR_TABLA; // Cantidad de entradas de cada tabla de páginas
    int RETARDO_MEMORIA; // Tiempo en milisegundos que se deberá esperar para dar una respuesta al CPU.
    char* ALGORITMO_REEMPLAZO;// CLOCK o CLOCK-M
    int MARCOS_POR_PROCESO; // Cantidad de marcos permitidos por proceso en asignación fija.
    int RETARDO_SWAP; // Tiempo en milisegundos que se deberá esperar para cada operación del SWAP (leer/escribir).
    char* PATH_SWAP; // Carpeta donde se van a encontrar los archivos de SWAP de cada proceso.
    int TAMANIO_SWAP; // Tamaño del archivo de SWAP.
    char* IP_MEMORIA;
} Memory_config_struct; 

Memory_config_struct memory_config_struct; // perdon por los namings :( pero esto seria como la configuracion de la memoria

// ----- TABLA DE PAGINAS ----- //
typedef struct{
    int id;     // Id de cada tanña de pagina
    t_list* pages_list; // contiene elementos de tipo Page_table_row
    int segment_num;
} Page_table_struct;


t_list* all_page_tables; // contiene todas las T.paginas iniciadas en el sistema. (elementos tipo: Page_table_struct)
typedef struct{
    int frame_number;   // nro_marco
    int swap_position;

    bool presence;      // presencia
    bool use;           // uso
    bool modified;      // modificado
} Page_table_row;

typedef struct {
	int frame_number_in_memory; 
	Page_table_row* page; 
	int frame_number_in_swap; 
} row_struct_clock;

typedef struct {
	uint16_t process_id;
	t_list* frames_in_memory;  // esto guarda elementos de tipo row_struct_clock;
	uint16_t pointer;
} clock_struct;

t_dictionary* clock_structures;
clock_struct* get_struct_clock(int process_id);


void* principal_memory;

t_config* init_config(char*);
void load_config();

/* -------- variables de configuracion --------*/

t_config* config_memory; // y esto para bajarlo a la configuracion de la memoria

pthread_mutex_t thread_memory;

int socket_kernel, socket_cpu;
/* -------- conexiones --------*/

void attend_cpu();
void attend_kernel();


void init_global_lists();
void init_tables_and_memory();
t_pcb* init_process_structures(t_pcb* pcb);
void create_swap_file(); 
void init_page_table(int int_key, int size);
void loggearPageTable(Page_table_struct* tablaDePaginas);
void logguear_tabla_paginas_globales();
uint32_t fetch_value_in_memory(int physical_adress);
int* get_frame_data(int frame);

void init_principal_memory_and_swap_file();

Page_table_struct* builder_page_table(int table_page_id,int size_of_segment, int segment);
int calculate_frames_of_segment(int size_of_segment);

int use_replacement_algorithm(int process_id, Page_table_struct* page, int page_number); 

Page_table_row* page_table_attribute_modifier(Page_table_row* row, int frame_number, bool p, bool u, bool m);

void set_struct_clock(int process_id, clock_struct* e_clock);

void del_struct_clock(int process_id);

void create_clock_structure(int process_id);

void add_page_to_clock_structure(int process_id, int frame_number, Page_table_row* page, int frame_number_in_swap);
int simple_clock(int process_id, Page_table_struct* page, int page_number);
int move_to_next_pointer(int clock_pointer);
void replace_by_clock(int frame_number_in_swap, Page_table_row* page, int process_id ,int frameNumber);
int modified_clock(int process_id, Page_table_struct* page, int page_number);


int calculate_frames_of_segment(int size_of_segment);



/* ------------------------------------- SWAP FUNCTIONS -------------------------------------*/
void create_swap(); 
t_bitarray* create_bitmap(int entries);
void* read_page_swap(int position, int page_number, int segment_number, int frame, int pid);
void write_page_swap(t_bitarray* bitmap, int position, void* value, int process_id, int frame_number, int segment_number, int page_number);
void delete_page_swap(t_bitarray* bitmap, int position);
void delete_process();
int find_page_free(t_bitarray* bitmap); //IMPORTANTE: Usarlo solo cuando la pagina no tiene asignada una posicion en el swap

t_bitarray* bitmap_swap;
t_bitarray* bitmap_memory;

Page_table_struct* get_page_table_by_index(int index);
int get_frame_number_RAM(t_pcb* pcb, int page_number, int segment_number);
void* fill_frame();

uint32_t read_value_in_principal_memory(int physical_address1);
void save_value_in_memory(int physical_address1,uint32_t register_value_mov_out, int segment_index);

void logguear_tabla_paginas_globales();
int frames_in_memory_of_process(int process_id);

void testingSwap();

void move_page_memory_to_swap();
void move_page_swap_to_memory();
void send_mov_in_package(uint32_t valor);


int id_loco_global;
bool encontrandoTablas(Page_table_struct* tabla1);
void search_page_by_frame_in_memory(int process_id, int frame_number);

void update_page_use(t_pcb* pcb, int segment_number, int page_number);

#endif