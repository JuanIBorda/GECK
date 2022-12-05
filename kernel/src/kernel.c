#include "kernel.h"

void end_kernel_module(sig_t s){
    close_connection(connection_cpu_dispatch);
    close_connection(connection_cpu_interrupt);
    close_connection(memory_connection);
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
    signal(SIGINT, end_kernel_module);


    logger = log_create("./logs/kernel_extra.log", "KERNEL", false, LOG_LEVEL_INFO);
    mandatory_logger = log_create("./logs/kernel.log", "KERNEL", true, LOG_LEVEL_INFO);

    t_config* config = init_config(argv[1]);
    load_kernel_config(config);
    log_warning(logger, "Vamos a usar el algoritmo %s", config_kernel->ALGORITMO_PLANIFICACION);

    init_global_lists(); // es para manejar los estados del proceso 
    init_device_config_dictionary();    

    /*---------------------- PRENDEMOS LOS SEMAFOROS ----------------------*/
    
    init_semaphores();

    /*---------------------- Conexion con MEMORIA ----------------------*/

    memory_connection = connect_with_memory();

    
    /*---------------------- Conexion con CPU ----------------------*/

	connection_cpu_dispatch = connect_with_cpu_dispatch();
    connection_cpu_interrupt = connect_with_cpu_interrupt();

    /*---------------------- CONEXIONES CON LAS CONSOLAS ----------------------*/
    int kernel_socket = server_init(config_kernel->IP_KERNEL, config_kernel->PUERTO_ESCUCHA, logger);

    //-------- Iniciando planificadores (corto y largo plazo)------------ 
    init_scheduling();

    while(1) {
        log_info(logger, "Esperando un cliente nuevo de la consola...");
        int client_socket = wait_client(kernel_socket, logger); 
        log_info(logger, "Entro una consola con este socket: %d", client_socket);
        pthread_t attend_console;
                pthread_create(&attend_console, NULL, (void*) receive_console, (void*) client_socket);
                pthread_detach(attend_console);
        // esperamos un cliente (consola)
    }

    //en algun momento liberar memoria
    return 0;
}    /*---------------------- FIN MAIN ----------------------*/


t_config* init_config(char* path){
    log_info(logger, "Path: %s ", path);
    t_config* new_config = config_create(path);

	if (new_config == NULL){
		log_error(logger, "No se encontro el archivo de configuracion del kernel\n");
		exit(2);
	}

	return new_config;
}



void load_kernel_config(t_config* config){
    config_kernel = malloc(sizeof(t_kernel_config));
    config_kernel->IP_KERNEL =                   config_get_string_value(config, "IP_KERNEL");
	config_kernel->IP_MEMORIA =                  config_get_string_value(config, "IP_MEMORIA");
	config_kernel->PUERTO_MEMORIA =              config_get_string_value(config, "PUERTO_MEMORIA");
    config_kernel->IP_CPU =                      config_get_string_value(config, "IP_CPU"); 		
    config_kernel->PUERTO_CPU_DISPATCH =         config_get_string_value(config, "PUERTO_CPU_DISPATCH");
    config_kernel->PUERTO_CPU_INTERRUPT =        config_get_string_value(config, "PUERTO_CPU_INTERRUPT"); 
    config_kernel->PUERTO_ESCUCHA =              config_get_string_value(config, "PUERTO_ESCUCHA");
    config_kernel->ALGORITMO_PLANIFICACION =     config_get_string_value(config, "ALGORITMO_PLANIFICACION");
    config_kernel->GRADO_MAX_MULTIPROGRAMACION = config_get_int_value(config, "GRADO_MAX_MULTIPROGRAMACION");
    config_kernel->DISPOSITIVOS_IO =             config_get_array_value(config, "DISPOSITIVOS_IO");
    config_kernel->TIEMPOS_IO =                  config_get_array_value(config, "TIEMPOS_IO");  
    config_kernel->QUANTUM_RR =                  config_get_string_value(config, "QUANTUM_RR");
   
    log_info(logger, "Config cargada en  'kernel_config' ");    
}
 

    
void receive_console(int client_socket){
    while(1){
        int op_code = receive_operation(client_socket);
        log_warning(logger, "Codigo de operacion recibido desde consola: %d", op_code);
        t_pcb* pcb;
        switch(op_code){
            case INIT_PROCESS:
                log_info(logger, "Entro una consola, enviando paquete a iniciar");
                pcb = pcb_init(client_socket);

                //loggear_pcb(pcb,logger);
                log_info(mandatory_logger, "Se crea el proceso %d en NEW", pcb->process_id);
                // pcb_init_in_memory();

                schedule_next_to(pcb, NEW, list_NEW, m_list_NEW);
                sem_wait(&sem_schedule_ready);
                schedule_next_to_ready();
                sem_post(&sem_schedule_ready);
                break;
            case IO_SCREEN_RESPONSE:
                log_info(logger, "Llego respuesta de la pantalla de consola");
                pcb = receive_pcb(client_socket, logger);
                //loggear_pcb(pcb, logger);
                if (string_equals_ignore_case(config_kernel->ALGORITMO_PLANIFICACION, "FEEDBACK")) {
                    schedule_next_to(pcb, READY, list_READY_RR, m_list_READY_RR);
                    log_info(mandatory_logger, "PID: %d - Estado Anterior: BLOCKED - Estado Actual: READY", pcb->process_id);
                    loggear_cola_ready(config_kernel->ALGORITMO_PLANIFICACION);
                }
                else {
                    // si el algoritmo es FIFO o RR
                    schedule_next_to(pcb, READY, list_READY, m_list_READY);
                    log_info(mandatory_logger,"PID: %d - Estado Anterior: BLOCKED - Estado Actual: READY",pcb->process_id);
                    loggear_cola_ready(config_kernel->ALGORITMO_PLANIFICACION);
                }
                sem_post(&sem_process_in_ready);
                break;
            case IO_KEYBOARD_RESPONSE:
                log_info(logger, "Llego respuesta del teclado de consola");
                pcb = receive_pcb(client_socket, logger);
                //loggear_pcb(pcb, logger);   
                if (string_equals_ignore_case(config_kernel->ALGORITMO_PLANIFICACION, "FEEDBACK")){
                    schedule_next_to(pcb, READY, list_READY_RR, m_list_READY_RR); //Se puede agregar el semaforo de abajo en esta funcion capaz para simplificar a futuro
                    log_info(mandatory_logger, "PID: %d - Estado Anterior: BLOCKED - Estado Actual: READY", pcb->process_id);
                    loggear_cola_ready(config_kernel->ALGORITMO_PLANIFICACION);
                    sem_post(&sem_process_in_ready);
                }
                else {  
                    // si el algoritmo es FIFO o RR
                    schedule_next_to(pcb, READY, list_READY, m_list_READY); //Se puede agregar el semaforo de abajo en esta funcion capaz para simplificar a futuro
                    log_info(mandatory_logger,"PID: %d - Estado Anterior: BLOCKED - Estado Actual: READY",pcb->process_id);
                    loggear_cola_ready(config_kernel->ALGORITMO_PLANIFICACION);
                    sem_post(&sem_process_in_ready);
                }
                break;
            case -1:
                log_warning(logger, "El client_socket %d se deconecto", client_socket);
                close_connection(client_socket);
                pthread_exit(NULL);
                break;
            default:
                log_warning(logger, "El kernel desconoce el operation code recibido");
                break;
        }
    }
}

