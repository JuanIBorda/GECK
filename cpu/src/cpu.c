#include "cpu.h"

void end_cpu_module(sig_t s){
    close_connection(socket_kernel);
    close_connection(socket_memory);
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
    signal(SIGINT, end_cpu_module);

    logger = log_create("./logs/cpu_extra.log", "CPU", false, LOG_LEVEL_INFO);
    mandatory_logger = log_create("./logs/cpu.log", "CPU", true, LOG_LEVEL_INFO);

    


    char* tipo_config = argv[1];
    t_config* config_cpu = init_config(tipo_config);
	load_config(config_cpu);

    /* ------------------------ CONEXION CON MEMORIA ------------------------ */
    socket_memory = connect_with_memory(); 
    handshake_memory(socket_memory);
    
    init_registers();
    init_semaphores();

    
    TLB = list_create();
    init_tlb();

    /* ------------------------ CONEXION CON KERNEL ------------------------ */
    pthread_t threadDispatch, threadInterrupt;

    pthread_create(&threadDispatch, NULL, (void *) process_dispatch, NULL);
    pthread_create(&threadInterrupt, NULL, (void *) process_interrupt, NULL);
    pthread_join(threadDispatch, NULL);
    pthread_join(threadInterrupt, NULL);
    return 0;
}/* ------------------------ fin main ------------------------ */

int connect_with_memory(){
     // ADVERTENCIA: Antes de continuar, tenemos que asegurarnos que el servidor esté corriendo para poder conectarnos a él
    int conection = create_connection(cpu_config_struct.IP_MEMORIA,cpu_config_struct.PUERTO_MEMORIA,logger);

   if(conection == -1){
        log_error(logger, "NO se pudo realizar la conexion la memoria");
    }else{
        log_info(logger, "Se realizo la conexion con la memoria");
    }

    return conection;
}

t_config* init_config(char* path){
    log_info(logger, "Path: %s ", path);
    t_config* new_config = config_create(path);
    
	if (new_config == NULL){
		log_error(logger, "No se encontro el archivo de configuracion del kernel\n");
		exit(2);
	}
    log_info(logger, "Path: %s ", path);
	return new_config;
}

void load_config(t_config* config_cpu){
    cpu_config_struct.ENTRADAS_TLB              = config_get_int_value(config_cpu, "ENTRADAS_TLB");
    cpu_config_struct.REEMPLAZO_TLB             = config_get_string_value(config_cpu,"REEMPLAZO_TLB");
    cpu_config_struct.RETARDO_INSTRUCCION       = config_get_string_value(config_cpu, "RETARDO_INSTRUCCION");

	cpu_config_struct.IP_MEMORIA                = config_get_string_value(config_cpu, "IP_MEMORIA");
    cpu_config_struct.IP_CPU                    = config_get_string_value(config_cpu, "IP_CPU"); 	
    cpu_config_struct.CLAVE                     = config_get_string_value(config_cpu, "CLAVE"); 		

    cpu_config_struct.PUERTO_ESCUCHA_DISPATCH   = config_get_string_value(config_cpu, "PUERTO_ESCUCHA_DISPATCH"); 
    cpu_config_struct.PUERTO_ESCUCHA_INTERRUPT  = config_get_string_value(config_cpu, "PUERTO_ESCUCHA_INTERRUPT");
    cpu_config_struct.PUERTO_MEMORIA            = config_get_string_value(config_cpu, "PUERTO_MEMORIA");
    
    log_info(logger, "Config cargada");
	log_info(logger, "Entrada TLB %d, algoritmo de reemplazo: %s", cpu_config_struct.ENTRADAS_TLB, cpu_config_struct.REEMPLAZO_TLB);
}


void handshake_memory(){
    uint32_t handshake = SEND_CONFIG;
    send(socket_memory, &handshake, sizeof(uint32_t), 0);   
    
    int cod_op = receive_operation(socket_memory);    
    if(cod_op == CONFIG_MEMORY){
        //por ahora lo recimo como lista porque en el enunciado me dice que por lo menos le llegan dos valores
        init_memory_config();
        log_info(logger, "El tamaño de las paginas es: %i", config_memory_in_cpu.TAM_PAGINA);
        log_info(logger, "La cantidad de entradas por tabla es: %i", config_memory_in_cpu.ENTRADAS_POR_TABLA);
    }else{
        log_error(logger, "NO se recibio la config de memoria");
    }
}


