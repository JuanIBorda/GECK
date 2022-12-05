#include "memoria.h"

// Funciones para el bitmap importantes!!
// bitarray_set_bit(bitmap, position); // Cambia a 1 la posicion en el bitmap para indicar que esta ocupado
// bitarray_clean_bit(bitmap, position); // Cambia a 0 la posicion en el bitmap para indicar que esta libre
// find_page_free(t_bitarray* bitmap); // Busca la proxima pagina libre (0/clean_bit) en el bitmap

/*
void testingSwap(){
	log_info(logger, "Empezando test");
	create_swap();
	
	int entries = memory_config_struct.TAMANIO_SWAP / memory_config_struct.TAM_PAGINA / 8;
	bitmap_swap = create_bitmap(entries); //TODO: Fixear 
	log_info(logger, "Bit maximo: %d", bitarray_get_max_bit(bitmap_swap));


	log_info(logger, "El proximo frame libre es: %d", find_page_free(bitmap_swap));

	
	//find_page_free(t_bitarray* bitmap); 
	uint32_t value = 80;
	uint32_t value2 = 12;
	void* page1 = malloc(memory_config_struct.TAM_PAGINA);
	memcpy(page1, &value, sizeof(uint32_t));
	memcpy(page1 + sizeof(uint32_t), &value2, sizeof(uint32_t));
	memcpy(page1 + (sizeof(uint32_t) * 2), &value, sizeof(uint32_t));
	memcpy(page1 + (sizeof(uint32_t) * 3), &value, sizeof(uint32_t));

	

	
	uint32_t n1 = malloc(sizeof(uint32_t));
	uint32_t n2 = malloc(sizeof(uint32_t));
	uint32_t n3 = malloc(sizeof(uint32_t));
	uint32_t n4 = malloc(sizeof(uint32_t));

	void* data = read_frame_swap(1);

	memcpy(&n1, data, 4);
	memcpy(&n2, data + 4, 4);
	memcpy(&n3, data + 8, 4);
	memcpy(&n4, data + 12, 4);

	log_info(logger, "El valor dentro de la posicion 1 es: %d", n1);
	log_info(logger, "El valor dentro de la posicion 2 es: %d", n2);
	log_info(logger, "El valor dentro de la posicion 3 es: %d", n3);
	log_info(logger, "El valor dentro de la posicion 4 es: %d", n4);

}*/

void end_memory_module(sig_t s){
    close_connection(socket_kernel);
    close_connection(socket_cpu);
    exit(1); 
}

int main(int argc, char** argv){
	/* 
        IMPORTANTE: Tenemos que cambiar el flag del 3er parametro de los loggers. 
        logger:
            - Testeando nosotros -> true
            - En el lab -> false
        mandatory_logger:
            - Testeando nosotros -> false
            - En el lab -> true
    */
    signal(SIGINT, end_memory_module);

    logger = log_create("./logs/memoria_extra.log", "MEMORIA", false, LOG_LEVEL_INFO);
	mandatory_logger = log_create("./logs/memoria.log", "MEMORIA", true, LOG_LEVEL_INFO);

	
    // Iniciamos configuraciones 
    t_config* config_memory = init_config(argv[1]);
	load_config(config_memory);

	// Solo habilitar en caso de querer testear swap
	//testingSwap(); // Borrar luego de testear
	//return EXIT_SUCCESS; // Borrar luego de testear

	init_global_lists();
	init_principal_memory_and_swap_file();
	
  	/* ------------------------ SERVER ------------------------ */
    int server = server_init(memory_config_struct.IP_MEMORIA, memory_config_struct.PUERTO_ESCUCHA, logger);
    log_info(logger, "Servidor iniciado");

   	log_info(logger, "Esperando que venga CPU");
    socket_cpu = wait_client(server, logger);
    pthread_t thread_cpu;
    pthread_create(&thread_cpu, NULL, (void*) attend_cpu, NULL);
	pthread_detach(thread_cpu);
	
    log_info(logger, "Esperando que venga KERNEL");
	
	while (1){ 
		socket_kernel = wait_client(server, logger);
		pthread_t thread_kernel;
		pthread_create(&thread_kernel, NULL, (void*)attend_kernel, NULL);
		pthread_detach(thread_kernel);
	}

    /* ------------------------ LIBERAR MEMORIA ------------------------ */
   	// log_destroy(logger);
   	// config_destroy(config_memory);



    return EXIT_SUCCESS;
}/* ------------------------ fin main ------------------------ */

void load_config(t_config* config_memory){
	memory_config_struct.IP_MEMORIA = config_get_string_value(config_memory, "IP_MEMORIA");
	memory_config_struct.PUERTO_ESCUCHA = config_get_string_value(config_memory, "PUERTO_ESCUCHA");	
	memory_config_struct.TAM_MEMORIA = config_get_int_value(config_memory, "TAM_MEMORIA");
	memory_config_struct.TAM_PAGINA = config_get_int_value(config_memory, "TAM_PAGINA");
	memory_config_struct.ENTRADAS_POR_TABLA = config_get_int_value(config_memory, "ENTRADAS_POR_TABLA");
	memory_config_struct.RETARDO_MEMORIA = config_get_int_value(config_memory, "RETARDO_MEMORIA");
	memory_config_struct.ALGORITMO_REEMPLAZO = config_get_string_value(config_memory, "ALGORITMO_REEMPLAZO");
	memory_config_struct.MARCOS_POR_PROCESO = config_get_int_value(config_memory, "MARCOS_POR_PROCESO");
	memory_config_struct.RETARDO_SWAP = config_get_int_value(config_memory, "RETARDO_SWAP");
	memory_config_struct.PATH_SWAP = config_get_string_value(config_memory, "PATH_SWAP");
	memory_config_struct.TAMANIO_SWAP = config_get_int_value(config_memory, "TAMANIO_SWAP");
    log_info(logger, "Config de Memoria cargada");
}