t_pcb* pcb_init(int socket){
    int size = 0;
    void* buffer;
    int desp = 0; 
    buffer = receive_buffer(&size, socket);
    char** instructions = read_string_array(buffer, &desp);
    char** segments = read_string_array(buffer, &desp);
    
    sem_wait(&sem_pcb_create);

    t_pcb* pcb = pcb_create(instructions, socket, pid_counter, segments);
    
    pid_counter++;
    log_info(logger, "valor id: %d", pcb->process_id);
    
    sem_post(&sem_pcb_create);
    
    return pcb;
}


int connect_with_memory(){
    // ADVERTENCIA: Antes de continuar, tenemos que asegurarnos que el servidor esté corriendo para poder conectarnos a él
    int conexion = create_connection(config_kernel->IP_MEMORIA, config_kernel->PUERTO_MEMORIA, logger);
    if(conexion == -1) {
        log_error(logger, "NO se pudo realizar la conexion la memoria");
    } else {
        log_info(logger, "Pudimos realizar la conexion con la memoria");
    }

    return conexion;
}

// CPU
int connect_with_cpu_dispatch(){
    log_info(logger, "Iniciando conectar cpu dispatch");
    // ADVERTENCIA: Antes de continuar, tenemos que asegurarnos que el servidor esté corriendo para poder conectarnos a él
    int connection = create_connection(config_kernel->IP_CPU, config_kernel->PUERTO_CPU_DISPATCH, logger);
    if(connection == -1){
        log_error(logger, "NO se pudo realizar la conexion con CPU DISPATCH");
    }else{
        log_info(logger, "Se realizo la conexion con CPU DISPATCH");
    }
    return connection;
}

int connect_with_cpu_interrupt(){
    // ADVERTENCIA: Antes de continuar, tenemos que asegurarnos que el servidor esté corriendo para poder conectarnos a él
    log_info(logger, "Iniciando conectar cpu interrupt");
    int connection = create_connection(config_kernel->IP_CPU, config_kernel->PUERTO_CPU_INTERRUPT, logger);
    if(connection == -1){
        log_error(logger, "NO se pudo realizar la conexion con CPU INTERRUPT");
    }else{
        log_info(logger, "Pudimos realizar la conexion con CPU INTERRUPT");
    }
    return connection;
}



void init_semaphores() {
    sem_init(&sem_grado_multiprogamacion, 0, config_kernel->GRADO_MAX_MULTIPROGRAMACION);
	sem_init(&sem_process_in_ready,0, 0);
    sem_init(&sem_io_arrived, 0, 0);
    sem_init(&sem_cpu_free_to_execute, 0, 1);
    sem_init(&sem_pcb_create, 0, 1);
    sem_init(&sem_beggin_quantum,0,0);
    sem_init(&sem_init_process_memory,0,0);
    sem_init(&sem_schedule_ready,0,1);
  

    if(string_equals_ignore_case(config_kernel->ALGORITMO_PLANIFICACION, "RR") || string_equals_ignore_case(config_kernel->ALGORITMO_PLANIFICACION, "FEEDBACK") ){
        // es para avisar que hay q interrumpir en caso de q se usa round robin
        sem_init(&sem_interruption_quantum, 0, 0);
        sem_init(&sem_interruption_quantum_finish, 0, 1);
        log_info(logger,"-------------------inicialice en semaforo de interrupcion por quantum");
    }
    
}

