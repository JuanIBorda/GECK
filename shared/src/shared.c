#include "shared.h"

/*---------------------------------- INSTRUCTIONS ----------------------------------*/
// https://stackoverflow.com/questions/4014827/how-can-i-compare-strings-in-c-using-a-switch-statement (Gracias por tanto, perdon por tan poco)

typedef struct { char *key; int val; } t_symstruct;

static t_symstruct lookuptable[] = {
    { "SET", I_SET }, 
	{ "ADD", I_ADD }, 
	{ "I/O", I_IO }, 
	{ "MOV_IN", I_MOV_IN }, 
	{ "MOV_OUT", I_MOV_OUT }, 
	{ "EXIT", I_EXIT }
};

int keyfromstring(char *key) {
    int i;
    for (i=0; i < 6; i++) {
        t_symstruct sym = lookuptable[i];
        if (strcmp(sym.key, key) == 0)
            return sym.val;
    }
    return BADKEY;
}


/*---------------------------------- NETWORK.H ----------------------------------*/

// Servidor
int server_init(char* ip, char* port, t_log* logger) {
    int server_socket;
    struct addrinfo hints, *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, port, &hints, &server_info);

    // se crea el socket de escucha del server
    server_socket = socket(server_info->ai_family,
							server_info->ai_socktype,
		                    server_info->ai_protocol);
    // se asocia el socket a un puerto
    bind(server_socket, server_info->ai_addr, server_info->ai_addrlen);

	// Escuchamos las conexiones entrantes
	listen(server_socket, SOMAXCONN);
    
    freeaddrinfo(server_info);

    return server_socket;
}

int wait_client(int server_socket, t_log* logger) {
    //acepta un nuevo cliente
    int client_socket = accept(server_socket, NULL, NULL);
    return client_socket;
}

int receive_operation(int client_socket) {
    
    int op_code;
    if(recv(client_socket, &op_code, sizeof(int), MSG_WAITALL) > 0){
        return op_code;
    }else{
        close(client_socket);
        return -1;
    }
}

void close_connection(int socket) {
	close(socket);
}

// Cliente
int create_connection(char* ip, char* port, t_log* logger) {
    struct addrinfo hints, *server_info;

    memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

    getaddrinfo(ip, port, &hints, &server_info);

    // Ahora vamos a crear el socket.
    int client_socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

	//
	const int enable = 1;
	if (setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) log_error(logger, "setsockopt(SO_REUSEADDR) failed");

    // Ahora que tenemos el socket, vamos a conectarlo
    if (connect(client_socket, server_info->ai_addr, server_info->ai_addrlen) == -1){
        printf("\n[ion!]\n");
		return -1;
	}

    freeaddrinfo(server_info);

	return client_socket;
}

/*---------------------------------- NETWORK.H ----------------------------------*/





/*---------------------------------- PCB_UTILS.H --------------------------------------------------------------------*/
t_pcb* pcb_create(char** instructions, int client_socket, int pid, char** segments) {
    t_pcb* new_pcb = malloc(sizeof(t_pcb));

    new_pcb->process_id = pid;
    new_pcb->size = 0; // TODO: Ver en que basarse para ponele un size
    new_pcb->status = NEW;
	new_pcb->instructions = instructions;
    new_pcb->program_counter = 0;
    new_pcb->client_socket = client_socket;

    new_pcb->cpu_registers[AX] = 0;
	new_pcb->cpu_registers[BX] = 0;
	new_pcb->cpu_registers[CX] = 0;
	new_pcb->cpu_registers[DX] = 0;

	new_pcb->segment_table = list_create();

    int size = string_array_size(segments);
	
    for(int i = 0; i < size; i++) {
		t_segment_table* segment_table = malloc(sizeof(t_segment_table));
		segment_table->number = i;
		segment_table->size = atoi(segments[i]);
		segment_table->index_page = -1; // queda en 0 porque lo inicializa memoria. pero si lo recibimos asi va a sobreescribir lo q setee memoria
		
		list_add(new_pcb->segment_table, segment_table);
	}

    return new_pcb;
}


