#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>

//######################### SECCION ESTRUCTURAS ############################

#define NUM_ASCENSORES 2 //Cantidad de ascensores constante

typedef struct { //Estructura que define un pasajero
    int origen;
    int destino;
    int indice;
} Pasajero;

typedef struct { //Estructura que define un ascensor
    int id;
    int pisoActual;
    int direccion;
    int cantPasajeros;
    int capacidad;
    pthread_mutex_t mutex;
    Pasajero *pasajeros;
} Ascensor;

typedef struct { //Estructura para definir la cola de pasajeros
    Pasajero *elementos;
    int inicio;
    int final;
    int cantidad;
    pthread_mutex_t mutex;
    pthread_cond_t cond_vacio;
} ColaPasajeros;

Ascensor ascensores[NUM_ASCENSORES]; //Ascensores totales de la implementacion
//ColaPasajeros colaPasajeros;
int *pasajeros_esperando;
char *seg_asc = "2";
char *pisos;
bool salida_bool = true;
int fd[2]; // Array para almacenar los descriptores de la tubería de mensaje
pthread_mutex_t p_mutex = PTHREAD_MUTEX_INITIALIZER;

//###########################################################################

//######################### SECCION INICIALIZADORES ############################

void iniciar_escritor(){
    pid_t pid;

    if (pipe(fd) == -1) {
        perror("Error al crear la tubería");
        exit(1);
    }

    pid = fork(); 

    if (pid == -1) {
        perror("Error al crear el proceso hijo");
        exit(1);
    }

    if (pid == 0) { // Código del proceso hijo
        close(fd[1]); // Cerramos el extremo de escritura de la tubería en el proceso hijo

        char mensaje[1024];
        int bytes_leidos;

        while ((bytes_leidos = read(fd[0], mensaje, sizeof(mensaje))) > 0) {
            mensaje[bytes_leidos] = '\0'; // Añadimos un terminador nulo al final
            system("clear");
            printf("%s\n", mensaje);
        }

        close(fd[0]);
        exit(0);
    } else { 
        close(fd[0]);
    }
}

void inicializar_ascensores(int capacidad_ascensor){ //Inicializacion de ascensores
    for(int i=0;i<NUM_ASCENSORES;i++){
        ascensores[i].id=i;
        ascensores[i].pisoActual=0;
        ascensores[i].direccion=1;
        ascensores[i].cantPasajeros=0;
        ascensores[i].capacidad = capacidad_ascensor;
        ascensores[i].pasajeros = malloc(sizeof(Pasajero)*capacidad_ascensor);
        pthread_mutex_init(&ascensores[i].mutex,NULL);
    }
}

//#####################################################################

//##################### SECCION HILO LECTURA ##########################

void cargar_pasajero(Ascensor *ascensor, Pasajero *pasajero){ //Metodo para cargar un pasajero a un ascensor
    if(ascensor->cantPasajeros < ascensor->capacidad){
        ascensor->cantPasajeros++; 
        ascensor->pasajeros[ascensor->cantPasajeros]=*pasajero;
    }
}

void ubicar_ascensor(Pasajero *pasajero){ //Metodo para ubicar el mejor ascensor a un pasajero
    int ascensor_elegido = -1;
    int tiempo_minimo = INT_MAX;

    for(int i=0; i<NUM_ASCENSORES;i++){
        Ascensor *ascensor = &ascensores[i];
        
        if((ascensor->direccion == 1 && pasajero->origen>=ascensor->pisoActual) ||
           (ascensor->direccion == -1 && pasajero->origen <= ascensor->pisoActual)){ //Validacion de direccion
            int tiempo_llegada = abs(pasajero->origen - ascensor->pisoActual) * atoi(seg_asc);
            
            if(tiempo_llegada < tiempo_minimo && ascensor->cantPasajeros < ascensor->capacidad){ //Validacion de capacidad y el menor tiempo de espera
                tiempo_minimo = tiempo_llegada;
                ascensor_elegido = i;
            }
        }else {
            ascensor_elegido = 0;        
        }
    }

    if(ascensor_elegido != -1){ //Cargar pasajero al ascensor
        pthread_mutex_lock(&ascensores[ascensor_elegido].mutex);
        cargar_pasajero(&ascensores[ascensor_elegido],pasajero);
        pthread_mutex_unlock(&ascensores[ascensor_elegido].mutex);    
    }
    
}

void *lecturaSensores(void *arg) { //Metodo que ejecuta el hilo de la lectura de los sensores
    char mensaje[1024] = "";
    int contador = 0;
    while (1) {
        FILE *archivo = fopen("npipe", "r");

        if (archivo == NULL) {
            perror("No se pudo abrir el archivo");
            return NULL;
        }
        char linea[1024];
        for(int i=0;i<contador;i++){
            fgets(linea, sizeof(linea), archivo);
        } 
        while (fgets(linea, sizeof(linea), archivo) != NULL) {
            int orig,dest;
            if(sscanf(linea,"%d-%d\n",&orig,&dest)==2){
                Pasajero pasajero;
                pasajero.origen=orig;
                pasajero.destino=dest;
                ubicar_ascensor(&pasajero);
                pthread_mutex_lock(&p_mutex);
                pasajeros_esperando[pasajero.origen]++;
                pthread_mutex_unlock(&p_mutex);
                contador++;
            }
        }
        fclose(archivo);
        char piso_m[70] = "";
        for(int i=0;i<atoi(pisos);i++){
            sprintf(piso_m, "Piso %d: %d pasajeros esperando\n",i, *pasajeros_esperando);
            strcat(mensaje,piso_m);
        }        
        write(fd[1],mensaje,strlen(mensaje));
        sprintf(mensaje,"contador: %d\n",contador);
        write(fd[1],mensaje,strlen(mensaje)); 
        
        sleep(1);       
    }

    return NULL;
}