void init_scheduling() {
    log_info(logger, "Inicializando hilos...");
    pthread_create(&short_term_scheduler, NULL, (void*) schedule_next_to_running, NULL);
    pthread_detach(short_term_scheduler);  

    pthread_create(&thread_memory, NULL, (void*) manage_memory, NULL);
    pthread_detach(thread_memory);

    pthread_create(&thread_Dispatch, NULL, (void*) manage_dispatch, (void*) connection_cpu_dispatch);
    pthread_detach(thread_Dispatch);

    pthread_create(&thread_Interrupt, NULL, (void*) manage_interrupt, (void*) connection_cpu_interrupt);
    pthread_detach(thread_Interrupt);

    if(strcmp(config_kernel->ALGORITMO_PLANIFICACION, "RR") == 0 || strcmp(config_kernel->ALGORITMO_PLANIFICACION, "FEEDBACK") == 0) {
        log_info(logger, "Prendiendo el contador de Quantum");
        turn_on_switcher_clock_RR();
    }

}

void schedule_next_to(t_pcb* pcb, pcb_status state, t_list* list, pthread_mutex_t mutex) {
    change_pcb_state_to(pcb, state);
    add_to_list_with_sems(pcb, list, mutex);
    log_info(logger, "El pcb entro en la cola de %s", get_state_name(state));
}

void add_to_list_with_sems(t_pcb* pcb_to_add, t_list* list, pthread_mutex_t m_sem){
    pthread_mutex_lock(&m_sem);
    list_add(list, pcb_to_add);
    pthread_mutex_unlock(&m_sem);
}

void schedule_next_to_new(t_pcb* pcb) {
    change_pcb_state_to(pcb, NEW);
    add_to_list_with_sems(pcb, list_NEW, m_list_NEW);
    log_info(logger, "El pcb entro en la cola de NEW!");
}

// --------------------------------------------------------------------------------------------------------- //
// TODO                
void schedule_next_to_ready() {
    sem_wait(&sem_grado_multiprogamacion); // Valida que no supere el grado de multiprogramacion
    t_pcb* pcb_to_ready;

    // NEW -> READY
     if(!list_is_empty(list_NEW)) {     //Si hay un proceso en NEW -> ... 
        
        // Primero obtenemos el pcb
        pthread_mutex_lock(&m_list_NEW);
        pcb_to_ready = list_get(list_NEW, 0);
        pthread_mutex_unlock(&m_list_NEW);

        //--------LOGICA PARA  INICIALIZAR EL PROCESO EN MEMORIA------------//
        log_info(logger, "Inicializamos estructuras del pcb en memoria");
        
        init_structures_memory(pcb_to_ready);
        
        // Luego de inicializar las estructuras lo obtenemos actualizado y lo removemos
        pthread_mutex_lock(&m_list_NEW);
        pcb_to_ready = list_remove(list_NEW, 0);
        pthread_mutex_unlock(&m_list_NEW);
        
        log_info(logger, "Memoria nos dio el valor de la tabla de paginas");
        //--------FIN DE LOGICA PARA INICIALIZAR EL PROCESO EN MEMORIA------------//
        
        if(string_equals_ignore_case(config_kernel->ALGORITMO_PLANIFICACION, "FEEDBACK")) {
            schedule_next_to(pcb_to_ready, READY, list_READY_RR, m_list_READY_RR);

            log_info(logger, "Entro el pcb #%d a READY!", pcb_to_ready->process_id);
            log_info(mandatory_logger,"PID: %d - Estado Anterior: NEW - Estado Actual: READY",pcb_to_ready->process_id);
            loggear_cola_ready(config_kernel->ALGORITMO_PLANIFICACION);
            
            sem_post(&sem_process_in_ready);
            
        } else if (string_equals_ignore_case(config_kernel->ALGORITMO_PLANIFICACION, "FIFO") || string_equals_ignore_case(config_kernel->ALGORITMO_PLANIFICACION, "RR")) {
            schedule_next_to(pcb_to_ready, READY, list_READY, m_list_READY);

            loggear_cola_ready(config_kernel->ALGORITMO_PLANIFICACION);

            log_info(logger, "Entro el pcb #%d a READY!", pcb_to_ready->process_id);
            log_info(mandatory_logger,"PID: %d - Estado Anterior: NEW - Estado Actual: READY",pcb_to_ready->process_id);

            sem_post(&sem_process_in_ready);
        }
         
    }                 
}