t_config* init_config(char* path){
    t_config* new_config = config_create(path);
	if (new_config == NULL) {
		log_error(logger, "No se encontro el archivo de configuracion de MEMORIA\n");
		exit(2);
	}
	return new_config;
}

//------------------------------- INICIAMOS MEMORIA PRINCIPAL ----------------------//
void init_tables_and_memory(){
	log_info(logger, "Memoria de tamanio %d y marcos x proceso: %d ", memory_config_struct.TAM_MEMORIA, memory_config_struct.MARCOS_POR_PROCESO);
	log_info(logger, "Tamanio pagina (y de marco)%d ", memory_config_struct.TAM_PAGINA);
	
	// no se si esta bien la linea de abajo
	principal_memory = malloc(memory_config_struct.TAM_MEMORIA);

    if(principal_memory == NULL){ 
		log_error(logger, "Fallo Malloc");
    	exit(1);
    } 

	int quantity_frames = memory_config_struct.TAM_MEMORIA/memory_config_struct.TAM_PAGINA;
	log_info(logger, "Cantidad de marcos de RAM: %d ", quantity_frames);
}

void init_principal_memory_and_swap_file(){
	init_tables_and_memory();
	create_swap(); // Esto despues hay que volver a levantarlo
	
	int entries_swap = memory_config_struct.TAMANIO_SWAP / memory_config_struct.TAM_PAGINA / 8;
	bitmap_swap = create_bitmap(entries_swap);

	int entries_memory = memory_config_struct.TAM_MEMORIA / memory_config_struct.TAM_PAGINA / 8;
	bitmap_memory = create_bitmap(entries_memory);
}

void init_global_lists(){ 
	clock_structures = dictionary_create();
    all_page_tables = list_create();
}

void attend_cpu() {
	int cod_op, size, desp, physical_address;
	void* buffer;
	t_package* package;

	while (1) {
		cod_op = receive_operation(socket_cpu);
        log_warning(logger, "Codigo de operacion recibido desde cpu: %d", cod_op);

		pthread_mutex_lock(&thread_memory);
		switch (cod_op) {
			case SEND_CONFIG:
				log_info(logger, "Me llegaron el pedido de los datos de config\n");	
				package = create_package(CONFIG_MEMORY);
				add_int_to_package(package, memory_config_struct.TAM_PAGINA); 
				add_int_to_package(package, memory_config_struct.ENTRADAS_POR_TABLA); 
				send_package(package, socket_cpu);
				log_info(logger, "ENVIO DESDE EL CODIGO DE SEND_CONFIG");
				delete_package(package);
				break;
			case FRAME_REQUEST: ;// cpu nos pide un frame
				usleep(memory_config_struct.RETARDO_MEMORIA * 1000);

				size = 0;
				desp = 0;
				buffer = receive_buffer(&size, socket_cpu);
																				
				t_pcb* pcb = read_pcb(buffer,&desp);
				int page_number = read_int(buffer,&desp);
				int segment_number = read_int(buffer,&desp);
				
				// logica para devolver el framo correspondiente.
				int frame_number = get_frame_number_RAM(pcb,page_number,segment_number);

				if(frame_number == -1) {
					log_info(logger, "FRAME_REQUEST --> MEM - > CPU : PAGEFAULT ");
					package = create_package(PAGE_FAULT);
					log_info(logger, "FIN ENVIANDO DESDE MEMORIA PAGE_FAULT");
				} else {
					log_info(logger, "ENVIANDO DESDE MEMORIA->CPU EL FRAME QUE SE PIDIO: %d, codigo de operacion: %d", frame_number, FRAME_RESPONSE);
					package = create_package(FRAME_RESPONSE);
					log_info(logger, "FRAME ENVIADO DESDE MEMORIA HACIA CPU");
					log_info(mandatory_logger, "PID: %d - Página: %d - Marco: %d", pcb->process_id, page_number, frame_number);
				}
				
				add_int_to_package(package, frame_number);
				send_package(package, socket_cpu);
				log_info(logger, "ENVIO DESDE EL CODIGO DE OPERACION DE FRAME REQUEST");
				delete_package(package);
				break;
			case MOV_OUT: ;
				usleep(memory_config_struct.RETARDO_MEMORIA * 1000);

				size = 0;
				desp = 0;
				buffer = receive_buffer(&size, socket_cpu);
				physical_address = read_int(buffer, &desp);
				uint32_t register_value_mov_out = read_uint32(buffer,&desp);
				int segment_index = read_int(buffer, &desp);
				int pid = read_int(buffer, &desp);

				log_info(logger, "Logueando datos recibidos MOV OUT, Direccion fisica: %d, Valor: %d", physical_address, register_value_mov_out);
				log_info(mandatory_logger, "PID: %d - Acción: ESCRIBIR - Dirección física: %d", pid, physical_address);
				save_value_in_memory(physical_address, register_value_mov_out, segment_index); // Guardamos el registro en memoria
				log_info(logger, "Guardamos correctamente el valor en memoria :D");
				
                package = create_package(MOV_OUT_OK);
				add_int_to_package(package, 1);
                send_package(package, socket_cpu);
				log_info(logger, "ENVIO DESDE EL CODIGO DE OPERACION DE MOV_OUT");
                delete_package(package);
                break;
            case MOV_IN: ;
				usleep(memory_config_struct.RETARDO_MEMORIA * 1000);

                size = 0;
                desp = 0;
                buffer = receive_buffer(&size, socket_cpu);
                physical_address = read_int(buffer, &desp);
				t_pcb* pcb_to_use = read_pcb(buffer,&desp);
				int segment_number_to_use = read_int(buffer,&desp);
				int page_number_to_use = read_int(buffer,&desp);

				log_info(logger, "Direccion fisica: %d", physical_address);

				update_page_use(pcb_to_use, segment_number_to_use, page_number_to_use);


                uint32_t value = read_value_in_principal_memory(physical_address);
				log_info(logger, "El valor de la instruccion MOV IN es: %d, y su tamaño es %ld", value, sizeof(value));
				log_info(mandatory_logger, "PID: %d - Acción: LEER - Dirección física: %d", pcb_to_use->process_id, physical_address);
				
				package = create_package(MOV_IN_OK);
				add_uint32_to_package(package, value);
				send_package(package, socket_cpu);
				delete_package(package);
                break;
			case -1:
                log_warning(logger, "El modulo cpu se desconecto");
                end_memory_module(1);
                pthread_exit(NULL);
				break;
			default:
				log_error(logger, "Operacion desconocida de CPU. Entre por default");
				break;
		}
     	pthread_mutex_unlock(&thread_memory);
	}
}