void loggear_pcb(t_pcb* pcb, t_log* logger){
	log_info(logger, "------------------------------- INICIAR PCB Logger -------------------------------");
	log_info(logger, "Id %d", pcb->process_id);
	logguar_state(logger, pcb->status);
	log_info(logger, "Tamanio %d", pcb->size);

	// Loguea mal las instrucciones, ver porque
	log_info(logger, "Instrucciones");
	for(int i=0; i < string_array_size(pcb->instructions); i++) {
		log_info(logger, "#%d: %s", i, pcb->instructions[i]);
	}

	log_info(logger, "Tabla de segmentos");
	for(int i=0; i < list_size(pcb->segment_table); i++) {
		t_segment_table* segment_table = list_get(pcb->segment_table, i);

		log_info(logger, "----------- segmento numero %d",i);
		log_info(logger, "#%d size: %d", segment_table->number, segment_table->size);
		log_info(logger, "#%d page_index: %d", segment_table->number, segment_table->index_page);
	}

	log_info(logger, "Program counter %d", pcb->program_counter);
	log_info(logger, "socket_cliente_consola %d", pcb->client_socket);

	log_info(logger, "------------------------------- FIN PCB Logger -------------------------------");
}

char* get_state_name(int state){
	char* string_state;
	switch(state){
		case NEW :
			string_state = string_duplicate("NEW");
			break;
		case READY:
			string_state = string_duplicate("READY");
			break;
		case BLOCKED:
			string_state = string_duplicate("BLOCKED");
			break;
		case RUNNING:
			string_state = string_duplicate("RUNNING");
			break;
		case EXIT:
			string_state = string_duplicate("EXIT");
			break;
		default:
			string_state = string_duplicate("ESTADO NO REGISTRADO");
			break;
	}
	return string_state;
}

void logguar_state(t_log* logger, int state) {
	char* string_state = get_state_name(state);
	log_info(logger, "Estado %d (%s)", state, string_state);
	free(string_state);
}

t_pcb* receive_pcb(int socket, t_log* logger) {
	// OJO! Hay que tener cuidado como deserializamos el pcb, tiene que estar en el mismo orden que la serializacion

    t_pcb* new_pcb = malloc(sizeof(t_pcb));
    int size = 0;
    char * buffer;
    int desp = 0;
    
    buffer = receive_buffer(&size, socket);

    new_pcb->process_id =       read_int(buffer, &desp);
	new_pcb->status =           read_int(buffer, &desp);
    new_pcb->size =             read_int(buffer, &desp);
    
	new_pcb->instructions =     read_string_array(buffer, &desp); //Es un array de strings
	
	new_pcb->cpu_registers[AX] = read_uint32(buffer, &desp);
	new_pcb->cpu_registers[BX] = read_uint32(buffer, &desp);
	new_pcb->cpu_registers[CX] = read_uint32(buffer, &desp);
	new_pcb->cpu_registers[DX] = read_uint32(buffer, &desp);

	new_pcb->program_counter =  read_int(buffer, &desp);

	new_pcb->client_socket =  read_int(buffer, &desp);

	new_pcb->segment_table = list_create();
	int s = read_int(buffer, &desp);
	for(int i = 0; i < s; i++) {
		t_segment_table* segment_table = malloc(sizeof(t_segment_table));
		segment_table->number = read_int(buffer, &desp);
		segment_table->size = read_int(buffer, &desp);
		segment_table->index_page = read_int(buffer, &desp);
		
		list_add(new_pcb->segment_table, segment_table);
	}

    //loggear_pcb(new_pcb, logger);
	//free(buffer);
    return new_pcb;
}