void schedule_next_to_running() {
    while(1){
        sem_wait(&sem_process_in_ready);
        sem_wait(&sem_cpu_free_to_execute); 

        if(strcmp(config_kernel->ALGORITMO_PLANIFICACION, "FIFO") == 0) {
            log_info(logger, "Entre por FIFO");

            pthread_mutex_lock(&m_list_READY);
            t_pcb* pcb_to_execute = list_remove(list_READY, 0); // Funciona bien esto??????? Confio ciegamente
            pthread_mutex_unlock(&m_list_READY);

            schedule_next_to(pcb_to_execute, RUNNING, list_RUNNING, m_list_RUNNING);

            log_info(logger, "El proceso %d cambio su estado a RUNNING", pcb_to_execute->process_id);
            log_info(mandatory_logger,"PID: %d - Estado Anterior: READY - Estado Actual: RUNNING",pcb_to_execute->process_id);
            //loggear_pcb(pcb_to_execute,logger);

            //log_warning(logger, "pcb->segment_table->number")

            send_pcb_package(connection_cpu_dispatch, pcb_to_execute, EXECUTE_PCB); // Kernel -> CPU. Preguntar a ayudante
            ultimo = "FIFO";
        } else if(strcmp(config_kernel->ALGORITMO_PLANIFICACION, "RR") == 0) { // empieza round robin
            log_info(logger, "Entre por RR");

            pthread_mutex_lock(&m_list_READY);
            t_pcb* pcb_to_execute = list_remove(list_READY, 0); // Funciona bien esto??????? Confio ciegamente
            pthread_mutex_unlock(&m_list_READY);
            
            schedule_next_to(pcb_to_execute, RUNNING, list_RUNNING, m_list_RUNNING);

            log_info(mandatory_logger,"PID: %d - Estado Anterior: READY - Estado Actual: RUNNING",pcb_to_execute->process_id);
            log_info(logger, "El proceso %d cambio su estado a RUNNING", pcb_to_execute->process_id);

            send_pcb_package(connection_cpu_dispatch, pcb_to_execute, EXECUTE_PCB); // Kernel -> CPU. Preguntar a ayudante

            sem_post(&sem_beggin_quantum); 
            ultimo = "RR";
        } else if(string_equals_ignore_case(config_kernel->ALGORITMO_PLANIFICACION, "FEEDBACK")) {
            if(!list_is_empty(list_READY_RR)) { // si la lista NO esta vacia -> entonces hay un proceso con prioridad maxima
                log_info(logger, "Entre por ROUND ROBIN de feedback");
                log_info(logger,"Entre porque la cola de ready rr no esta vacia");
                // NOSE SI AL TENER DESALOJO DEBERIAMOS TENER UN SEMAFORO QUE SE FIJE SI ESTA LIBRE EL CPU 

                pthread_mutex_lock(&m_list_READY_RR);
                t_pcb* pcb_to_execute = list_remove(list_READY_RR, 0); // Funciona bien esto??????? Confio ciegamente
                pthread_mutex_unlock(&m_list_READY_RR);
                
                schedule_next_to(pcb_to_execute, RUNNING, list_RUNNING, m_list_RUNNING);

                log_info(mandatory_logger,"PID: %d - Estado Anterior: READY - Estado Actual: RUNNING",pcb_to_execute->process_id);
                log_info(logger, "El proceso %d cambio su estado a RUNNING", pcb_to_execute->process_id);

                send_pcb_package(connection_cpu_dispatch, pcb_to_execute, EXECUTE_PCB); // Kernel -> CPU. Preguntar a ayudante

                sem_post(&sem_beggin_quantum);
                ultimo = "RR"; // por terminar quantum a FIFO
            } else if(!list_is_empty(list_READY_FIFO)) {

                //hay que hacer algo para que fifo no sea interrumpido por el quantum del RR,
                // ya que es un c.multinivel SIN desalojo, eso nose si se encarga kernel o cpu
                log_info(logger,"Entre porque la cola de ready FIFO no esta vacia");
                // sera con un semaforo que reinicie? y un if? if fifo is running then no interrumpir lokita?
                // en la funcion q envia la interrupcion?

                log_info(logger, "Entre por FIFO de feedback");

                pthread_mutex_lock(&m_list_READY_FIFO);
                t_pcb* pcb_to_execute = list_remove(list_READY_FIFO, 0); // Funciona bien esto??????? Confio ciegamente
                pthread_mutex_unlock(&m_list_READY_FIFO);

                schedule_next_to(pcb_to_execute, RUNNING, list_RUNNING, m_list_RUNNING);

                //sem_post(&estacorriendofifoynosepuedeinterrumpir) // esto nos lleva a la funcion q tira el quantum de RR
                // o tal vez es una variable que varia entre 0 y 1 (true false)

                log_info(mandatory_logger,"PID: %d - Estado Anterior: READY - Estado Actual: RUNNING",pcb_to_execute->process_id);
                log_info(logger, "El proceso %d cambio su estado a RUNNING", pcb_to_execute->process_id);

                send_pcb_package(connection_cpu_dispatch, pcb_to_execute, EXECUTE_PCB);

                ultimo = "FIFO";
            }
        } else {
            log_error(logger, "Algoritmo invalido");
        }
    }
}

//----------- prendemos el hilo que se va a encargar de contar el quantum de RR
void turn_on_switcher_clock_RR(){ 
    quantum_round_robin = atoi(config_kernel->QUANTUM_RR);
    log_info(logger, "El quantum sacado de la config es:  %d", quantum_round_robin);

    pthread_t switcher_clock_RR;
    pthread_create(&switcher_clock_RR, NULL, (void*) count_quantum_of_RoundRobin, NULL);
    pthread_detach(switcher_clock_RR);
 }