void attend_kernel() {
	int size, desp;
	void* buffer;
	
	while (1) {
		int cod_op = receive_operation(socket_kernel);
		log_warning(logger, "Codigo de operacion recibido desde kernel: %d", cod_op);
	    pthread_mutex_lock(&thread_memory);			
		switch (cod_op) {
			case INIT_PROCESS_STRUCTURES_MEMORY: 
				log_info(logger, "Iniciar estructuras del proceso en memoria (kernel -> memoria)");
				t_pcb* pcb = receive_pcb(socket_kernel, logger);
				//loggear_pcb(pcb,logger);
				
				pcb = init_process_structures(pcb);

				//loggear_pcb(pcb,logger);
				//logguear_tabla_paginas_globales();
				
				t_package* package = create_package(INIT_PROCESS_STRUCTURES_MEMORY_RESPONSE);
				add_pcb_to_package(package, pcb); //preguntar si hay problema en recibir el pcb entero
				send_package(package, socket_kernel);
				delete_package(package);

				break;
			case REQUEST_PAGE_FAULT:
				log_info(logger, "VOY A SOLUCIONAR EL REQUEST_PAGE_FAULT QUE ME MANDO KERNEL");
				// kernel le esta pidiendo a memoria que solucione el page fault llevando una pagina de swap a memoria
				size = 0;
				desp = 0;
				buffer = receive_buffer(&size, socket_kernel);
				pcb = read_pcb(buffer,&desp);
				int segment_number = read_int(buffer, &desp);
				int page_number = read_int(buffer, &desp);
				log_info(logger, "Recibi numero segmento: %d numero de pagina: %d", segment_number, page_number);
				//loggear_pcb(pcb,logger);
			
				Page_table_row* page_to_load_in_memory;
				int frame_number;

				// encontrar la pagina que queremos llevar a RAM  en las T.paginas
				t_list* segment_table = pcb->segment_table;
				t_segment_table* segment_data = list_get(segment_table,segment_number);
				int id_table_page = segment_data->index_page;
				Page_table_struct* page_table = get_page_table_by_index(id_table_page);
				
				if(page_table == NULL) {
					log_error(logger, "No se encontro la tabla de paginas");
					break;
				}
				else {
					// encontramos la T.Paginas , ahora buscamos la pagina en la T.paginas
					log_info(logger, "Se encontro la tabla de paginas");
					page_to_load_in_memory = list_get(page_table->pages_list, page_number);
				}
				// CON page_to_load_in_memory la voy a buscar a SWAP.

				// Si ya ocupa todos los marcos que puede ocupar, hacer un reemplazo (EJECUTAR ALGORITMO)		
				if (frames_in_memory_of_process(pcb->process_id) == memory_config_struct.MARCOS_POR_PROCESO){ // ACA USAS ALGORTIMO DE REEMPLAZO
					//Devuelve el marco donde estaba la víctima, que es donde voy a colocar la nueva página
					frame_number = use_replacement_algorithm(pcb->process_id, page_table, page_number); 
					
					// de frame_number hay que encontrar que pagina es!
				}
				else {
					// hay que buscar un marco libre en RAM! 
					
					frame_number = find_page_free(bitmap_memory); 
					log_info(logger, "[CPU] El proceso tiene marcos disponibles en RAM :D");
			
				}

				log_info(logger, "Leemos la pagina de swap");
				void* page = read_page_swap(page_to_load_in_memory->swap_position, page_number, segment_number,frame_number, pcb->process_id);

				memcpy(principal_memory + (frame_number * memory_config_struct.TAM_PAGINA), page, memory_config_struct.TAM_PAGINA);

				log_info(logger, "Cambiamos el bitmap de memoria");
				bitarray_set_bit(bitmap_memory, frame_number);

				page_to_load_in_memory->frame_number = frame_number;
				page_to_load_in_memory->presence = 1;
				page_to_load_in_memory->use = 1;

				add_page_to_clock_structure(pcb->process_id, frame_number, page_to_load_in_memory, page_to_load_in_memory->swap_position);

				log_warning(logger, "SOLUCIONADO PAGE FAULT Enviamos a kernel RESPONSE_PAGE_FAULT, pid: %d", pcb->process_id);
				t_package* packgagePF = create_package(RESPONSE_PAGE_FAULT);
				add_pcb_to_package(packgagePF, pcb);
				send_package(packgagePF, socket_kernel);
				delete_package(packgagePF);
				
				break;
			case END_STRUCTURES:
				size = 0;
				desp = 0;
				buffer = receive_buffer(&size, socket_kernel);
				pcb = read_pcb(buffer, &desp);
				delete_process(pcb);
				break;
			case -1:
				log_warning(logger, "El kernel se desconecto");
                end_memory_module(1);
                pthread_exit(NULL);
				break;
			default:
				log_warning(logger, "Operacion desconocida de Kernel. Entro por default");
				break;
		}
    	pthread_mutex_unlock(&thread_memory);
	}
}