void init_memory_config() {
	int size = 0;
	int desp = 0;
	void * buffer = receive_buffer(&size, socket_memory);

	config_memory_in_cpu.TAM_PAGINA = read_int(buffer, &desp);
	config_memory_in_cpu.ENTRADAS_POR_TABLA = read_int(buffer, &desp);

	free(buffer);
}

/* ------------------HILO DISPATCH------------------- */

void process_dispatch() {
    log_info(logger, "Soy el proceso Dispatch :P");
    int server = server_init(cpu_config_struct.IP_CPU, cpu_config_struct.PUERTO_ESCUCHA_DISPATCH, logger);
	log_info(logger, "Servidor DISPATCH listo para recibir al cliente");

	socket_kernel = wait_client(server, logger); 
    
    log_info(logger, "Esperando a que envie mensaje/paquete");

	while (1) {
        //sem_wait(&proceso_a_ejecutar);
		int op_code = receive_operation(socket_kernel);
        log_warning(logger, "Codigo de operacion recibido de kernel: %d", op_code);
        t_pcb* pcb;

		switch (op_code) {
            case EXECUTE_PCB: 
                pcb = receive_pcb(socket_kernel, logger);
                log_info(logger, "Llego correctamente el PCB con id: %d", pcb->process_id);
                execute_process(pcb);
                break;   
            case -1:
                log_warning(logger, "El kernel se desconecto");
                end_cpu_module(1);
                pthread_exit(NULL);
                break;
            default:
                log_error(logger, "Codigo de operacion desconocido");
                exit(1);
                break;     
	    }
    }
}
/* ------------------HILO para interrupciones ------------------- */
void process_interrupt() {
    int server = server_init(cpu_config_struct.IP_CPU, cpu_config_struct.PUERTO_ESCUCHA_INTERRUPT, logger);
	int client = wait_client(server, logger);
    log_info(logger, "Servidor INTERRUPT realizo la conexion con el cliente");
	while (1) {
		int cod_op = receive_operation(client);
        log_warning(logger, "Codigo de operacion recibido en la interrupcion %d",cod_op);
		switch (cod_op) {
            case EXECUTE_INTERRUPTION:
                log_warning(logger, "El cliente debe abandonar por fin de Quantum :)");
                check_interruption = 1;
                break;
            case -1:
                log_warning(logger, "El kernel se desconecto");
                end_cpu_module(1);
                pthread_exit(NULL);
                break;
            default:
                log_warning(logger, "Operacion desconocida entre por default");
                break;
		}
	}
}


/* ------------------CICLO DE INSTRUCCION: EXECUTE------------------- */

// ... variables globales
int end_process = 0;
int input_ouput = 0;
int check_interruption = 0;

char* device = "NONE";
char* parameter = "NONE";