//--------- Esta es la funcion que va a contar quantum de los procesos por RR------------- //
 void count_quantum_of_RoundRobin(){
    log_info(logger, "Entre por count_quantum_of_RoundRobin");
    pthread_mutex_lock(&m_out_of_Running);
    flag_out_of_Running = 0;
    pthread_mutex_unlock(&m_out_of_Running);
    t_temporal* clock;
    while(1){
        sem_wait(&sem_beggin_quantum);

        pthread_mutex_unlock(&m_out_of_Running);
        flag_out_of_Running = 0;    
        pthread_mutex_unlock(&m_out_of_Running);

        clock = temporal_create();
        log_info(logger, "ME VOY A DORMIR EL TIEMPO DEL QUANTUM");
        while(clock != NULL){ 
            pthread_mutex_lock(&m_out_of_Running);
            if(flag_out_of_Running == 1) {
                flag_out_of_Running = 0; //CREO QUE SE PUEDE SACAR
                log_info(logger, "HUBO UNA SALIDA POR I/O O EXIT, REINICIANDO CLOCK");
                temporal_destroy(clock);
                clock = NULL;
            }
            else if(temporal_gettime(clock) >= quantum_round_robin) {
                  log_info(logger, "ME DESPERTE VOY A PRENDER SEMAFORO");
                  sem_post(&sem_interruption_quantum);
                  log_info(logger, "LE AVISE AL HILO DE INTERRUPCIONES QUE HAY UNA INTERRUPCION");
                  temporal_destroy(clock);
                  clock = NULL;
                  sem_wait(&sem_interruption_quantum_finish); //Analizar si hace falta, esta para no sacar tiempo al otro proceso
            }
            pthread_mutex_unlock(&m_out_of_Running);
        }
    }    
 }