int frames_in_memory_of_process(int process_id) {
	clock_struct* clock = get_struct_clock(process_id);
	return list_size(clock->frames_in_memory);
}

int page_tables_counter = 0; // Usada para conocer la cantidad de tablas de paginas en el sistema y el ID de cada una

t_pcb* init_process_structures(t_pcb* pcb){
	log_info(logger, "Por iniciar estructuras");
	create_clock_structure(pcb->process_id);
	int amount_of_segments = list_size(pcb->segment_table); // Cantidad de tabla de paginas a crear  
	//t_list* sizeOfSegment;
	for(int i = 0; i < amount_of_segments; i++){									
		t_segment_table* segment_table = list_get(pcb->segment_table, i);			
		segment_table->index_page = page_tables_counter; // Le agrego al segmento la ID de la T.paginas q le corresponde 

		log_info(logger, "Tamaño de segmento:  %d", segment_table->size);
		Page_table_struct* page_table_of_segment = builder_page_table(page_tables_counter, segment_table->size, i);
		log_info(mandatory_logger, "PID: %d - Segmento: %d - TAMAÑO: %d paginas", pcb->process_id, i, calculate_frames_of_segment(segment_table->size));
		page_tables_counter++; // Aumento ID y cant T.paginas en sistema
		
		list_replace(pcb->segment_table, i, segment_table);

		
		list_add(all_page_tables, page_table_of_segment); // Una vez que le meti el ID de la T.Pagina a la T.segmento lo agrego de nuevo a la lista de segmentos
		log_info(logger,"Llegue a la tercera parte");
	}
	// actualizo la tabla del segmento al pcb

	log_info(logger, "Retornando pcb");

	return pcb;
}


void loggearPageTable(Page_table_struct* tablaDePaginas){
	int amount_pages = list_size(tablaDePaginas->pages_list);
	log_info(logger,"El tamaño de la tabla de paginas es: %d",amount_pages);
	//int id;     // Id de cada tanña de pagina
   
	
	log_info(logger,"La tabla de paginas tiene ID: %d",tablaDePaginas->id);
	for(int i = 0;i < amount_pages;i++){
		Page_table_row* page = list_get(tablaDePaginas->pages_list, i);
	
		log_info(logger,"frame : %d | presencia : %d | uso : %d | modificado : %d | swap_position : %d",page->frame_number,page->presence,page->use,page->modified,page->swap_position);
		
	}

}

Page_table_row* page_table_attribute_modifier(Page_table_row* row, int frame_number, bool p, bool u, bool m){
	row->frame_number = frame_number;
	row->presence = p;
	row->use = u;
	row->modified = m;
	return row;
}

void* fill_frame() {
	int registers_in_frame = memory_config_struct.TAM_PAGINA / sizeof(uint32_t);
	void* frame = malloc(memory_config_struct.TAM_PAGINA);
	int desp = 0;
	int random = rand() % 20;

	for(int i = 0;  registers_in_frame > i; i++) {
		memcpy(frame + desp, &random, sizeof(uint32_t));
		random = rand() % 20;
		desp += sizeof(uint32_t);
	}
	return frame;
}


Page_table_struct* builder_page_table(int table_page_id, int size_of_segment, int segment){
	log_info(logger, "builder_page_table");
	Page_table_struct* table = malloc(sizeof(Page_table_struct)); //nachoF: no se si este malloc esta bien // = malloc(sizeof(Fila_2do_nivel) ); // hace falta

	table->id = table_page_id;
	table->pages_list = list_create();
	table->segment_num = segment;

	for(int i = 0; i < calculate_frames_of_segment(size_of_segment); i++) {
		log_info(logger, "Dentro del for");

		Page_table_row* row = malloc(sizeof(Page_table_row));
		log_info(logger, "Busco una pagina libre");

		// guardas esta pagina en swap
		int position = find_page_free(bitmap_swap);
		log_info(logger, "Posicion de la pagina en bitmap: %d", position);

		if(position == -1) { // Si la posicion es -1 es porque no encontro una pagina libre -> estan todas ocupadas rey.
			log_error(logger, "SE LLENO EL SWAP");
		}
		bitarray_set_bit(bitmap_swap, position);

		row->frame_number = -1; // van todos iniciados pero harcodeados porque desp cuando entran
		row->presence = false; 	   // a ram, se reemplaza con los valores correspondientes
		row->use = false;
		row->modified = false;
		row->swap_position = position; 

		list_add(table->pages_list, row);  
	}

	return table;
}