void execute_process(t_pcb* pcb){
    //char* value_to_copy = string_new(); // ?????

    set_registers(pcb);

    char* instruction = malloc(sizeof(char*));
    char** decoded_instruction = malloc(sizeof(char*));

    log_info(logger, "Por empezar check_interruption != 1 && end_process != 1 && input_ouput != 1 && page_fault != 1"); 
    while(check_interruption != 1 && end_process != 1 && input_ouput != 1 && page_fault != 1 && sigsegv != 1){
        //Llega el pcb y con el program counter buscas la instruccion que necesita
        instruction = string_duplicate(fetch_next_instruction_to_execute(pcb));
        decoded_instruction = decode(instruction);

        log_info(logger, "Por ejecutar la instruccion decodificada %s", decoded_instruction[0]);
        execute_instruction(decoded_instruction, pcb);

        if(page_fault != 1) {   // en caso de tener page fault no se actualiza program counter
            update_program_counter(pcb);
        }
         
        
        log_info(logger, "PROGRAM COUNTER: %d", pcb->program_counter);

        usleep(atoi(cpu_config_struct.RETARDO_INSTRUCCION) * 1000);
        log_info(logger, "DESPERTE RETARDADO INSTS");
    } //si salis del while es porque te llego una interrupcion o termino el proceso o entrada y salida
    
    log_info(logger, "SALI DEL WHILE DE EJECUCION");


    save_context_pcb(pcb); // ACA GUARDAMOS EL CONTEXTO

    if(end_process) {
        end_process = 0; // IMPORTANTE: Apagar el flag para que no rompa el proximo proceso que llegue
        check_interruption = 0;
        send_pcb_package(socket_kernel, pcb, END_PROCESS);
        log_info(logger, "Enviamos paquete a dispatch: FIN PROCESO");
    } 
    else if(input_ouput) {
        input_ouput = 0;
        check_interruption = 0;
        log_info(logger, "Device: %s, Parameter: %s", device, parameter);
        send_pcb_io_package(socket_kernel, pcb, device, parameter, REQUEST);
    }
    else if(page_fault) {
        page_fault = 0;
        check_interruption = 0;
        log_info(logger, "OCURRIO UN PAGE FAULT, ENVIANDO A KERNEL PARA QUE SOLUCIONE");
        
        // subirle pcb , 
        t_package* package = create_package(PAGE_FAULT); // le pide a kernel q solucione page fault
        add_pcb_to_package(package, pcb);
        add_int_to_package(package, numeroSegmentoGlobalPageFault);
        add_int_to_package(package, numeroPaginaGlobalPageFault);
        send_package(package, socket_kernel);
        delete_package(package);
        //send_pcb_package(socket_kernel, pcb, REQUEST_PAGE_FAULT); //Este codigo de operacion?
    }
    else if(sigsegv == 1){
        sigsegv = 0;
        check_interruption = 0;
        log_info(logger, "Error: Segmentation Fault (SEG_FAULT), enviando para terminar proceso");
        send_pcb_package(socket_kernel, pcb, SEG_FAULT); //Este codigo de operacion?
    }
    else if(check_interruption) {
        check_interruption = 0;
        log_info(logger, "Entro por check interrupt");
        send_pcb_package(socket_kernel, pcb, EXECUTE_INTERRUPTION); //Este codigo de operacion?
    }
}

void save_context_pcb(t_pcb* pcb){
    pcb->cpu_registers[AX] = registers[AX];
    pcb->cpu_registers[BX] = registers[BX];
    pcb->cpu_registers[CX] = registers[CX];
    pcb->cpu_registers[DX] = registers[DX];
}

void set_registers(t_pcb* pcb) {
    registers[AX] = pcb->cpu_registers[AX];
    registers[BX] = pcb->cpu_registers[BX];
    registers[CX] = pcb->cpu_registers[CX];
    registers[DX] = pcb->cpu_registers[DX];
}

void init_registers() {
    registers[AX] = 0;
    registers[BX] = 0;
    registers[CX] = 0;
    registers[DX] = 0;
}

void init_semaphores() {
}

// Fetch La primera etapa del ciclo consiste en buscar la próxima instrucción a ejecutar.
char* fetch_next_instruction_to_execute(t_pcb* pcb){
    return pcb->instructions[pcb->program_counter];
}

//Decode  consiste en interpretar qué inst va a ejecutar y si requiere de un acceso a memoria o no.
char** decode(char* linea){ // separarSegunEspacios
    char** instruction = string_split(linea, " "); 

    if(instruction[0] == NULL){
        log_info(logger, "linea vacia!");
    }
    // no va el free aca, linea se libera en el scope donde se declara
    return instruction; 
}

void update_program_counter(t_pcb* pcb){
    pcb->program_counter += 1;
}

