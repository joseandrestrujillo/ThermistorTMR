/******************************************************************************
* FILENAME : config.h
*
* DESCRIPTION : 
*
* PUBLIC LICENSE :
* Este código es de uso público y libre de modificar bajo los términos de la
* Licencia Pública General GNU (GPL v3) o posterior. Se proporciona "tal cual",
* sin garantías de ningún tipo.
*
* AUTHOR :   Dr. Fernando Leon (fernando.leon@uco.es) University of Cordoba
******************************************************************************/

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdbool.h>

// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

// esp
#include <hal/adc_types.h>

// propias
#include "system.h"

// Abstracciones para facilitar la legibilidad
#define CORE0 0
#define CORE1 1

// Configuraciones y constantes

// Nombre y estados de la máquina
#define SYS_NAME "STF P1 System"
enum{
	INIT,
	NORMAL_MODE
};

// Configuración de los termistores

#define THERMISTOR_ADC_UNIT ADC_UNIT_1
#define THERMISTOR_A_ADC_CHANNEL ADC_CHANNEL_6 // GPIO34
#define THERMISTOR_B_ADC_CHANNEL ADC_CHANNEL_7 // GPIO35
#define THERMISTOR_C_ADC_CHANNEL ADC_CHANNEL_8 // GPIO25


#define THERMISTOR_A_ADC_CHANNEL_POWER_GPIO GPIO_NUM_33
#define THERMISTOR_B_ADC_CHANNEL_POWER_GPIO GPIO_NUM_32
#define THERMISTOR_C_ADC_CHANNEL_POWER_GPIO GPIO_NUM_26


#define SERIES_RESISTANCE 10000       // 10K ohms
#define NOMINAL_RESISTANCE 10000      // 10K ohms
#define NOMINAL_TEMPERATURE 298.15    // 25°C en Kelvin
#define BETA_COEFFICIENT 3950         // Constante B (ajustar según el termistor)

// Configuración del buffer cíclico
#define BUFFER_SIZE  2048
#define BUFFER_TYPE  RINGBUF_TYPE_NOSPLIT

typedef enum {
	THERMISTOR_A,
	THERMISTOR_B,
	THERMISTOR_C
} therm_data_source_t;

typedef struct {
    therm_data_source_t source;
    float value;
} therm_data_t;

// Configuración de las tareas

// SENSOR
// Tarea sensor
SYSTEM_TASK(TASK_SENSOR);
#define CHECK_INTERVAL_CYCLES 2
// definición de los argumentos que requiere la tarea
typedef struct 
{
	RingbufHandle_t* monitor_ring_buffer; // puntero al buffer del monitor 
	uint8_t freq;          // frecuencia de muestreo
    // ...
}task_sensor_args_t;
// Timeout de la tarea (ver system_task_stop)
#define TASK_SENSOR_TIMEOUT_MS 2000 
// Tamaño de la pila de la tarea
#define TASK_SENSOR_STACK_SIZE 4096


// MONITOR
SYSTEM_TASK(TASK_MONITOR);
// definición de los argumentos que requiere la tarea
typedef struct 
{
	RingbufHandle_t* monitor_ring_buffer; // puntero al buffer 
}task_monitor_args_t;
// Timeout de la tarea (ver system_task_stop)
#define TASK_MONITOR_TIMEOUT_MS 2000 
// Tamaño de la pila de la tarea
#define TASK_MONITOR_STACK_SIZE 4096
#endif