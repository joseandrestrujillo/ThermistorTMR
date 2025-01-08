/******************************************************************************
* FILENAME : main.c
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
#include <stdio.h>
#include <assert.h>

// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>

// esp-idf
#include <esp_log.h>
#include <esp_event.h>
#include <nvs_flash.h>

// propias
#include "config.h"
#include "system.h"

static const char *TAG = "STF_P1:main";

// Punto de entrada
void app_main(void)
{
	system_t sys_stf_p1;
	ESP_LOGI(TAG,"Starting STF_P1 system");
	system_create(&sys_stf_p1, SYS_NAME);
	system_register_state(&sys_stf_p1, INIT);
	system_register_state(&sys_stf_p1, NORMAL_MODE);
	system_register_state(&sys_stf_p1, DEGRADED_MODE);
	system_register_state(&sys_stf_p1, ERROR);
	system_set_default_state(&sys_stf_p1, INIT);


	// Define manejadores de tareas (de momento sin asignar)
	system_task_t task_sensor;
	system_task_t task_monitor;
	system_task_t task_checker;

	// Define y crea un buffer cíclico (ver documentación de ESP-IDF)
	// a modo de buffer thread-safe entre tareas. 

	// Buffer cíclico para el monitor
	RingbufHandle_t monitor_ring_buffer;
	monitor_ring_buffer = xRingbufferCreate(BUFFER_SIZE, BUFFER_TYPE);


	// Buffer cíclico para el comprobador
	RingbufHandle_t checker_ring_buffer;
	checker_ring_buffer = xRingbufferCreate(BUFFER_SIZE, BUFFER_TYPE);

	// variable para códigos de retorno 
	esp_err_t ret;

	STATE_MACHINE(sys_stf_p1) 
	{
		STATE_MACHINE_BEGIN();
		STATE(INIT)
		{
			STATE_BEGIN();
			ESP_LOGI(TAG, "State: INIT");
 
            ret = nvs_flash_init();
            if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
            {
                ESP_ERROR_CHECK(nvs_flash_erase());
                ESP_ERROR_CHECK(nvs_flash_init());
            }

			// Crea la tarea sensor como un proceso asociado al CORE 0. 
			// Lo que hace la tarea está en task_sensor.h
            ESP_LOGI(TAG, "starting sensor task...");
            task_sensor_args_t task_sensor_args = {&monitor_ring_buffer, &checker_ring_buffer, 1, CHECK_INTERVAL_CYCLES};
			system_task_start_in_core(&sys_stf_p1, &task_sensor, TASK_SENSOR, "TASK_SENSOR", TASK_SENSOR_STACK_SIZE, &task_sensor_args, 0, CORE0);
			ESP_LOGI(TAG, "Done");

			// Delay
			vTaskDelay(pdMS_TO_TICKS(1000));

			// Crea la tarea monitor como un proceso asociado al CORE 1.
			// Lo que hace la tarea está en task_monitor.c
			ESP_LOGI(TAG, "starting monitor task...");
			task_monitor_args_t task_monitor_args = {&monitor_ring_buffer, &sys_stf_p1, &task_monitor};
			system_task_start_in_core(&sys_stf_p1, &task_monitor, TASK_MONITOR, "TASK_MONITOR", TASK_MONITOR_STACK_SIZE, &task_monitor_args, 0, CORE1);
			ESP_LOGI(TAG, "Done");

			// Tarea Comprobador
			ESP_LOGI(TAG, "starting checker task...");
			task_checker_args_t task_checker_args = {&monitor_ring_buffer, &checker_ring_buffer};
			system_task_start_in_core(&sys_stf_p1, &task_checker, TASK_CHECKER, "TASK_CHECKER", TASK_CHECKER_STACK_SIZE, &task_checker_args, 0, CORE1);
			ESP_LOGI(TAG, "Done");

			SWITCH_ST(&sys_stf_p1, NORMAL_MODE);
			STATE_END();
		}
		STATE(NORMAL_MODE)
		{
			STATE_BEGIN();
			ESP_LOGI(TAG, "State: NORMAL_MODE");
			STATE_END();
		}
		STATE(DEGRADED_MODE)
		{
			STATE_BEGIN();
			ESP_LOGI(TAG, "State: DEGRADED_MODE");
			STATE_END();
		}
		STATE(ERROR)
		{
			STATE_BEGIN();
			system_task_stop(&sys_stf_p1, &task_sensor, TASK_SENSOR_TIMEOUT_MS);
			system_task_stop(&sys_stf_p1, &task_checker, TASK_CHECKER_TIMEOUT_MS);
			ESP_LOGI(TAG, "State: ERROR");
			STATE_END();
		}
		STATE_MACHINE_END();
	}
}
