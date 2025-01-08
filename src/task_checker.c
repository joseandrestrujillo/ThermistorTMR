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
	RingbufHandle_t* monitor_ring_buffer = ptr_args->monitor_ring_buffer; 
	RingbufHandle_t* checker_ring_buffer = ptr_args->checker_ring_buffer; 

	// variables para reutilizar en el bucle
	size_t length;
	void *ptr;
	therm_data_t main_data, replica_data;
	bool main_received = false;
	bool replica_received = false;

	// Loop
	TASK_LOOP()
	{
		ptr = xRingbufferReceive(*checker_ring_buffer, &length, pdMS_TO_TICKS(1000));

		if (ptr != NULL) 
		{
			therm_data_t *received_data = (therm_data_t *) ptr;
			if (received_data->source == MAIN) {
				main_data = *received_data;
				main_received = true;
			} else if (received_data->source == REPLICA) {
				replica_data = *received_data;
				replica_received = true;
			}
			vRingbufferReturnItem(*checker_ring_buffer, ptr);
		} 
		else 
		{
			ESP_LOGW(TAG, "Esperando datos ...");
		}

		if (main_received && replica_received) {
			float deviation_percentage = 0.0f;
			if (main_data.value != 0) {
				deviation_percentage = ((replica_data.value - main_data.value) / main_data.value);
			}

			therm_data_t deviation_data;
			deviation_data.source = DEVIATION;
			deviation_data.value = deviation_percentage;

			if (xRingbufferSendAcquire(*monitor_ring_buffer, &ptr, sizeof(deviation_data), pdMS_TO_TICKS(100)) != pdTRUE)
			{
				ESP_LOGI(TAG,"Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*monitor_ring_buffer));
			}
			else 
			{
				memcpy(ptr, &deviation_data, sizeof(deviation_data));
				xRingbufferSendComplete(*monitor_ring_buffer, ptr);

			}

			main_received = false;
			replica_received = false;

			uint8_t current_state = GET_ST_FROM_TASK();

			if (
				should_transition_from_normal_mode_to_error(current_state, deviation_percentage) 
				|| should_transition_from_degraded_mode_to_error(current_state, deviation_percentage))
			{
				SWITCH_ST_FROM_TASK(ERROR);
			} 
			else if (should_transition_from_normal_to_degraded_mode(current_state, deviation_percentage))
			{
				SWITCH_ST_FROM_TASK(DEGRADED_MODE);
			}
			else if (should_transition_from_degraded_to_normal_mode(current_state, deviation_percentage))
			{
				SWITCH_ST_FROM_TASK(NORMAL_MODE);
			}
		}
	}
	ESP_LOGI(TAG,"Deteniendo la tarea ...");
	TASK_END();
}