void execute_instruction(char** instruction, t_pcb* pcb){
    Logical_address_parameters* logical_address_params;

    switch(keyfromstring(instruction[0])){
        case I_SET: //ESTA INSTRUCCION YA ANDA
            // SET (Registro, Valor)
            log_info(logger, "Por ejecutar instruccion SET");
            log_info(mandatory_logger, "PID: %d - Ejecutando: %s - %s - %s", pcb->process_id, instruction[0], instruction[1], instruction[2]);

            add_value_to_register(instruction[1], instruction[2]);
            break;
        case I_ADD:
            //ADD (Registro Destino, Registro Origen)
            log_info(logger, "Por ejecutar instruccion ADD");
            log_info(mandatory_logger, "PID: %d - Ejecutando: %s - %s - %s", pcb->process_id, instruction[0], instruction[1], instruction[2]);

            add_two_registers(instruction[1], instruction[2]);
            break;
        case I_IO:
            // I/O (Dispositivo, Registro / Unidades de trabajo)
            log_info(logger, "Por ejecutar instruccion I/O");
            log_info(mandatory_logger, "PID: %d - Ejecutando: %s - %s - %s", pcb->process_id, instruction[0], instruction[1], instruction[2]);

            device = instruction[1];
            parameter = instruction[2];
            
            log_info(logger, "%s",device);
            input_ouput = 1;
            break;
        case I_MOV_IN: // (Registro, Dirección Lógica): mov 
            //MOV_IN  Lee el valor de memoria del segmento de Datos correspondiente a la Dirección Lógica y lo almacena en el Registro. // es el MUY parecido al  READ del cuatri pasado
            log_info(logger, "Instruccion MOV_IN ejecutada");
            log_info(mandatory_logger, "PID: %d - Ejecutando: %s - %s - %s", pcb->process_id, instruction[0], instruction[1], instruction[2]);

            char* register_mov_in = instruction[1];
            int logical_address_mov_in = atoi(instruction[2]);
            // 1) armas direccion fisica
            
            // max Seg size , n°seg , seg offset , n°pag pag offset
            logical_address_params = translate_logical_address(logical_address_mov_in, pcb); // PASA A DIRECCION FISICA, NO LOGICA

            //------------SI NO TENEMOS SEG FAULT EJECUTAMOS LO DEMAS ------------ //
            if(sigsegv != 1) {
                int physical_address = build_physical_address(logical_address_params , pcb);
                
                if(physical_address == -1){ // tenemos un PAGE FAULT
                    log_info(mandatory_logger, "Page Fault PID: %d - Segmento: %d - Pagina: %d",
                        pcb->process_id, logical_address_params->segment_number, logical_address_params->page_num);
                }  
                else { // ta todo bien!!
                    // 2) buscas valor en memoria: 
                    uint32_t value = fetch_value_in_memory(physical_address, pcb, logical_address_params);
                    
                    
                    // 3) guardamos valor en registro
                    store_value_in_register(register_mov_in, value); // ya esta armada, no testeada
                    log_info(mandatory_logger, "PID: %d - Acción: LEER - Segmento: %d - Pagina: %d - Dirección Fisica: %d",
                        pcb->process_id, logical_address_params->segment_number, logical_address_params->page_num, physical_address);
                    log_info(logger,"YA GUARDE VALOR EN REGISTRO!!");
                }
            }
            break;
        case I_MOV_OUT: // (Dirección Lógica, Registro)
            //Lee el valor del Registro y lo escribe en la Dir física de memoria del segmento de Datos obtenida a partir de la Dirección Lógica.
            // es el MUY parecido al  WRITE del cuatri pasado
            log_info(logger, "Ejecutando Instruccion MOV_OUT ");
            int logical_address_mov_out = atoi(instruction[1]);
            char* register_mov_out = instruction[2]; // el case  no deja q se llame register_mov porq repite el de arriba
            log_info(mandatory_logger, "PID: %d - Ejecutando: %s - %s - %s", pcb->process_id, instruction[0], instruction[1], instruction[2]);


            // 1) traducir direccion logica
            logical_address_params = translate_logical_address(logical_address_mov_out, pcb); 
            // PASA A DIRECCION FISICA, NO LOGICA

            // 2) armar direccion fisica 
            if(sigsegv != 1) {
                int physical_address = build_physical_address(logical_address_params, pcb);

                if(physical_address == -1){ // tenemos un PAGE FAULT
                    log_info(mandatory_logger, "Page Fault PID: %d - Segmento: %d - Pagina: %d",
                        pcb->process_id, logical_address_params->segment_number, logical_address_params->page_num);
                }  
                else { 
                    log_info(logger, "Recibimos una physical address valida!");
                    uint32_t register_value_mov_out = findValueOfRegister(register_mov_out);
                    t_segment_table* segment = list_get(pcb->segment_table, logical_address_params->segment_number);
                    int segment_index = segment->index_page;    //segmente_index es Indice de tabla de pagina

                    write_value(physical_address, register_value_mov_out, segment_index, pcb->process_id);   
                    int code_op = receive_operation(socket_memory);
                    int size = 0;
                    int desp = 0;
	                void * buffer = receive_buffer(&size, socket_memory);
                        
                    log_info(logger, "Leo buffer: %d", read_int(buffer, &desp));
                    log_info(mandatory_logger, "PID: %d - Acción: ESCRIBIR - Segmento: %d - Pagina: %d - Dirección Fisica: %d",
                        pcb->process_id, logical_address_params->segment_number, logical_address_params->page_num, physical_address);
                }  
            }
            break;
        case I_EXIT:
            //EXIT: Esta instrucción representa la syscall de finalización del proceso.
            //Se deberá devolver el PCB actualizado al Kernel para su finalización.
            log_info(logger, "Instruccion EXIT ejecutada");
            log_info(mandatory_logger, "PID: %d - Ejecutando: %s", pcb->process_id, instruction[0]);

            end_process = 1;
            break;
         default:
            log_info(logger, "No ejecute nada nadita nada nadita :(");
            break;
    }
 
}


