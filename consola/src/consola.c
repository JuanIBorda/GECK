#include "consola.h"

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

    logger = log_create("./logs/consola.log", "CONSOLA", true, LOG_LEVEL_INFO);

    // validamos la cantidad de parametros que le llegaron al main.
    if(argc < 3){
        log_error(logger, "Faltan parametros para ejecutar la consola. La sintaxis debe ser:  './consola PATH_CONFIG PATH_INSTRUCTIONS' ");
        return EXIT_FAILURE;
    } 

    if(argc > 3){
        log_error(logger, "Hay un exceso de parametros para ejecutar la consola. La sintaxis debe ser:  './consola PATH_CONFIG PATH_INSTRUCTIONS' ");
        return EXIT_FAILURE;
    }

    /*----------------- ARCHIVO DE CONFIGURACION -----------------*/
    t_config* config = init_config(argv[1]);

    char *buffer = readFile(argv[2]);
    //log_info(logger, "Lectura del buffer: \n%s ", buffer);

    if(buffer == NULL){
        log_error(logger, "No se encontraron instrucciones.");
        return EXIT_FAILURE; 
    }

	char* ip = config_get_string_value(config, "IP_KERNEL");
	char* port = config_get_string_value(config, "PUERTO_KERNEL");
    log_info(logger, "El cliente se conectara a %s:%s", ip, port);
    
    /*----------------- CONEXION CON KERNEL -----------------*/
    int connection = create_connection(ip, port, logger);
    if(connection == -1) {
        log_error(logger, "No se pudo conectar al servidor");
        exit(2);
    }
    log_info(logger, "Conexion exitosa con el servidor: %d", connection);


    char** segments = config_get_array_value(config, "SEGMENTOS");
    char** instructions = parseLines(buffer);

    send_process_package(connection, instructions, segments); // Enviamos las instrucciones  al kernel

    int delay = config_get_int_value(config, "TIEMPO_PANTALLA"); // Necesitamos este valor para congelar la pantalla antes de volver al kernel
    int end_while = 0, code, desp = 0, size = 0, value;
    t_pcb* pcb;
    char* parameter;
    while(end_while == 0){
        log_info(logger, "Esperando respuesta del KERNEL.");
        code = receive_operation(connection);
        switch(code){
            case IO_SCREEN:                
                // Recibir parametros

                size = 0; 
                desp = 0;

                buffer = receive_buffer(&size, connection);
                pcb = read_pcb(buffer, &desp);
                read_string(buffer, &desp); // Lee el device, lo salteamos
                parameter = read_string(buffer, &desp);

                // Operacion
                show_on_screen_pcb_register(pcb, parameter);
                usleep(delay * 1000);

                // Vuelta a kernel
                send_pcb_package(connection, pcb, IO_SCREEN_RESPONSE);

                break;
            case IO_KEYBOARD:
                size = 0;
                desp = 0;
                buffer = receive_buffer(&size, connection);
                pcb = read_pcb(buffer, &desp);
                read_string(buffer, &desp); // Lee el device, lo salteamos 
                parameter = read_string(buffer, &desp);

                // Operacion
                printf("Ingresa el valor del registro a actualizar: ");
                scanf("%d", &value);

                if(strcmp(parameter, "AX") == 0) pcb->cpu_registers[AX] = value;
                else if(strcmp(parameter, "BX") == 0) pcb->cpu_registers[BX] = value;
                else if(strcmp(parameter, "CX") == 0) pcb->cpu_registers[CX] = value;
                else if(strcmp(parameter, "DX") == 0) pcb->cpu_registers[DX] = value;

                loggear_registers(logger, pcb->cpu_registers);

                // Vuelta a kernel 
                send_pcb_package(connection, pcb, IO_KEYBOARD_RESPONSE);
                break;
            case END_CONSOLE:
                log_info(logger, "Se recibio la orden finalizar consola enviada por KERNEL.");
                end_while = 1;
                break;
            case END_CONSOLE_SEG_FAULT:
                log_info(logger, "Se recibio la orden finalizar consola enviada por KERNEL SEG_FAULT.");
                end_while = 1;
                break;
            case -1:
                log_warning(logger, "El modulo kernel se desconecto");
                close_connection(connection);
                end_while = 1;
                break;
            default:
                log_error(logger, "Murio el KERNEL.");
                end_while = 1;
                break;
        }
    }
    

    // Hay que liberar memoria NO COLGARSE  --> nacho says: nose si esto es correcto but i let it as a gift!

    log_destroy(logger);
    config_destroy(config);
    //matar la conexion
    free(buffer); // esto anda?
    close_connection(connection);


    return 0;
}   /*----------------- FIN MAIN -----------------*/



// para conexion con KERNEL
t_config* init_config(char* path){
    t_config* new_config = config_create(path);

	if (new_config == NULL) {
		log_error(logger, "No se encontro el archivo de configuracion de la consola\n");
		exit(2);
	}

	return new_config;
}

char* readFile(char* path){
    FILE* file = fopen(path, "r");
    if(file == NULL){
        log_error(logger, "No se encontro el archivo: %s", path);
        exit(1);
    }

    struct stat stat_file;
    stat(path, &stat_file);

    char* buffer = calloc(1, stat_file.st_size + 1);
    fread(buffer, stat_file.st_size, 1, file);

    return buffer;
}

char** parseLines(char* buffer) {
    char** lines = string_split(buffer, "\n");
    return lines;
}


void send_process_package(int connection, char** instructions, char** segments) {
    t_package* package = create_package(INIT_PROCESS);
    
    //add_value_to_package(package, buffer, (strlen(buffer) + 1)); // Envio el buffer con las instrucciones
    
    add_string_array_to_package(package, instructions);
    add_string_array_to_package(package, segments); // Envio la lista de segmentos

    send_package(package, connection);
}

void show_on_screen_pcb_register(t_pcb* pcb ,char* parameter){
    if(string_equals_ignore_case(parameter, "AX")) printf("El valor del registro %s es = %d\n", parameter,pcb->cpu_registers[atoi(parameter)]);
    else if(string_equals_ignore_case(parameter, "BX")) printf("El valor del registro %s es = %d\n",parameter,pcb->cpu_registers[atoi(parameter)]);
    else if(string_equals_ignore_case(parameter, "CX")) printf("El valor del registro %s es = %d\n",parameter,pcb->cpu_registers[atoi(parameter)]);
    else if(string_equals_ignore_case(parameter, "DX")) printf("El valor del registro %s es = %d\n",parameter,pcb->cpu_registers[atoi(parameter)]);
}
