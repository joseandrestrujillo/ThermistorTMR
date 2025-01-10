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

void app_main(void)
{
	system_t sys_stf_p1;
	ESP_LOGI(TAG,"Starting STF_P1 system");
	system_create(&sys_stf_p1, SYS_NAME);
	system_register_state(&sys_stf_p1, INIT);
	system_register_state(&sys_stf_p1, NORMAL_MODE);
	system_set_default_state(&sys_stf_p1, INIT);

	system_task_t task_sensor;
	system_task_t task_monitor;
	system_task_t task_voter;

	RingbufHandle_t voter_ring_buffer;
	voter_ring_buffer = xRingbufferCreate(BUFFER_SIZE, BUFFER_TYPE);

	RingbufHandle_t monitor_ring_buffer;
	monitor_ring_buffer = xRingbufferCreate(BUFFER_SIZE, BUFFER_TYPE);

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

            ESP_LOGI(TAG, "starting sensor task...");
            task_sensor_args_t task_sensor_args = {&voter_ring_buffer, 1};
			system_task_start_in_core(&sys_stf_p1, &task_sensor, TASK_SENSOR, "TASK_SENSOR", TASK_SENSOR_STACK_SIZE, &task_sensor_args, 0, CORE0);
			ESP_LOGI(TAG, "Done");

			vTaskDelay(pdMS_TO_TICKS(1000));

			ESP_LOGI(TAG, "starting voter task...");
			task_voter_args_t task_voter_args = {&voter_ring_buffer, &monitor_ring_buffer, MASK};
			system_task_start_in_core(&sys_stf_p1, &task_voter, TASK_VOTER, "TASK_VOTER", TASK_VOTER_STACK_SIZE, &task_voter_args, 0, CORE1);
			ESP_LOGI(TAG, "Done");

			ESP_LOGI(TAG, "starting monitor task...");
			task_monitor_args_t task_monitor_args = {&monitor_ring_buffer};
			system_task_start_in_core(&sys_stf_p1, &task_monitor, TASK_MONITOR, "TASK_MONITOR", TASK_MONITOR_STACK_SIZE, &task_monitor_args, 0, CORE1);
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
		STATE_MACHINE_END();
	}
}