void update_pcb_status(t_pcb* pcb, pcb_status status){
    pcb->status = status;
}
//------------------------ FUNCIONES PARA EJECUTAR LAS INSTRUCCIONES ------------------------//


//-------------------------- PARA LA INSTRUCCION SET --------------------------//
void add_value_to_register(char* registerToModify, char* valueToAdd){ // no borrar, son dos funciones distintas
    //convertir el valor a agregar a un tipo de dato int
    uint32_t value = atoi(valueToAdd);
    
    log_info(logger, "valor a sumarle al registro %d",value);
    if (strcmp(registerToModify, "AX") == 0) {
        registers[AX] += value;
    }
    else if (strcmp(registerToModify, "BX") == 0) {
        registers[BX] += value;
    }
    else if (strcmp(registerToModify, "CX") == 0) {
        registers[CX] += value;
    }
    else if (strcmp(registerToModify, "DX") == 0) {
        registers[DX] += value;
    }
}

//-------------------------- PARA LA INSTRUCCION ADD --------------------------//
void add_two_registers(char* registerToModify, char* registroParaSumarleAlOtroRegistro){
    char* value = string_itoa(findValueOfRegister(registroParaSumarleAlOtroRegistro));
    add_value_to_register(registerToModify,value);
}

uint32_t findValueOfRegister(char* register_to_find_value){
    if (strcmp(register_to_find_value, "AX") == 0) return registers[AX];
    else if (strcmp(register_to_find_value, "BX") == 0) return registers[BX];
    else if (strcmp(register_to_find_value, "CX") == 0) return registers[CX];
    else if (strcmp(register_to_find_value, "DX") == 0) return registers[DX];
    else return 0;
}

/* ------------------MMU------------------- */