//###############################################################

//###################### SECCION HILO ASCENSOR ##################

void mover_ascensor(Ascensor *ascensor) { //Metodo para mover el ascensor
    int cont_bajan = 0;
    char mensaje[256] = "";
    // Bloquear el mutex para acceder a los datos del ascensor
    pthread_mutex_lock(&ascensor->mutex);

    // Determinar si hay pasajeros en la dirección actual
    int hay_pasajeros_en_direccion = 0;
    for (int i = 0; i < ascensor->cantPasajeros; i++) {
        if ((ascensor->direccion == 1 && ascensor->pasajeros[i].destino > ascensor->pisoActual) ||
            (ascensor->direccion == -1 && ascensor->pasajeros[i].destino < ascensor->pisoActual)) {
            hay_pasajeros_en_direccion = 1;
            break;
        }
    }

    // Si no hay pasajeros en la dirección actual, cambiar la dirección
    if (!hay_pasajeros_en_direccion) {
        ascensor->direccion = -ascensor->direccion;
    }

    // Mover el ascensor en la dirección actual
    ascensor->pisoActual += ascensor->direccion;

    // Descargar a los pasajeros cuyo piso de destino es el actual
    for (int i = 0; i < ascensor->cantPasajeros; i++) {
        if (ascensor->pasajeros[i].destino == ascensor->pisoActual) {
            // Descargar pasajero
            ascensor->cantPasajeros--;
            for (int j = i; j < ascensor->cantPasajeros; j++) {
                ascensor->pasajeros[j] = ascensor->pasajeros[j + 1];
            }
            pthread_mutex_lock(&p_mutex);
            pasajeros_esperando[ascensor->pisoActual]+= -1;
            pthread_mutex_unlock(&p_mutex);
            i--; // Retroceder el índice para el próximo pasajero
            cont_bajan++;
        }
    }
    if(cont_bajan>0){
        sprintf(mensaje,"Ascensor %d - Se bajaron %d pasajeros en el piso: %d\n",ascensor->id,cont_bajan,ascensor->pisoActual);
    }    
    write(fd[1],mensaje,strlen(mensaje));

    // Liberar el mutex
    pthread_mutex_unlock(&ascensor->mutex);
}

void *hiloAscensor(void *arg){ //Metodo que ejecuta el hilo de cada ascensor
    Ascensor* ascensor = (Ascensor*)arg;
    char mensaje[100] = "";
            
    while(1){
        if(ascensor->cantPasajeros>0){
            sprintf(mensaje,"Pasajeros ascensor %d: %d\n",ascensor->id,ascensor->cantPasajeros);
            write(fd[1],mensaje,strlen(mensaje));
            mover_ascensor(ascensor);        
        }
    }
}

//################################################################

// ###################### SECCION HILO PEDIDOS ###################

void *pedidosAscensor(void *arg) { //Metodo que ejecuta el hilo de los pedidos de personas
    char **args = (char **)arg;
    
    execvp(args[0], args); 
    perror("Error al ejecutar el programa pedidos");   
    return NULL;
}

//###############################################################

//####################### SECCION PRINCIPAL ####################

int main(int argc, char *argv[]) { //Metodo principal
    pthread_t threads_ascensores[NUM_ASCENSORES]; //Hilos para los ascensores
    pthread_t peticiones; //Hilo para crear peticiones de personas
    pthread_t lectura; //Hilo para lectura de peticiones
    
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <pisos> <pasajeros> <salida> [segundos_traslado] [segundos_personas]\n", argv[0]);
        return 1;
    }
    
    pisos = argv[1];
    int max_pasajeros = atoi(argv[2]);
    char *salida_c = argv[3];
    salida_bool = (atoi(salida_c) !=0);
    char *seg_per = "2";
    if (argc>4) { //El intervalo de traslado de los ascensores es opcional
        seg_asc = argv[4];
    }
    if (argc==6) { //El intervalo de llegada de personas es opcional
        seg_per = argv[5];
    }
    pasajeros_esperando = malloc(sizeof(int)*(atoi(pisos)+1));
    char *args[] = {"./sensor","0",pisos,salida_c,seg_per,NULL};
    inicializar_ascensores(max_pasajeros);
    for(int i=0;i<NUM_ASCENSORES;i++){ 
        if (pthread_create(&threads_ascensores[i], NULL, hiloAscensor, &ascensores[i]) != 0) { //Se ejecuta el hilo de peticiones de ascensor
            perror("Error al crear el hilo de ascensor");
            return 1;
        }    
    }
    if (pthread_create(&peticiones, NULL, pedidosAscensor, (void *)args) != 0) { //Se ejecuta el hilo de peticiones de ascensor
        perror("Error al crear el hilo de pedidos de personas");
        return 1;
    }
    if (pthread_create(&lectura, NULL, lecturaSensores, NULL) != 0) { //Se ejecuta el hilo de peticiones de ascensor
        perror("Error al crear el hilo de pedidos de personas");
        return 1;
    }

    pthread_join(peticiones, NULL);  // Espera a que el hilo termine
    pthread_join(lectura, NULL);  // Espera a que el hilo termine   
    for(int i=0;i<NUM_ASCENSORES;i++){ 
        pthread_join(threads_ascensores[i], NULL); // Espera a que el hilo termine
    }
    for(int i=0; i<NUM_ASCENSORES;i++){ //Termina los mutex de los ascensores
        free(ascensores[i].pasajeros);
    pthread_mutex_destroy(&ascensores[i].mutex);
    }

    return 0;
}

//#######################################################################
