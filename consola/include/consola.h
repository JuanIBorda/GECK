#ifndef CONSOLA_H
#define CONSOLA_H

#include <stdio.h>
#include <stdbool.h>
#include <commons/log.h>

#include <sys/stat.h>

#include "shared.h"
//#include "network.h"
//#include "serialize.h"

t_log* logger;
t_package* package;

char* readFile(char* path);
char** parseLines(char* buffer);
void send_process_package(int connection, char** instructions, char** segments);
void show_on_screen_pcb_register(t_pcb* pcb, char* parameter);

/* ----- de configuracion :P */
t_config* init_config(char*);

#endif