// Devuelve la cantidad de marcos que requiere un proceso del tamanio especificado
int calculate_frames_of_segment(int size_of_segment){ 
	int  amount_of_frames = size_of_segment / memory_config_struct.TAM_PAGINA;
	if (size_of_segment % memory_config_struct.TAM_PAGINA != 0)
		amount_of_frames++;
	return amount_of_frames;
}

// -------------------------- OTRA IMPLEMENTACION SWAP -------------------------- //
void create_swap(){
	void* str = malloc(memory_config_struct.TAMANIO_SWAP);
	uint32_t blocks = memory_config_struct.TAMANIO_SWAP / memory_config_struct.TAM_PAGINA;
	uint32_t value = 0;

	for(int i = 0; i < blocks; i++){
		memcpy(str + (memory_config_struct.TAM_PAGINA * i), &value, memory_config_struct.TAM_PAGINA);
	}
	
	if(access(memory_config_struct.PATH_SWAP, F_OK) == 0) {
		remove(memory_config_struct.PATH_SWAP);
		log_info(logger, "Se elimino el archivo: %s", memory_config_struct.PATH_SWAP);
	}

	log_info(logger, "Archivo a abrir: %s", memory_config_struct.PATH_SWAP);
	FILE* file_swap = fopen(memory_config_struct.PATH_SWAP, "ab");
	
	if(file_swap != NULL) {
		fwrite(str, memory_config_struct.TAM_PAGINA, blocks, file_swap);
	} else {
		log_error(logger, "Error abriendo el archivo: %s", memory_config_struct.PATH_SWAP);
	}

	fclose(file_swap);
}

t_bitarray* create_bitmap(int entries) { 
	void* pointer = malloc(entries);
	t_bitarray* bitmap = bitarray_create_with_mode(pointer, entries, MSB_FIRST);

	// Limpio todos los bits y los dejo en 0 (Cuando inicializa el bitarray tiene valores en 1, por eso hacemos esto)
	for(int i = 0; i < (entries*8); i++) { // Multiplico entries * 8 ya que viene en bytes y necesitamos limpiar CADA BIT
		bitarray_clean_bit(bitmap, i);
	}

	return bitmap;
}

int find_page_free(t_bitarray* bitmap) {
	uint32_t entries = memory_config_struct.TAMANIO_SWAP / memory_config_struct.TAM_PAGINA;
	for(int i = 0; i < entries; i++){
		if(!bitarray_test_bit(bitmap, i)){
			return i;
		}
	}

	return -1;
}

void* read_page_swap(int position, int page_number, int segment_number, int frame, int pid) {
	FILE* file_swap = fopen(memory_config_struct.PATH_SWAP, "rb+");
	//uint32_t blocks = memory_config_struct.TAMANIO_SWAP / memory_config_struct.TAM_PAGINA;
	//SWAP IN -  PID: <PID> - Marco: <MARCO> - Page In: <SEGMENTO>|<PÁGINA>

	void* value = malloc(memory_config_struct.TAM_PAGINA);
	
	if(file_swap != NULL) {
		fseek(file_swap, position * memory_config_struct.TAM_PAGINA, SEEK_SET);
		fread(value, memory_config_struct.TAM_PAGINA, 1, file_swap);
		log_info(mandatory_logger, "SWAP IN -  PID: %d - Marco: %d - Page In: %d|%d",
			pid, frame, segment_number, page_number);
	} else {
		log_error(logger, "Error abriendo el archivo: %s", memory_config_struct.PATH_SWAP);
	}

	fclose(file_swap);
	return value;
}

 
void write_page_swap(t_bitarray* bitmap, int position, void* value, int process_id, int frame_number, int segment_number, int page_number) {
	FILE* file_swap = fopen(memory_config_struct.PATH_SWAP, "r+b");
	// Escritura de Página en SWAP:
	// “SWAP OUT -  PID: <PID> - Marco: <MARCO> - Page Out: <NUMERO SEGMENTO>|<NUMERO PÁGINA>”
	//search_page_by_frame_in_memory(process_id,frame_number); nachovilla : intento fallido!

	log_info(mandatory_logger,"SWAP OUT -  PID: %d - Marco: %d - Page Out: %d|%d", process_id, frame_number, segment_number, page_number);
	if(file_swap != NULL) {
		fseek(file_swap, position * memory_config_struct.TAM_PAGINA, SEEK_SET);
		fwrite(value, memory_config_struct.TAM_PAGINA, 1, file_swap);
	} else {
		log_error(logger, "Error abriendo el archivo: %s", memory_config_struct.PATH_SWAP);
	}

	fclose(file_swap);


	//bitarray_set_bit(bitmap, position); //Cambia a 1 la posicion en el bitmap para indicar que esta ocupado
}

// Deprecated
void delete_page_swap(t_bitarray* bitmap, int position) {
	bitarray_clean_bit(bitmap, position); //Cambia a 0 la posicion en el bitmap para indicar que esta libre
	// No hace falta que lo eliminemos del swap, con indicar que esa posicion esta libre en el bitmap ya sabemos que podemos sobreescribir ahi
}