// esta funcion se encarga de recibir los pcb del CPU
void manage_dispatch(){ 
    log_info(logger, "Entre por manejar dispatch");
    t_pcb* pcb = malloc(sizeof(t_pcb));
                       
    while(1){
        int cod_op = receive_operation(connection_cpu_dispatch);
        log_warning(logger, "Codigo de operacion recibido en el CPU dispatch %d", cod_op);
        int size = 0, desp = 0;

        void* buffer = receive_buffer(&size, connection_cpu_dispatch);
        pcb = read_pcb(buffer, &desp);

        sem_post(&sem_cpu_free_to_execute); // El proceso deja de correr, ahora puede ingresar otro a cpu
        switch(cod_op){
            case IO_SCREEN:             
            case IO_KEYBOARD:
                log_info(logger, "Recibi una solicitud de IO");
                pthread_mutex_lock(&m_out_of_Running);
                if(!strcmp(ultimo,"RR")) flag_out_of_Running = 1; //Aca hay que hacer que solo ocurra cuando salio de un RR
                pthread_mutex_unlock(&m_out_of_Running);

                change_pcb_state_to(pcb, BLOCKED);
                log_info(mandatory_logger, "PID: %d - Estado Anterior: RUNNING - Estado Actual: BLOCKED", pcb->process_id);
                
                char* device = read_string(buffer, &desp);
                char* parameter = read_string(buffer, &desp);

                log_info(mandatory_logger, "PID: %d - Bloqueado por : %s", pcb->process_id, device);

                send_pcb_io_package(pcb->client_socket, pcb, device, parameter, REQUEST);
                break;
            case IO_DEFAULT:
                log_info(logger, "Recibi una solicitud de IO");

                pthread_mutex_lock(&m_out_of_Running);
                if(!strcmp(ultimo,"RR")) flag_out_of_Running = 1;
                pthread_mutex_unlock(&m_out_of_Running);
                
                //loggear_pcb(pcb, logger);
                log_info(logger, "PASE 1");

                char* device_default = read_string(buffer, &desp);
                int parameter_integer = read_int(buffer, &desp);


                log_info(logger, "El parametro es %s y el tiempo es %d", device_default, parameter_integer);
                
                schedule_next_to(pcb, BLOCKED, list_BLOCKED, m_list_BLOCKED);

                log_info(mandatory_logger,"PID: %d - Estado Anterior: RUNNING - Estado Actual: BLOCKED",pcb->process_id);
                log_info(mandatory_logger,"PID: %d - Bloqueado por : %s",pcb->process_id,device_default);

                execute_io_default(pcb, device_default, parameter_integer);

                break;
            // codigos de op q no son de io
            case EXECUTE_INTERRUPTION:
                log_info(logger, "Salio el pcb %d por fin de Quantum", pcb->process_id);
                //loggear_pcb(pcb, logger);

                    pthread_mutex_lock(&m_list_RUNNING);
                list_remove(list_RUNNING, 0); // Inicializar pcb y despues liberarlo
                    pthread_mutex_unlock(&m_list_RUNNING);
                
                log_info(mandatory_logger,"PID: %d - Desalojo por fin de Quantum",pcb->process_id);
                
                if(string_equals_ignore_case(config_kernel->ALGORITMO_PLANIFICACION, "RR")) {
                    schedule_next_to(pcb, READY, list_READY, m_list_READY);

                    log_info(mandatory_logger,"PID: %d - Estado Anterior: RUNNING - Estado Actual: READY",pcb->process_id);
                    loggear_cola_ready(config_kernel->ALGORITMO_PLANIFICACION);

                    sem_post(&sem_process_in_ready); // Envio a CPU 
                } else if (string_equals_ignore_case(config_kernel->ALGORITMO_PLANIFICACION, "FEEDBACK")) {
                    schedule_next_to(pcb, READY, list_READY_FIFO, m_list_READY_FIFO); // Le bajamos la prioridad al proceso a la cola 2
                    
                    loggear_cola_ready(config_kernel->ALGORITMO_PLANIFICACION);
                    log_info(mandatory_logger,"PID: %d - Estado Anterior: RUNNING - Estado Actual: READY",pcb->process_id);

                    sem_post(&sem_process_in_ready); // Envio a CPU 
                }
                break;

            case SEG_FAULT:  //Error: Segmentation Fault (SEG_FAULT).SEG_FAULT //Error: Segmentation Fault (SEG_FAULT).
                
                // hay que matar al proceso, chekeen si lo hice bien

                sem_post(&sem_grado_multiprogamacion); // Finalizo el proceso -> Deja de estar en memoria -> puede ingresar otro proceso
                change_pcb_state_to(pcb, EXIT);

                pthread_mutex_lock(&m_out_of_Running);
                if(!strcmp(ultimo,"RR")) flag_out_of_Running = 1;
                pthread_mutex_unlock(&m_out_of_Running);

                    pthread_mutex_lock(&m_list_RUNNING);
                    list_remove(list_RUNNING, 0); // inicializar pcb y despues liberarlo
                    pthread_mutex_unlock(&m_list_RUNNING);

                    pthread_mutex_lock(&m_list_EXIT);
                list_add(list_EXIT, pcb);
                    pthread_mutex_unlock(&m_list_EXIT);

                log_info(mandatory_logger,"PID: %d - Estado Anterior: RUNNING - Estado Actual: EXIT",pcb->process_id);
                log_info(logger, "Finalizo proceso con id: %d", pcb->process_id);

                //FALTA ENVIARMENSAJE PARA QUE FINALICEN LAS ESTRUCTURAS DEL PROCESO EN MEMORIA
               
                
                log_info(logger, "Volviendo a la consola: %d", pcb->client_socket);
                t_package* package_console = create_package(END_CONSOLE_SEG_FAULT);
	            send_package(package_console, pcb->client_socket);
                delete_package(package_console);

                log_info(logger, "PUM! Mate memoria: %d", pcb->client_socket);
                t_package* package_memory = create_package(END_STRUCTURES);
                add_pcb_to_package(package_memory, pcb);
	            send_package(package_memory, memory_connection);
                delete_package(package_memory);

                break;
            case PAGE_FAULT:
                log_info(logger,"CPU ME PIDIO QUE SOLUCIONE UN PAGE FAULT");
                int segment_number = read_int(buffer, &desp);
                int page_number = read_int(buffer, &desp);
                log_info(logger,"valoresRecibidos de para sol. PF : segmento %d , pagina %d",segment_number,page_number);
                log_info(mandatory_logger, "Page Fault PID: %d - Segmento: %d - Pagina: %d", pcb->process_id, segment_number, page_number);

                t_list* arguments_list = list_create();
                list_add(arguments_list, pcb);
                list_add(arguments_list, segment_number);
                list_add(arguments_list, page_number);

                    pthread_mutex_lock(&m_list_RUNNING);
                list_remove(list_RUNNING, 0); 
                    pthread_mutex_unlock(&m_list_RUNNING);

                pthread_t execute_page_fault;
                pthread_create(&execute_page_fault, NULL, (void*) resolve_page_fault, arguments_list);    
                pthread_detach(execute_page_fault);
                break;
            case END_PROCESS: 
                sem_post(&sem_grado_multiprogamacion); // Finalizo el proceso -> Deja de estar en memoria -> puede ingresar otro proceso
                change_pcb_state_to(pcb, EXIT);
                pthread_mutex_lock(&m_out_of_Running);
                if(!strcmp(ultimo,"RR")) flag_out_of_Running = 1;
                pthread_mutex_unlock(&m_out_of_Running);

                    pthread_mutex_lock(&m_list_RUNNING);
                    list_remove(list_RUNNING, 0); // inicializar pcb y despues liberarlo
                    pthread_mutex_unlock(&m_list_RUNNING);

                    pthread_mutex_lock(&m_list_EXIT);
                list_add(list_EXIT, pcb);
                    pthread_mutex_unlock(&m_list_EXIT);

                log_info(mandatory_logger,"PID: %d - Estado Anterior: RUNNING - Estado Actual: EXIT",pcb->process_id);
                log_info(logger, "Finalizo proceso con id: %d", pcb->process_id);
               
                t_package* package = create_package(END_STRUCTURES);
                add_pcb_to_package(package, pcb);
	            send_package(package, memory_connection);
                delete_package(package);

                send_end_to_console(pcb->client_socket);
                break;
            case -1:
                log_warning(logger, "Se desconecta el hilo por desconexion de cpu");
                end_kernel_module(1);
                pthread_exit(NULL);
                break;
            default: 
                log_error(logger, "Codigo de operacion desconocido");
                break;
        }
    }
}

void send_end_to_console(int console) {
    log_info(logger, "Volviendo a la consola: %d", console);
    t_package* package = create_package(END_CONSOLE);
	send_package(package, console);
}

// este es el hilo que se encarga de manejar las interrupciones generadas por kernel --> cpu
void manage_interrupt() {
    log_info(logger, "Entre por manage_interrupt");
    
    while(1) {
        sem_wait(&sem_interruption_quantum);

        // Envia un paquete a cpu para interrumpir el clock
        if (!list_is_empty(list_RUNNING)) {
            t_package* package = create_package(EXECUTE_INTERRUPTION);
            send_package(package, connection_cpu_interrupt);
        
            log_info(logger, "TENGO QUE INTERRUMPIR PORQUE SE CUMPLIO EL QUANTUM");  
            sem_post(&sem_interruption_quantum_finish);
        }
    }
}

