# SistemaAscensores
Comandos para compilar
gcc pass_sensor.c -o sensor
gcc main.c -o main

Comandos para ejecutar
./main <pisos> <max_pasajeros> <salida 1=T 0=F> [segndos_traslado] [segundos_intervalo_llegada]

ejemplo: ./main 4 3 1 1 5