void delete_process(t_pcb* pcb) { 
	log_info(logger,"Ejecutando delete_process");
	t_list* segments = pcb->segment_table;

	for(int i = 0; list_size(segments) > i; i++) {
		t_segment_table* segment = list_get(segments,i);
		Page_table_struct* pageTable = get_page_table_by_index(segment->index_page);

		for(int j = 0; list_size(pageTable->pages_list) > j; j++) {
			Page_table_row* row = list_get(pageTable->pages_list, j);
			
			if(row->presence == true) {
				bitarray_clean_bit(bitmap_memory, row->frame_number);
			}
			bitarray_clean_bit(bitmap_swap, row->swap_position);
		}
		log_info(mandatory_logger, "PID: %d - Segmento: %d - TAMAÑO: %d paginas", pcb->process_id, i, calculate_frames_of_segment(segment->size));
	}
}

//--------------------------- MEMORIA PRINCIPAL ---------------------------//

Page_table_struct* get_page_table_by_index(int index) {
	log_info(logger, "Buscando pagina por indice");
    Page_table_struct* page_table;
    for(int i = 0; i < list_size(all_page_tables); i++) {
        page_table = list_get(all_page_tables, i);
        if(index == page_table->id) return page_table;
    }
    return NULL;
} 


int get_frame_number_RAM(t_pcb* pcb, int page_number, int segment_number) {
	log_info(logger,"get frame number RAM");
	//1 buscamos la tabla de paginas correpondiente. y buscamos la pagina correspondiente
	// nos fijamos si esta en memoria, sino page fault.

    t_list* segment_table = pcb->segment_table;

	// traemos los datos del segmento
    t_segment_table* segment_data = list_get(segment_table,segment_number);
    int id_table_page = segment_data->index_page;

	// traemos la tabla de paginas del segmento que buscabamos
    Page_table_struct* page_table = get_page_table_by_index(id_table_page);


    if(page_table == NULL) {
		log_error(logger, "No se encontro la tabla de paginas");
	}
    else {
		log_info(logger, "Se encontro la tabla de paginas: %d", page_table->id);
		// encontramos la T.Paginas , ahora buscamos la pagina en la T.paginas
		log_info(logger, "page_number: %d", page_number);


        Page_table_row* row_page = list_get(page_table->pages_list, page_number);
		log_info(logger, "row_page->frame_number: %d", row_page->frame_number);

		// si esta en 1 --> esta en ram --> devolvemos el frame
        if(row_page->presence) {
			return row_page->frame_number;
		}
    }

	return -1;
}

// ---------------------------- CLOCK ----------------------------//

// Vemos que algoritmo usar y lo usamos
int use_replacement_algorithm(int process_id, Page_table_struct* page_table, int page_number){
	if (strcmp(memory_config_struct.ALGORITMO_REEMPLAZO, "CLOCK-M") == 0){
		log_info(logger, "[CPU] Reemplazo por CLOCK-M");
		return modified_clock(process_id, page_table, page_number);
	}
	else if (strcmp(memory_config_struct.ALGORITMO_REEMPLAZO, "CLOCK") == 0){
		log_info(logger, "[CPU] Reemplazo por CLOCK");
		return simple_clock(process_id, page_table, page_number);
	}
	else{
		return -1;
	}
}

void create_clock_structure(int process_id){
	clock_struct* clock = malloc(sizeof(clock_struct));
	clock->process_id = process_id; // el identificador del clock es el mismo que el proceso.
	clock->frames_in_memory = list_create();
	clock->pointer = 0;
	set_struct_clock(process_id, clock);
	log_info(logger,"[CPU] Creada estructura clock del proceso: %d", process_id);
}

// agregamos el clock del proceso y lo metemos a un diccionario de clocks.
void set_struct_clock(int process_id, clock_struct* e_clock){
	char* key = string_itoa(process_id);
	dictionary_put(clock_structures, key, e_clock); // docccionario | id del clock | clock con los datos
	free(key);
}

// obtenemos el clock del diccionario 
clock_struct* get_struct_clock(int process_id){
	char* key = string_itoa(process_id);
	clock_struct* e_clock = dictionary_get(clock_structures, key);
	free(key);
	return e_clock;
}

void del_struct_clock(int process_id){
	char* key = string_itoa(process_id);
	dictionary_remove_and_destroy(clock_structures, key, free);
	free(key);
}

void add_page_to_clock_structure(int process_id, int frame_number, Page_table_row* page, int frame_number_in_swap){
	clock_struct* clock = get_struct_clock(process_id);
	row_struct_clock* row_to_search;

	//Si ya está ese marco en la estructura, actualizo su página.
	for (int i = 0; i < list_size(clock->frames_in_memory); i++){
		row_to_search = list_get(clock->frames_in_memory, i);
		if (frame_number == row_to_search->frame_number_in_memory){
			row_to_search->page = page;
			row_to_search->frame_number_in_swap = frame_number_in_swap;
			return;
		}
	}

	//Si no está el marco en la estructura, lo agrego al final
	row_struct_clock* new_row = malloc(sizeof(row_struct_clock));
	new_row->frame_number_in_memory = frame_number;
	new_row->page = page;
	new_row->frame_number_in_swap = frame_number_in_swap;
	list_add(clock->frames_in_memory, new_row);
}

int* get_frame_data(int frame) {
	int* res = malloc(sizeof(int));
	bool found = false;
	for(int i=0; list_size(all_page_tables) > i; i++) {
		Page_table_struct* pageTable = list_get(all_page_tables, i);
		for(int j=0; list_size(pageTable->pages_list) > j; j++){
			Page_table_row* row = list_get(pageTable->pages_list, j);
			if(row->frame_number == frame && row->presence == true) {
				res[0] = pageTable->segment_num;
				res[1] = j;
				found = true;
				continue;
			}
		}
		if(found) continue;
	}
	return res;
}