//  armar_direccion_fisica
int build_physical_address(Logical_address_parameters* logical_address_params, t_pcb* pcb){    
    int frame = present_in_tlb(logical_address_params->page_num, logical_address_params->segment_number, pcb->process_id);
    
    //----------------- CASO PAGINA NO ESTA EN TLB -----------------//
    if (frame == -1 ){ // caso en el cual no tenemos referencia en tlb
        
        log_info(logger, "TLB MISS");
        log_info(mandatory_logger, "PID: %d - TLB MISS - Segmento: %d - Pagina: %d", pcb->process_id, logical_address_params->segment_number,
            logical_address_params->page_num);
            
        //Segunda conexion: buscar el marco  
        frame = find_frame(logical_address_params,pcb); // Cuando es un page fault -> Devuelve -1 Y ACTUALIZA LA VARIABLE GLOBAL DE page_fault = 1
        //---------------- FRAME EN MEMORIA -> SE ACTUALIZA REFERENCIA TLB ----------------//
        if (frame != -1){
            log_info(logger, "Por actualizar la tlb");
            add_to_tlb(pcb->process_id, logical_address_params->segment_number, logical_address_params->page_num, frame);
            loggear_TLB(); // vemos como nos quedo la TLB :D
            return frame * config_memory_in_cpu.TAM_PAGINA + logical_address_params->page_offset;
        }
    }
    //------------------ CASO OCURRIO PAGE FAULT ------------------//
    if (frame == -1) { 
        return -1;  
    }

    //---------------------- CASO TLB HIT ----------------------//
    int physical_address = calculate_physical_address(frame,logical_address_params->page_offset,config_memory_in_cpu.TAM_PAGINA);
    log_info(mandatory_logger, "PID: %d - TLB HIT - Segmento: %d - Pagina: %d", pcb->process_id, logical_address_params->segment_number,
        logical_address_params->page_num);

    return physical_address; 
} 

int calculate_physical_address(int frame, int offset, int page_size){
    log_info(logger, "CALCULANDO calculate_physical_address");
    return (frame * page_size) + offset;
}

Logical_address_parameters* translate_logical_address(int logical_address, t_pcb* pcb) {

    Logical_address_parameters* parameters = malloc(sizeof(Logical_address_parameters)); // falta liberar memoria?

    //hay error con el flor?
    parameters -> max_segment_size = config_memory_in_cpu.ENTRADAS_POR_TABLA * config_memory_in_cpu.TAM_PAGINA;
    parameters -> segment_number = (int) floor(logical_address / parameters->max_segment_size );
    parameters -> segment_offset = logical_address % parameters->max_segment_size;
    parameters -> page_num = (int) floor(parameters-> segment_offset  / config_memory_in_cpu.TAM_PAGINA);
    parameters -> page_offset = parameters-> segment_offset % config_memory_in_cpu.TAM_PAGINA;
    
    t_list* segment_table_pcb = pcb->segment_table;
    
    t_segment_table* segment = list_get(segment_table_pcb, parameters -> segment_number);

    if(parameters -> segment_offset >= segment->size ){
        // actualizar variable del sig
        sigsegv = 1;
    }
    
    return parameters;
    // falta un free?
}


// NO PODEMOS HACER ESTA FUNCION HASTA QUE TENGAMOS MEMORIA
int find_frame(Logical_address_parameters* logical_address_params, t_pcb* pcb){
    log_info(logger, "CPU BUSCA NUMERO DE FRAME EN MEMORIA");
    t_package* frame_package = create_package(FRAME_REQUEST);
    // necesito la tabla de paginas??? no me acuerdo de donde salia
    // nose si necesito mandar todos los calculos que nos dan en enunciado
    add_pcb_to_package(frame_package, pcb); 
    add_int_to_package(frame_package, logical_address_params->page_num);  
    add_int_to_package(frame_package, logical_address_params->segment_number);  
    send_package(frame_package, socket_memory); // que socket va aca?
    delete_package(frame_package);

    //Recibir marco de la memoria o PAGE FAULT
    int code_op = receive_operation(socket_memory);    
    log_warning(logger, "CODIGO DE OPERACION RECIBIDO DE MEMORIA AL BUSCAR UN FRAME: %d", code_op);
    int frame = -1, size = 0, desp = 0;
    
    void* buffer = receive_buffer(&size, socket_memory); // hay q devolver -1 desde memoria
    frame = read_int(buffer, &desp);

    if(code_op == FRAME_RESPONSE) {
        log_info(logger, "Se recibio el marco: %d", frame);

        // ACA ACTUALIZAMOS LA TLB
        // Datos para actualizar la TLB
        int process_id = pcb->process_id;
        int segment = logical_address_params->segment_number; 
        int page = logical_address_params->page_num;
        // grame es el q recibimos
        // actualizar_tlb()
        log_info(logger, "Le paso valores para que la TLB se actualice /n  Proceso: %d | segmento: %d | pagina %d | frameEnmemoria %d",process_id,segment,page,frame);
        
    } 
    else if(code_op == PAGE_FAULT) {
        log_info(logger, "Page Fault PID: %d - Segmento: %d - Pagina: %d", pcb->process_id, logical_address_params->segment_number, logical_address_params->page_num);
        page_fault = 1;
        numeroSegmentoGlobalPageFault = logical_address_params->segment_number;
        numeroPaginaGlobalPageFault = logical_address_params->page_num;
    }
    else {
        log_error(logger, "OH OH NOS ROMPIMOS! CULPA DE ALGUN NACHO!: %d", code_op);
        exit(1);
    }
    
    return frame;
}