void manage_memory() {
    log_info(logger, "Entre por manage_memory");
    while(1) {
        int cod_op = receive_operation(memory_connection);
        log_warning(logger, "Codigo de operacion recibido de la memoria %d", cod_op);
        t_pcb* pcb;

        switch(cod_op){
            case INIT_PROCESS_STRUCTURES_MEMORY_RESPONSE:
                pcb = receive_pcb(memory_connection, logger);
                list_replace(list_NEW, 0, pcb); 
                sem_post(&sem_init_process_memory);
                break;
            case RESPONSE_PAGE_FAULT:
                pcb = receive_pcb(memory_connection, logger); 
                //loggear_pcb(pcb, logger);
               
                log_warning(logger,"RESPONSE_PAGE_FAULT MEMORIA SOLUCIONO PAGE FAULT");
                
                if (string_equals_ignore_case(config_kernel->ALGORITMO_PLANIFICACION, "FEEDBACK")) {
                    schedule_next_to(pcb, READY, list_READY_RR, m_list_READY_RR);       //CREO QUE ACA Y ABAJO FALTA UN LOG DE QUE CAMBIA DE ESTADO PERO NOSE CUAL
                    loggear_cola_ready(config_kernel->ALGORITMO_PLANIFICACION);
                } else { // en caso de que que sean FIFO O RR , no usan prioridades.
                    schedule_next_to(pcb, READY, list_READY, m_list_READY);
                    loggear_cola_ready(config_kernel->ALGORITMO_PLANIFICACION);
                    sem_post(&sem_process_in_ready); // Envio a CPU 
                }
                change_pcb_state_to(pcb, READY);
                log_info(mandatory_logger,"PID: %d - Estado Anterior: BLOCKED - Estado Actual: READY",pcb->process_id);
                pthread_mutex_unlock(&m_resolve_page_fault);            
                break;
            case -1:
                log_warning(logger, "La memoria se desconecto");
                end_kernel_module(1);
                pthread_exit(NULL);
                break;
            default: 
                log_error(logger, "Codigo de operacion desconocido");
                break;
        }
    }
}

void execute_input_output(t_pcb* pcb) {
    
}

void init_global_lists() { 
    list_NEW =               list_create();
	list_READY =             list_create();
	list_BLOCKED =           list_create();
	list_RUNNING =           list_create();
	list_EXIT =              list_create();
    list_IO =                list_create();
    list_READY_FIFO =        list_create();
    list_READY_RR =          list_create();
}

void create_io_list(char* nombre_io){
    log_info(logger, "ACA ESTA EL LOG %s", nombre_io);
}

// ----------------------FUNCIONES PARA MEMORIA ----------------


// _______________________ KERNEL -----> MEMORIA _______________________

/*void initialize_structures(t_pcb* pcb) {
    t_package* package = create_package(INIT_STRUCTURES);
    add_int_to_package(package, pcb->size);
    add_int_to_package(package, pcb->process_id);

    send_package(package, memory_connection);
    delete_package(package);
}*/ //nachoF: esto se usa?

void create_queue_for_io(char* io_name, char* time) {

    t_list* io_list = list_create();
    t_device* device = malloc(sizeof(t_device));

    device->device_name = io_name;
    device->device_locked_time = atoi(time);
    device->list_processes = io_list;

    sem_t sem;
    sem_init(&sem, 1, 0);
    device->sem_devices = sem;

    dictionary_put(dictionary_io, io_name, device);

}

void init_device_config_dictionary() { 
    dictionary_io = dictionary_create();

    int index = 0;
    while(config_kernel->DISPOSITIVOS_IO[index] != NULL && config_kernel->TIEMPOS_IO[index] != NULL) {   // [DISCO, MOUSE]
        create_queue_for_io(config_kernel->DISPOSITIVOS_IO[index], config_kernel->TIEMPOS_IO[index]); 
        t_device* device = dictionary_get(dictionary_io, config_kernel->DISPOSITIVOS_IO[index]);

        pthread_t execute_io;
            pthread_create(&execute_io, NULL, (void*) simulate_io_default, device);
            pthread_detach(execute_io);

        index++;
    }
}