int simple_clock(int process_id, Page_table_struct* page_table, int page_number){
	// hasta que no encuentre uno no para
	clock_struct* clock = get_struct_clock(process_id);
	uint16_t clock_pointer = clock->pointer;			//Error de tipos de int?
	row_struct_clock* row;
	Page_table_row* page;
	int frame_number_in_swap;
	int frame_number;
	log_info(logger, "Antes del while");
	while(1){
		row = list_get(clock->frames_in_memory, clock_pointer);
		page = row->page;
		frame_number_in_swap = row->frame_number_in_swap;
		frame_number = row->frame_number_in_memory;
		clock_pointer = move_to_next_pointer(clock_pointer);

		if (page->use == 0){ //Encontró a la víctima
			int* res = get_frame_data(frame_number);

			log_info(mandatory_logger, "REEMPLAZO - PID: %d - Marco: %d - Page Out: %d|%d - Page In: %d|%d",
				process_id, frame_number, res[0], res[1], page_table->segment_num, page_number);

			replace_by_clock(frame_number_in_swap, page, process_id , frame_number);
			
			log_info(logger, "[CPU] Posición final puntero: %d", clock_pointer);
			clock->pointer = clock_pointer; //Guardo el puntero actualizado.
			free(res);
			return frame_number;
		}
		else{
			page->use = 0;
		}
	}
	log_info(logger, "Despues del while");

	return 0;
}

// avanza al proximo marco del proceso
int move_to_next_pointer(int clock_pointer){
    clock_pointer++;
    if(clock_pointer == memory_config_struct.MARCOS_POR_PROCESO){
		return 0;
	}
    return clock_pointer;
}

// LA PAGINA QUE FUE ELEGIDA VICTMIA SE VA A SWAP
// (posicion pagina swap,pagina victima de RAM,idproceso)
void replace_by_clock(int frame_number_in_swap, Page_table_row* page, int process_id, int frameNumber){
	// logica de swappeo de marco
	log_info(logger, "[CPU] Pagina victima elegida: %d", frame_number_in_swap);
	// si tiene el bit de modificado en 1, hay que actualizar el archivo swap
	if (page->modified == 1){
		log_info(logger, "[CPU][ACCESO A DISCO] Actualizando pagina victima en swap (bit de modificado = 1)");
		//-------------------- ACA VIENE EL MOMENTO DE USAR SWAP!!!!!!!!!!!!!!!!!!

		void* page_value = malloc(memory_config_struct.TAM_PAGINA);
		memcpy(page_value, principal_memory + (page->frame_number * memory_config_struct.TAM_PAGINA), memory_config_struct.TAM_PAGINA); // valor del frame	
		
		int* res = get_frame_data(page->frame_number);

		write_page_swap(bitmap_swap, page->swap_position, page_value, process_id, page->frame_number, res[0], res[1]); 
		free(res);
		
		//-------------------- FIN ACA VIENE EL MOMENTO DE USAR SWAP!!!!!!!!!!!!!!!
		
		usleep(memory_config_struct.RETARDO_SWAP * 1000); // tenemos el retardo por swappear un marco modificado
		page->modified = 0;
	}
	page->presence = 0;

	// este funca bien? testearlo. bitarray_clean_bit o imprimir en log para ver si aca lo limpia
	bitarray_clean_bit(bitmap_memory, page->frame_number);
	
	
}


int modified_clock(int process_id, Page_table_struct* page_table, int page_number){
	clock_struct* clock = get_struct_clock(process_id);
	int clock_pointer = clock->pointer;
	row_struct_clock* clock_page_row;
	Page_table_row* page_row;

	int frame_number_in_swap;
	int frame_number;
	// hasta que no encuentre uno no para
	while(1){
		// 1er paso del algoritmo: Buscar (0,0)
		while(1){
			clock_page_row = list_get(clock->frames_in_memory, clock_pointer);

			page_row = clock_page_row->page;
			frame_number_in_swap = clock_page_row->frame_number_in_swap;
			frame_number = clock_page_row->frame_number_in_memory;

			clock_pointer = move_to_next_pointer(clock_pointer);

			if (page_row->use == 0 && page_row->modified == 0){
				int* res = get_frame_data(frame_number);
				log_info(mandatory_logger, "REEMPLAZO - PID: %d - Marco: %d - Page Out: %d|%d - Page In: %d|%d",
					process_id, frame_number, res[0], res[1], page_table->segment_num, page_number);
				replace_by_clock(frame_number_in_swap, page_row, process_id,frame_number);
				log_info(logger, "[CPU] Posición final puntero: %d",clock_pointer);
				clock->pointer = clock_pointer; //Guardo el puntero actualizado.
				free(res);
				return frame_number;
			}
			//Condición para ir al siguiente paso
			if (clock_pointer == clock->pointer){
				break;
			}
		}
		// 2do paso del algoritmo: Buscar (0,1) y pasar Bit de uso a 0
		while(1){
			clock_page_row = list_get(clock->frames_in_memory, clock_pointer);
			page_row = clock_page_row->page;
			frame_number_in_swap = clock_page_row->frame_number_in_swap;
			frame_number = clock_page_row->frame_number_in_memory;
			clock_pointer = move_to_next_pointer(clock_pointer);

			if (page_row->use == 0 && page_row->modified == 1){
				int* res = get_frame_data(frame_number);
				log_info(mandatory_logger, "REEMPLAZO - PID: %d - Marco: %d - Page Out: %d|%d - Page In: %d|%d",
					process_id, frame_number, res[0], res[1], page_table->segment_num, page_number);
				replace_by_clock(frame_number_in_swap, page_row, process_id,frame_number);
				clock->pointer = clock_pointer; //Guardo el puntero actualizado.
				free(res);
				return frame_number;
			}
			else{
				page_row->use = 0;
			}

			//Condición para ir al siguiente paso
			if (clock_pointer == clock->pointer){
				break;
			}
		}
		// 3er paso del algoritmo, volver a empezar
	}

	return 0;
}

