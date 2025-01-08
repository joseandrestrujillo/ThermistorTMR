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

static const char *TAG = "STF_P1:task_checker";


// Tarea MONITOR
SYSTEM_TASK(TASK_CHECKER)
{
	TASK_BEGIN();
	ESP_LOGI(TAG,"Task Checker running");

	// Recibe los argumentos de configuración de la tarea y los desempaqueta
	task_checker_args_t* ptr_args = (task_checker_args_t*) TASK_ARGS;
	RingbufHandle_t* checker_ring_buffer = ptr_args->checker_ring_buffer; 
	uint16_t mask = ptr_args->mask; 

	// variables para reutilizar en el bucle
	size_t length;
	void *ptr;

	// Loop
	TASK_LOOP()
	{
		ptr = xRingbufferReceive(*checker_ring_buffer, &length, pdMS_TO_TICKS(1000));

		if (ptr != NULL) 
		{
			uint16_t * thermistor_reads = (uint16_t *) ptr;

			uint16_t masked_a = thermistor_reads[0] & mask;
			uint16_t masked_b = thermistor_reads[1] & mask;
			uint16_t masked_c = thermistor_reads[2] & mask;

			if (masked_a == masked_b && masked_a == masked_c) {
				SWITCH_ST_FROM_TASK(NORMAL_MODE);
				SET_DEGRADED_THERMISTOR(THERMISTOR_NONE);
			} else if (masked_a == masked_b) {
				SET_DEGRADED_THERMISTOR(THERMISTOR_C);
				SWITCH_ST_FROM_TASK(DEGRADED_MODE);
			} else if (masked_a == masked_c) {
				SET_DEGRADED_THERMISTOR(THERMISTOR_B);
				SWITCH_ST_FROM_TASK(DEGRADED_MODE);
			} else if (masked_b == masked_c) {
				SET_DEGRADED_THERMISTOR(THERMISTOR_A);
				SWITCH_ST_FROM_TASK(DEGRADED_MODE);
			} else {
				SWITCH_ST_FROM_TASK(ERROR);
			}

			vRingbufferReturnItem(*checker_ring_buffer, ptr);
		} 
		else 
		{
			ESP_LOGW(TAG, "Esperando datos ...");
		}
	}
	ESP_LOGI(TAG,"Deteniendo la tarea ...");
	TASK_END();
}