t_pcb* read_pcb(void* buffer, int* desp) { 
	t_pcb* new_pcb = malloc(sizeof(t_pcb));
	new_pcb->process_id =       read_int(buffer, desp);
	new_pcb->status =           read_int(buffer, desp);
    new_pcb->size =             read_int(buffer, desp);

	new_pcb->instructions =     read_string_array(buffer, desp); //Es un array de strings

	new_pcb->cpu_registers[AX] = read_uint32(buffer, desp);
	new_pcb->cpu_registers[BX] = read_uint32(buffer, desp);
	new_pcb->cpu_registers[CX] = read_uint32(buffer, desp);
	new_pcb->cpu_registers[DX] = read_uint32(buffer, desp);

	new_pcb->program_counter =  read_int(buffer, desp);

	new_pcb->client_socket =  read_int(buffer, desp);

	new_pcb->segment_table = list_create();
	int size = read_int(buffer, desp);
	for(int i = 0; i < size; i++) {
		t_segment_table* segment_table = malloc(sizeof(t_segment_table));
		segment_table->number = read_int(buffer, desp);
		segment_table->size = read_int(buffer, desp);
		segment_table->index_page = read_int(buffer, desp);
		
		list_add(new_pcb->segment_table, segment_table);
	}

	return new_pcb;
}


void change_pcb_state_to(t_pcb* pcb, pcb_status newState){
    pcb->status = newState;
}

void send_pcb_package(int connection, t_pcb* pcb, operation_code code){
    t_package* package = create_package(code);
    add_pcb_to_package(package, pcb);
	send_package(package, connection);
	delete_package(package);
}



/*---------------------------------- PCB_UTILS.H --------------------------------------------------------------------*/

void loggear_registers(t_log* logger, uint32_t* registers) {
    log_info(logger, "El registro del PCB AX:  %d", registers[AX]);
    log_info(logger, "El registro del PCB BX:  %d", registers[BX]);
    log_info(logger, "El registro del PCB CX:  %d", registers[CX]);
    log_info(logger, "El registro del PCB DX:  %d", registers[DX]);
}


/*---------------------------------- SERIALIZE.H ----------------------------------*/
t_package* create_package(operation_code op_code){
    t_package* package = malloc(sizeof(t_package));
    package->op_code = op_code;
    create_buffer(package);
    return package;
}

void send_package(t_package* package, int client_socket){
	int bytes = package->buffer->size + 2 * sizeof(int);
	void* to_send = serialize_package(package, bytes);

	send(client_socket, to_send, bytes, 0);

	free(to_send);
}

void delete_package(t_package* package){
	free(package->buffer->stream);
	free(package->buffer);
	free(package);
}

void* serialize_package(t_package* package, int bytes){
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(package->op_code), sizeof(int));
	desplazamiento += sizeof(int);
	memcpy(magic + desplazamiento, &(package->buffer->size), sizeof(int));
	desplazamiento += sizeof(int);
	memcpy(magic + desplazamiento, package->buffer->stream, package->buffer->size);
	desplazamiento += package->buffer->size;

	return magic;
}


void create_buffer(t_package* package) {
    package->buffer = malloc(sizeof(t_buffer));
	package->buffer->size = 0;
	package->buffer->stream = NULL;
}


void add_value_to_package(t_package* package, void* value, int size) {
    package->buffer->stream = realloc(package->buffer->stream, package->buffer->size + size + sizeof(int));

	memcpy(package->buffer->stream + package->buffer->size, &size, sizeof(int));
	memcpy(package->buffer->stream + package->buffer->size + sizeof(int), value, size);

	package->buffer->size += size + sizeof(int);
}

void add_pcb_to_package(t_package* package, t_pcb* pcb) {
 	add_int_to_package(package, pcb->process_id);
    add_int_to_package(package, pcb->status);
    add_int_to_package(package, pcb->size);
    
	add_string_array_to_package(package, pcb->instructions);

	add_registers_to_package(package, pcb->cpu_registers);
    
    add_int_to_package(package, pcb->program_counter); 
    
	add_int_to_package(package, pcb->client_socket);

	add_segment_table_to_package(package, pcb->segment_table);
}

void add_segment_table_to_package(t_package* package, t_list* segment_table_list) {
    int size = list_size(segment_table_list);
    add_int_to_package(package, size);
    for(int i = 0; i < size; i++) {
		t_segment_table* segment_table = list_get(segment_table_list, i);
		add_int_to_package(package, segment_table->number);
		add_int_to_package(package, segment_table->size);
		add_int_to_package(package, segment_table->index_page);
	}
}