uint32_t read_value_in_principal_memory(int physical_address) {
    uint32_t value_of_register = 0;

    memcpy(&value_of_register, principal_memory + physical_address, sizeof(uint32_t));

    return value_of_register;
}

void save_value_in_memory(int physical_address, uint32_t register_value_mov_out, int segment_index) {
    // physical_address se calculaba como:  (frame * page_size) + offset 
    // Aca hay que guardar el valor de register_value_mov_out en la physical_address de principal_memory
	int frame = physical_address / memory_config_struct.TAM_PAGINA;	//CHEQUEAR

	Page_table_struct* page_table = get_page_table_by_index(segment_index);
	Page_table_row* row = malloc(sizeof(Page_table_row));

	// Para encontrar la pagina en la tabla de paginas debe tener el bit de presencia en 1 y el mismo numero de frame que el calculo de arriba
	int i = -1;
	do {
		i++;
		row = list_get(page_table->pages_list, i); 
	} while(((row->frame_number != frame || row->presence != true)) && i < list_size(page_table->pages_list));  


	if(row->frame_number != frame || row->presence != true) {
		log_error(logger, "Error al pasar el nro de segmento, no se realizo MOV_OUT");	
	} else {

		memcpy(principal_memory + physical_address, &register_value_mov_out, sizeof(uint32_t));
		
		// Actualizamos la tabla de paginas
		row->modified = 1;

		list_replace(page_table->pages_list, i, row);

		// aca tendriamos que loguear la row antes y despues.
		list_replace(all_page_tables, segment_index, page_table);
		// deberiamos loguear la tabla de paginas antes y despues.
	}
}


void logguear_tabla_paginas_globales(){
	int cant_tablas_paginas_en_sistema = list_size(all_page_tables);
	int cantidadPaginasTpag;
	log_info(logger,"-----------/n En el sistema Hay : %d tablas de paginas" , cant_tablas_paginas_en_sistema);
	for(int i = 0 ; i<cant_tablas_paginas_en_sistema; i++){
		Page_table_struct* unaTablaDePaginas = list_get(all_page_tables,i);
		cantidadPaginasTpag = list_size(unaTablaDePaginas->pages_list);
		log_info(logger,"La tabla de paginas con en la posicion: %d , tiene el id: %d  /n y tiene un tamaño de %d",i,unaTablaDePaginas->id ,cantidadPaginasTpag );

		loggearPageTable(unaTablaDePaginas);
	}

}






void search_page_by_frame_in_memory(int process_id, int frame_number){
	log_warning(logger,"mnachoide logger");
	// 1 tramemos las tablas de paginas que corresponden al proccess id con un filter a las alltablepages


	//ahora de esas tablas de paginas buscas de cada una la que coincide con el frame number.
	id_loco_global = process_id;

	t_list* tablasPaginasDelProcessID =  list_filter(all_page_tables,encontrandoTablas);
	
	Page_table_struct* Tpaginaproceso = list_remove(tablasPaginasDelProcessID,0);

	log_info(logger,"NACHO FALOPEADAS : /n LA TABLA DE PAGINAS A SACAR SWAP ID ES : %d",Tpaginaproceso->id);
	
}


bool encontrandoTablas(Page_table_struct* tabla1){
	if(tabla1->id == id_loco_global){
		return true;
	}
	else {return false;};
    //tv_sec nos da los segundos desde 1970
}


void update_page_use(t_pcb* pcb, int segment_number, int page_number){
	
	// encontrar tabla de paginas del segmento
	t_list* segment_table = pcb->segment_table;
	t_segment_table* segment_data = list_get(segment_table,segment_number);
	int id_table_page = segment_data->index_page;
	Page_table_struct* page_table = get_page_table_by_index(id_table_page);
	Page_table_row* page_to_update;
	if(page_table == NULL) {
		log_error(logger, "No se encontro la tabla de paginas");
	}
	else {
		// encontramos la T.Paginas , ahora buscamos la pagina en la T.paginas
		log_info(logger, "Se encontro la tabla de paginas");
		page_to_update = list_get(page_table->pages_list, page_number);
	}
	
	int frame_number = page_to_update->frame_number;
	int process_id = pcb->process_id;
		
	clock_struct* clock = get_struct_clock(process_id);
	row_struct_clock* row_to_search;
	for (int i = 0; i < list_size(clock->frames_in_memory); i++){
		row_to_search = list_get(clock->frames_in_memory, i);

		if (frame_number == row_to_search->frame_number_in_memory){

			Page_table_row* pageFounded = row_to_search->page;

			pageFounded->use = 1;

			row_to_search->page = pageFounded;

			list_replace(clock->frames_in_memory,i,row_to_search);
		}
	}
	// actualizar los datos.
}

