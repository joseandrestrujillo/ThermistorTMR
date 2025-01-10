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
	uint16_t mask = ptr_args->mask; 

	// variables para reutilizar en el bucle
	size_t length;
	void *ptr;

	uint16_t * thermistor_reads;

	// Loop
	TASK_LOOP()
	{
		ptr = xRingbufferReceive(*voter_ring_buffer, &length, pdMS_TO_TICKS(1000));

		if (ptr != NULL) 
		{
			thermistor_reads = (uint16_t *) ptr;
			vRingbufferReturnItem(*voter_ring_buffer, ptr);

			uint16_t a = thermistor_reads[0] & mask;
			uint16_t b = thermistor_reads[1] & mask;
			uint16_t c = thermistor_reads[2] & mask;

			uint16_t lsb_mean = (a & b) | (a & c) | (b & c);

			if (xRingbufferSendAcquire(*monitor_ring_buffer, &ptr, sizeof(uint16_t), pdMS_TO_TICKS(100)) != pdTRUE)
			{
				ESP_LOGI(TAG,"Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*monitor_ring_buffer));
			}
			else 
			{
				memcpy(ptr, &lsb_mean, sizeof(uint16_t));
				xRingbufferSendComplete(*monitor_ring_buffer, ptr);

			}
		} 
		else 
		{
			ESP_LOGW(TAG, "Esperando datos ...");
		}
	}
	ESP_LOGI(TAG,"Deteniendo la tarea ...");
	TASK_END();
}
