# tp-2022-2c-The-Kings
[Link a la Consigna]([https://docs.google.com/document/d/1BDpr5lfzOAqmOOgcAVg6rUqvMPUfCpMSz1u1J_Vjtac/edit](https://docs.google.com/document/d/1xYmkJXRRddM51fQZfxr3CEuhNtFCWe5YU7hhvsUnTtg/edit#heading=h.nm1zk6pu7e78))

## Vistazo general
Sistema distribuído concurrente que simula algunos aspectos del funcionamiento de un S.O de libro como visto en el Stallings o el Silberschatz.

Cuenta con 4 procesos principales:`Consola` ,`Kernel`,`CPU` , `Memoria` y `Swap`. Los cuales pueden ejecutarse en diferentes direcciones de red.

A través del modulo Consola los clientes pueden conectarse por red y hacer uso de los siguientes servicios:


### Funcionamiento general (Kernel):
- El modulo Kernel simula un planificador con grado de  multiprogramacion N.
- Los clientes conectados al modulo kernel son tratados como procesos a ser planificados.
- Algoritmos de planificación FIFO, ROUND ROBIN , FEEDBACK MULTINIVEL.
- En caso de ROUND ROBIN y FEEDBACK MULTINIVEL. Hay desalojo forzoso, un proceso deja de ejecutar por el quantum definido.


### Funcionamiento general (CPU):
- El módulo CPU es el encargado de interpretar y ejecutar las instrucciones de los PCB recibidos por parte del Kernel. Simulando un ciclo de instrucción simplificado (Fetch, Decode, Execute y Check Interrupt).


### Funcionamiento general (Memoria y Swap):
El modulo Memoria implementa un esquema de segmentación paginada, con una tabla de páginas por cada segmento que tenga un proceso.

- El modulo Memoria cuenta con una TLB, con algoritmos de reeplazo de entradas FIFO o LRU.
- En caso de estar llena la memoria principal, esta mueve páginas al módulo Swap.
- Criterios de asignación de frames: Tamaño fijo con reemplazo local.
- Algoritmos de selección de frame para reemplazos: Clock-Modificado o LRU.
