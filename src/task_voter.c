/**********************************************************************
* FILENAME : task_monitor.c       
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

// libc
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

// freerqtos
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// esp
#include <esp_system.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_timer.h>

// propias
#include "config.h"

static const char *TAG = "STF_P1:task_voter";


// Tarea Votador
SYSTEM_TASK(TASK_VOTER)
{
	TASK_BEGIN();
	ESP_LOGI(TAG,"Task Voter running");

	// Recibe los argumentos de configuración de la tarea y los desempaqueta
	task_voter_args_t* ptr_args = (task_voter_args_t*) TASK_ARGS;
	RingbufHandle_t* voter_ring_buffer = ptr_args->voter_ring_buffer; 
	RingbufHandle_t* monitor_ring_buffer = ptr_args->monitor_ring_buffer; 

	// variables para reutilizar en el bucle
	size_t length;
	void *ptr;
	therm_data_t thermistor_a_data, thermistor_b_data, thermistor_c_data;
	bool thermistor_a_received = false;
	bool thermistor_b_received = false;
	bool thermistor_c_received = false;

	// Loop
	TASK_LOOP()
	{
		ptr = xRingbufferReceive(*voter_ring_buffer, &length, pdMS_TO_TICKS(1000));

		if (ptr != NULL) 
		{
			therm_data_t *received_data = (therm_data_t *) ptr;
			if (received_data->source == THERMISTOR_A) {
				thermistor_a_data = *received_data;
				thermistor_a_received = true;
			} else if (received_data->source == THERMISTOR_B) {
				thermistor_b_data = *received_data;
				thermistor_b_received = true;
			} else if (received_data->source == THERMISTOR_C) {
                thermistor_c_data = *received_data;
				thermistor_c_received = true;
            }
            
			vRingbufferReturnItem(*voter_ring_buffer, ptr);
		} 
		else 
		{
			ESP_LOGW(TAG, "Esperando datos ...");
		}

		if (thermistor_a_received && thermistor_b_received && thermistor_c_received) {
            float mean = (thermistor_a_data.value + thermistor_b_data.value + thermistor_c_data.value)/3;

			therm_data_t mean_data;
			mean_data.source = VOTER;
			mean_data.value = mean;

			if (xRingbufferSendAcquire(*monitor_ring_buffer, &ptr, sizeof(mean_data), pdMS_TO_TICKS(100)) != pdTRUE)
			{
				ESP_LOGI(TAG,"Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*monitor_ring_buffer));
			}
			else 
			{
				memcpy(ptr, &mean_data, sizeof(mean_data));
				xRingbufferSendComplete(*monitor_ring_buffer, ptr);

			}

			thermistor_a_received = false;
			thermistor_b_received = false;
			thermistor_c_received = false;
		}
	}
	ESP_LOGI(TAG,"Deteniendo la tarea ...");
	TASK_END();
}
