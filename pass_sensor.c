#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso de sensor: %s <rango_min> <rango_max> <salida> [segundos]\n", argv[0]);
        return 1;
    }

    int rango_min = atoi(argv[1]);
    int rango_max = atoi(argv[2]);
    int salida_int = atoi(argv[3]);
    bool salida_bool = (salida_int !=0);
    int segundos = 2;
    if (argc>4) { //El intervalo de llegada de personas es opcional
        segundos = atoi(argv[4]);
    }

    if (rango_min >= rango_max){
        fprintf(stderr,"EL rango minimo debe ser menor que el rango maximo\n");
        return 1;
    }

    FILE *archivo = fopen("npipe", "a"); //Se abre el archivo para escribir
    if (archivo == NULL) {
        perror("No se pudo abrir el archivo");
        return 1;
    }

    srand(time(NULL));

    while (1) {
        int numero1 = rand() % (rango_max - rango_min + 1) + rango_min;  // Generar número de piso aleatorio
        int numero2 = rand() % (rango_max - rango_min + 1) + rango_min; // Generar número de piso aleatorio
        
        while(numero2==numero1){ //Valida que no sea el mismo numero
            numero2 = rand() % (rango_max - rango_min + 1) + rango_min;
        }

        fprintf(archivo, "%d-%d\n", numero1, numero2);  // Escribir en el archivo
        fflush(archivo);  // Forzar la escritura en el archivo
        
        sleep(segundos);  // Esperar la cantidad de segundos especificada
    }
    printf("Se cierra el archivo de escritura");
    fclose(archivo);
    return 0;
}