void write_value(int physical_address, uint32_t register_value_mov_out, int segment_index, int pid){
    t_package* package = create_package(MOV_OUT);
    add_int_to_package(package, physical_address);
    add_uint32_to_package(package, register_value_mov_out);
    add_int_to_package(package, segment_index);
    add_int_to_package(package, pid);
    send_package(package, socket_memory);
}

uint32_t fetch_value_in_memory(int physical_adress, t_pcb* pcb, Logical_address_parameters* logical_params){

    t_package* package = create_package(MOV_IN); 
    add_int_to_package(package, physical_adress);
    add_pcb_to_package(package,pcb);
    add_int_to_package(package, logical_params->segment_number);
    add_int_to_package(package, logical_params->page_num);
    send_package(package, socket_memory);
    delete_package(package);
    log_info(logger, "MOV IN enviado");    

    int code_op = receive_operation(socket_memory);
    log_info(logger, "CODIGO OPERACION RECIBIDO EN CPU: %d", code_op);
    

    uint32_t value_received;    
    int size = 0, desp = 0;

    if(code_op == MOV_IN_OK) {  
        log_info(logger,"ENTRE CARAJO");
        void* buffer = receive_buffer(&size, socket_memory);
        value_received = read_uint32(buffer, &desp); // OJO q este no mide lo mismo q un int mide 4 bytes!
        log_info(logger, "EL VALOR DEL REGISTRO RECIBIDO ES: %d", value_received);
    } else {
        log_error(logger, "CODIGO DE OPERACION INVALIDO como nacho");
    }

    return value_received;
}

// funcion usada para cuando mov in tiene que sobreescribir un valor a un registro
void store_value_in_register(char* register_mov_in, uint32_t value){

    log_info(logger, "el registro %s quedara el valor: %d",register_mov_in ,value);

    if (strcmp(register_mov_in, "AX") == 0) {
        registers[AX] = value;
    }
    else if (strcmp(register_mov_in, "BX") == 0) {
        registers[BX] = value;
    }
    else if (strcmp(register_mov_in, "CX") == 0) {
        registers[CX] = value;
    }
    else if (strcmp(register_mov_in, "DX") == 0) {
        registers[DX] = value;
    }
}




/* ------------------ TLB ------------------- */


void init_tlb() {
    for(int i=0;i < cpu_config_struct.ENTRADAS_TLB;i++){
		add_row_to_TLB(-1,-1,-1,-1); //(valores negativos primera vez para diferenciar)
	}
}

void add_row_to_TLB(int process_id, int segment, int page_number, int frame){
    
    row_tlb* row_to_add = malloc(sizeof(row_tlb));
    struct timeval time;
    gettimeofday(&time, NULL);
    
    row_to_add->process_id = process_id;
    row_to_add->segment = segment;
    row_to_add->page = page_number;
    row_to_add->frame = frame;

    row_to_add->init_time = time; 
    row_to_add->last_reference = time; //cuando se carga por primera vez, el init_time sera igual al time de last_reference

    list_add(TLB, row_to_add);

    log_info(logger, "la funcion add_row_to_TLB, termino ACA");

    //free(row_to_add);//esta bien este free?    
}

void destroy_row_TLB(row_tlb* row_to_destroy) {
    free(row_to_destroy);
}