void simulate_io_default(t_device* device) {        // DEBERIA HABER MUTEX PARA QUE 2 DISCOS NO PUEDAN EJECTURASE A AL VEZ?
    while(1){
        sem_wait(&device->sem_devices);
        log_info(logger,"El dispositivo %s inicio IO", device->device_name);

        int time_device = device->device_locked_time;
        int time_parameter = list_remove(device->list_processes, 0);
        t_pcb* pcb = list_remove(device->list_processes, 0);

        usleep(time_parameter * 1000 * time_device);

        /*int io_time = (time_parameter * 1000) * time_device; // pasa la cuenta 
        usleep(io_time);*/

        if(string_equals_ignore_case(config_kernel->ALGORITMO_PLANIFICACION, "FEEDBACK")) { 
            
            schedule_next_to(pcb, READY, list_READY_RR, m_list_READY_RR);
            log_info(mandatory_logger, "PID: %d - Estado Anterior: BLOCKED - Estado Actual: READY", pcb->process_id);
            pthread_mutex_lock(&m_list_READY_FIFO);
            pthread_mutex_lock(&m_list_READY_RR);
            loggear_cola_ready(config_kernel->ALGORITMO_PLANIFICACION);
            pthread_mutex_unlock(&m_list_READY_FIFO);
            pthread_mutex_unlock(&m_list_READY_RR);
            log_info(logger,"Ya meti al proceso %d en la cola de ready RR",pcb->process_id);
            sem_post(&sem_process_in_ready);
        } else {
            // si el algoritmo es FIFO o RR
            log_info(mandatory_logger,"PID: %d - Estado Anterior: BLOCKED - Estado Actual: READY",pcb->process_id);
            schedule_next_to(pcb, READY, list_READY, m_list_READY);
            pthread_mutex_lock(&m_list_READY);
            loggear_cola_ready(config_kernel->ALGORITMO_PLANIFICACION);
            pthread_mutex_unlock(&m_list_READY);
           
            sem_post(&sem_process_in_ready);
        }

        log_info(logger,"El dispositivo %s finalizo IO", device->device_name);
    } 
}

void execute_io_default(t_pcb* pcb, char* device_name, int parameter) {
    log_info(logger, "Me llego de CPU la IO %s", device_name);
    t_device* device = dictionary_get(dictionary_io, device_name);
    list_add(device->list_processes, parameter); 
    list_add(device->list_processes, pcb);
    dictionary_put(dictionary_io, device_name, device);
    sem_post(&device->sem_devices);
}



// ------------------- MEMORIA ------------------- //

void init_structures_memory(t_pcb* pcb) {
    t_package* package = create_package(INIT_PROCESS_STRUCTURES_MEMORY);

    // metemos los segmentos en la t_list s
    //segment_table

    add_pcb_to_package(package, pcb);
    send_package(package, memory_connection);
    delete_package(package);
    
    sem_wait(&sem_init_process_memory);
    /*int code = 0;
    while (code != INIT_STRUCTURES_MEMORY_RESPONSE) {
        code = wait_client(memory_connection, logger);
        pcb = receive_pcb(memory_connection, logger);
    }
    return pcb;*/
}



void resolve_page_fault(t_list* arguments_list){
    pthread_mutex_lock(&m_resolve_page_fault);
    //mutex 
    
    log_info(logger, "resolve_page_fault a punto de solucionar PF hacia memo");   

    t_pcb* pcb = list_get(arguments_list, 0);

    change_pcb_state_to(pcb, BLOCKED);
    log_info(mandatory_logger,"PID: %d - Estado Anterior: RUNNING - Estado Actual: BLOCKED",pcb->process_id);

       
    int segment_number = list_get(arguments_list, 1);  
    int page_number = list_get(arguments_list, 2);
      
    log_info(logger, "voy a enviar datos necesarios para solucionar REQUEST_PAGE_FAULT");
    
    //loggear_pcb(pcb,logger);
    // 2) solicitar a memoria que cargue en memoria principal la pagina.
  
    t_package* package = create_package(REQUEST_PAGE_FAULT);
    add_pcb_to_package(package, pcb);
    add_int_to_package(package, segment_number);
    add_int_to_package(package, page_number);
    send_package(package, memory_connection); //poner socket memory
    delete_package(package);

}

void loggear_cola_ready(char* algoritmo){
    char* pids_aux = string_new();
    if(string_equals_ignore_case(algoritmo,"FEEDBACK")){
        pids_aux = pids_on_ready_FIFO();
        log_info(mandatory_logger, "Cola Ready de FIFO %s: %s.", algoritmo, pids_aux);
        pids_aux = pids_on_ready_RR();
        log_info(mandatory_logger, "Cola Ready de RR %s: %s.", algoritmo, pids_aux);
    }else{
        pids_aux = pids_on_ready();
        log_info(mandatory_logger, "Cola Ready %s: %s.", algoritmo, pids_aux);
        }
    free(pids_aux);
}

char* pids_on_ready_FIFO(){
    char* aux = string_new();
    string_append(&aux,"[");
    int pid_aux;
    for(int i = 0 ; i < list_size(list_READY_FIFO); i++){
        t_pcb* pcb = list_get(list_READY_FIFO,i);
        pid_aux = pcb->process_id;
        string_append(&aux,string_itoa(pid_aux));
        if(i != list_size(list_READY_FIFO)-1) string_append(&aux,"|");
    }
    string_append(&aux,"]");
    return aux;
}

char* pids_on_ready_RR(){
    char* aux = string_new();
    string_append(&aux,"[");
    int pid_aux;
    for(int i = 0 ; i < list_size(list_READY_RR); i++){
        t_pcb* pcb = list_get(list_READY_RR,i);
        pid_aux = pcb->process_id;
        string_append(&aux,string_itoa(pid_aux));
        if(i != list_size(list_READY_RR)-1) string_append(&aux,"|");
    }
    string_append(&aux,"]");
    return aux;
}

char* pids_on_ready(){
    char* aux = string_new();
    string_append(&aux,"[");
    int pid_aux;
    for(int i = 0 ; i < list_size(list_READY); i++){
        t_pcb* pcb = list_get(list_READY,i);
        pid_aux = pcb->process_id;
        string_append(&aux,string_itoa(pid_aux));
        if(i != list_size(list_READY)-1) string_append(&aux,"|");
    }
    string_append(&aux,"]");
    return aux;
}