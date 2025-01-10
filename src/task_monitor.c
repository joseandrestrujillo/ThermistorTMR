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

static const char *TAG = "STF_P1:task_monitor";

// Tarea MONITOR
SYSTEM_TASK(TASK_MONITOR)
{
	TASK_BEGIN();
	ESP_LOGI(TAG,"Task Monitor running");

	// Recibe los argumentos de configuración de la tarea y los desempaqueta
	task_monitor_args_t* ptr_args = (task_monitor_args_t*) TASK_ARGS;
	RingbufHandle_t* monitor_ring_buffer = ptr_args->monitor_ring_buffer;

	// variables para reutilizar en el bucle
	size_t length;
	void *ptr;

	// Loop
	TASK_LOOP()
	{
        ptr = xRingbufferReceive(*monitor_ring_buffer, &length, pdMS_TO_TICKS(1000));

        if (ptr != NULL) 
        {
            therm_data_t *received_data = (therm_data_t *) ptr;
            ESP_LOGI(TAG, "NORMAL_MODE: T = (%.5f) ºC", received_data->value);
            vRingbufferReturnItem(*monitor_ring_buffer, ptr);
        } 
        else 
        {
            ESP_LOGW(TAG, "Esperando datos ...");
        }
	}
	ESP_LOGI(TAG,"Deteniendo la tarea ...");
	TASK_END();
}
