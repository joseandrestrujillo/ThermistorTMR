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
#include <math.h>

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

float voltage_to_temperature(float v) {
    float r_ntc = SERIES_RESISTANCE * (3.3f - v) / v;

    float t_kelvin = 1.0f / (1.0f / NOMINAL_TEMPERATURE + (1.0f / BETA_COEFFICIENT) * log(r_ntc / NOMINAL_RESISTANCE));
    
    return t_kelvin - 273.15f;
}

float lsb_to_voltage(uint16_t lsb) {
    return (float) ((lsb) * 3.3f / 4095.0f);
}

// Tarea MONITOR
SYSTEM_TASK(TASK_MONITOR)
{
	TASK_BEGIN();
	ESP_LOGI(TAG,"Task Monitor running");

	// Recibe los argumentos de configuración de la tarea y los desempaqueta
	task_monitor_args_t* ptr_args = (task_monitor_args_t*) TASK_ARGS;
	RingbufHandle_t* monitor_ring_buffer = ptr_args->monitor_ring_buffer; 
	system_t* system_state_machine = ptr_args->system_state_machine; 
	system_task_t* self_task = ptr_args->self_task; 

	// variables para reutilizar en el bucle
	size_t length;
	void *ptr;

	// Loop
	TASK_LOOP()
	{
        uint8_t current_state = GET_ST_FROM_TASK();

        if(current_state == INIT)
        {
            vTaskDelay(100);
            return;
        }
        if(current_state == ERROR)
        {
            ESP_LOGI(TAG, "Sensor ERROR. Repare and restart.");
            system_task_stop(system_state_machine, self_task, TASK_SENSOR_TIMEOUT_MS);

        }

        ptr = xRingbufferReceive(*monitor_ring_buffer, &length, pdMS_TO_TICKS(1000));

        if (ptr != NULL) 
        {
            uint16_t * received_data = (uint16_t *) ptr;
            float v_mean = lsb_to_voltage(*received_data);
			float t_mean = voltage_to_temperature(v_mean);
            if (current_state == NORMAL_MODE) {
                ESP_LOGI(TAG, "NORMAL_MODE: T = (%.5f) ºC", t_mean);
            } else if (current_state == DEGRADED_MODE)
            {
                thermistor_t degraded_thermistor = GET_DEGRADED_THERMISTOR();
                const char *therm_data_source_string[] = {
                    "THERMISTOR_A",
                    "THERMISTOR_B",
                    "THERMISTOR_C",
                    "NONE"
                };
                ESP_LOGI(TAG, "DEGRADED_MODE: T = (%.5f) ºC", t_mean);
                ESP_LOGE(TAG, "El termistor %s debe ser cambiado", therm_data_source_string[degraded_thermistor]);
            }
            
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