void add_registers_to_package(t_package* package, uint32_t* cpu_registers){
	add_uint32_to_package(package, cpu_registers[AX]);
	add_uint32_to_package(package, cpu_registers[BX]);
	add_uint32_to_package(package, cpu_registers[CX]);
	add_uint32_to_package(package, cpu_registers[DX]);
}

void add_uint32_to_package(t_package* package, uint32_t x) {
	package->buffer->stream = realloc(package->buffer->stream, package->buffer->size + sizeof(uint32_t));
	memcpy(package->buffer->stream + package->buffer->size, &x, sizeof(uint32_t));
	package->buffer->size += sizeof(uint32_t);
}

void add_int_to_package(t_package* package, int x) {
	package->buffer->stream = realloc(package->buffer->stream, package->buffer->size + sizeof(int));
	memcpy(package->buffer->stream + package->buffer->size, &x, sizeof(int));
	package->buffer->size += sizeof(int);
}

void add_float_to_package(t_package* package, float x) {
	package->buffer->stream = realloc(package->buffer->stream, package->buffer->size + sizeof(float));
	memcpy(package->buffer->stream + package->buffer->size, &x, sizeof(float));
	package->buffer->size += sizeof(float);
}


void add_string_array_to_package(t_package* package, char** arr) {
    int size = string_array_size(arr);
    add_int_to_package(package, size);
    for(int i = 0; i < size; i++) {
    	add_value_to_package(package, arr[i], string_length(arr[i])+1); 
	}
}

void* receive_buffer(int* size, int client_socket){
	void* buffer;
	recv(client_socket, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(client_socket, buffer, *size, MSG_WAITALL);
	return buffer;
}

void* receive_after_pcb_buffer(int* size, int client_socket) {
	void* buffer;
	recv(client_socket, size, sizeof(int), MSG_WAITALL);
	recv(client_socket, size, sizeof(t_pcb), MSG_WAITALL);
	buffer = malloc(*size);
	recv(client_socket, buffer, *size, MSG_WAITALL);
	return buffer;
}

int read_int(char* buffer, int* desp) {
	int ret;
	memcpy(&ret, buffer + (*desp), sizeof(int));
	(*desp)+=sizeof(int);
	return ret;
}

char* read_string(char* buffer, int* desp){
	int size = read_int(buffer, desp);

	char* valor = malloc(size);
	memcpy(valor, buffer+(*desp), size);
	(*desp)+=size;

	return valor;
}

char** read_string_array(char* buffer, int* desp) {
    int length = read_int(buffer, desp);
    char** arr = malloc((length + 1) * sizeof(char*));

    for(int i = 0; i < length; i++)
    {
        arr[i] = read_string(buffer, desp);
    }
    arr[length] = NULL;

    return arr;
}

uint32_t read_uint32(char* buffer, int* desp) {
	uint32_t ret;
	memcpy(&ret, buffer + (*desp), sizeof(uint32_t));
	(*desp)+=sizeof(uint32_t);
	return ret;
}


/*---------------------------------- SERIALIZE.H ----------------------------------*/
void send_pcb_io_package(int client_socket, t_pcb* pcb, char* device, char* parameter, int status){
	int code = IO_DEFAULT;
	if (strcmp(device, "PANTALLA") == 0) {
        code = status == REQUEST ? IO_SCREEN : IO_SCREEN_RESPONSE;
    } else if(strcmp(device, "TECLADO") == 0) {
		code = status == REQUEST ? IO_KEYBOARD : IO_KEYBOARD_RESPONSE;
	} 

	t_package* package = create_package(code); //code | pcb | tamaño | device | device | parameter 
    add_pcb_to_package(package, pcb);

    add_value_to_package(package, device, string_length(device) + 1);

	if (strcmp(device, "PANTALLA") == 0 || strcmp(device, "TECLADO") == 0) {
        add_value_to_package(package, parameter, string_length(parameter) + 1);
    } else {
		add_int_to_package(package, atoi(parameter));
	}

    send_package(package, client_socket);
	
	delete_package(package);
}