// para agregar una nueva entrada a la tlb haciendo todos los controles necesarios :D
void add_to_tlb(int process_id, int segment, int page_number, int frame){
    if(cpu_config_struct.ENTRADAS_TLB == 0){
        log_info(logger, "La cantidad de entradas de la TLB es 0");
        return;
    }
    else if(list_size(TLB) < cpu_config_struct.ENTRADAS_TLB){ // no hace falta semaforo porque no es multihilo
        add_row_to_TLB(process_id, segment, page_number, frame); 
    }
    else if(list_size(TLB) == cpu_config_struct.ENTRADAS_TLB ){ // caso donde entra alg reemplazo
        log_info(logger, "Reemplazo_tlb tiene el algoritmo de: %s", cpu_config_struct.REEMPLAZO_TLB);

        if(strcmp(cpu_config_struct.REEMPLAZO_TLB, "FIFO") == 0){   
            list_remove_and_destroy_element(TLB, 0, (void*) destroy_row_TLB); 
        }
        else if(strcmp(cpu_config_struct.REEMPLAZO_TLB, "LRU") == 0){ 
            log_info(logger,"EL TAMAÑO DE LA TLB %d previo al list_sort", list_size(TLB));

            list_sort(TLB, compare_last_reference); // pasar todo a ingles
            log_info(logger,"EL TAMAÑO DE LA TLB %d",list_size(TLB));
            list_remove(TLB, 0);
        }

        // actualizo la TLB.
        log_info(logger, "Por agregar a tlb");
        add_row_to_TLB(process_id, segment, page_number, frame); // para fifo o LRU
        log_info(logger, "Agregue a tlb y todos contentos :D");
    }
    else { // caso de error
        log_info(logger, "La lista TLB es mayor a las entradas de la tlb");
    }
}


// despues describo como funca esto -> POV nunca lo hizo
bool compare_last_reference(row_tlb* first_row, row_tlb* second_row){
    //tv_sec nos da los segundos desde 1970
    if(first_row->last_reference.tv_sec == second_row->last_reference.tv_sec){
        return (first_row->last_reference.tv_usec) < (second_row->last_reference.tv_usec);
    }
    return (first_row->last_reference.tv_sec) < (second_row->last_reference.tv_sec);
}



int present_in_tlb(int page_number, int segment_number, int pcb_id){

	for(int i=0;i< cpu_config_struct.ENTRADAS_TLB;i++){
		row_tlb* tlb_aux = list_get(TLB, i);

        /*log_info(logger, "pid: %d", tlb_aux->process_id);
        log_info(logger, "Numero segmento: %d", tlb_aux->segment);
		log_info(logger, "Numero pagina: %d", tlb_aux->page);
		log_info(logger, "Numero marco: %d", tlb_aux->frame);*/
		log_info(logger,"Ciclo cpu: %d",tlb_aux->last_reference);

		if( (tlb_aux->page == page_number) && (tlb_aux->segment == segment_number) && (tlb_aux->process_id == pcb_id)){

            // no se si es la manera correcta de guardar el time
            struct timeval time1;
            gettimeofday(&time1, NULL);

			tlb_aux->last_reference = time1;
			log_info(logger,"TLB HIT (pagina %d , segmento %d , id proceso %d)", page_number ,segment_number,pcb_id);
			return tlb_aux->frame; // RECORDAR QUE FRAME = MARCO
		}
	}

	return -1;
}

void loggear_TLB(){
    log_info(logger,"logueando TLB ---- /n numero proceso | segmento | pagina | marco | init_time | last_reference \n");
    log_info(mandatory_logger, "TLB:");
    for(int i=0;i < list_size(TLB); i++){
        row_tlb* row = list_get(TLB, i);
        log_info(mandatory_logger, "%d|PID:%d|SEGMENTO:%d|PAGINA:%d|MARCO:%d",
            i, row->process_id, row->segment, row->page, row->frame);
        /*log_info(logger, " %d     | %d     | %d     | %d     | %ld       |%ld     %ld \n", 
                            row->process_id,
                            row->segment,
                            row->page, 
                            row->frame, 
                            row->init_time.tv_sec, 
                            row->last_reference.tv_sec, 
                            row->last_reference.tv_usec);*/
    }
}



/* ------------------ END TLB ------------------- */



