/**********************************************************************
* FILENAME : task_sensor.c       
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


/*
	Circuito del termistor 1. 

   3.3V
     |
     |
  [ NTC ]  <-- Termistor 10K 
     |
     |-----------> ADC IN (GPIO34 por defecto. Ver config.h)
     |
  [ 10K ]  <-- R fija 10K
     |
    GND
**/

// libc 
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include <string.h>

// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// espidf
#include <esp_system.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_adc/adc_oneshot.h>

// propias
#include "config.h"
#include "therm.h"

static const char *TAG = "STF_P1:task_sensor";

// esta tarea temporiza la lectura de un termistor. 
// Para implementar el periodo de muestreo, se utiliza un semáforo declarado de forma global 
// para poder ser utilizado desde esta rutina de expiración del timer.
// Cada vez que el temporizador expira, libera el semáforo
// para que la tarea realice una iteración. 
static SemaphoreHandle_t semSample = NULL;
static void tmrSampleCallback(void* arg)
{
	xSemaphoreGive(semSample);
}


// Tarea SENSOR
SYSTEM_TASK(TASK_SENSOR)
{	
	TASK_BEGIN();
	ESP_LOGI(TAG,"Task Sensor running");

	// Recibe los argumentos de configuración de la tarea y los desempaqueta
	task_sensor_args_t* ptr_args = (task_sensor_args_t*) TASK_ARGS;
	RingbufHandle_t* monitor_ring_buffer = ptr_args->monitor_ring_buffer; 
	RingbufHandle_t* checker_ring_buffer = ptr_args->checker_ring_buffer; 
	uint8_t frequency = ptr_args->freq;
	uint8_t check_interval_cycles = ptr_args->check_interval_cycles;
	uint64_t period_us = 1000000 / frequency;

	// Configuramos ambos termistores
	adc_oneshot_unit_handle_t adc_hdlr;
	adc_oneshot_unit_init_cfg_t adc1_default_cfg = {
		.unit_id = ADC_UNIT_1,
		.clk_src = ADC_RTC_CLK_SRC_DEFAULT,
	};

	// Inicializar el manejador ADC
	ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc1_default_cfg, &adc_hdlr));

	// Inicializar los termistores con el manejador
	therm_t main_thermistor = { .adc_hdlr = adc_hdlr };
	therm_t replica_thermistor = { .adc_hdlr = adc_hdlr };

	// Configurar los termistores
	ESP_ERROR_CHECK(therm_config(&main_thermistor, MAIN_THERMISTOR_ADC_CHANNEL, MAIN_THERMISTOR_ADC_CHANNEL_POWER_GPIO, NULL));
	ESP_ERROR_CHECK(therm_config(&replica_thermistor, REPLICA_THERMISTOR_ADC_CHANNEL, REPLICA_THERMISTOR_ADC_CHANNEL_POWER_GPIO, NULL));

	// Inicializa el semásforo (la estructura del manejador se definió globalmente)
	semSample = xSemaphoreCreateBinary();

	// Crea y establece una estructura de configuración para el temporizador
	const esp_timer_create_args_t tmrSampleArgs = {
		.callback = &tmrSampleCallback,
		.name = "Timer Configuration"
	};

	// Lanza el temporizador, conel periodo de muestreo recibido como parámetro
	esp_timer_handle_t tmrSample;
	ESP_ERROR_CHECK(esp_timer_create(&tmrSampleArgs, &tmrSample));
	ESP_ERROR_CHECK(esp_timer_start_periodic(tmrSample, period_us));
	
	// variables para reutilizar en el bucle
	void *ptr;
	therm_data_t main_data, replica_data;
	main_data.source = MAIN;
	replica_data.source = REPLICA;

	int cycles = 0;
	// Loop
	TASK_LOOP()
	{
		// Se bloquea a la espera del semáforo. Si el periodo establecido se retrasa un 20%
		// el sistema se reinicia por seguridad. Este mecanismo de watchdog software es útil
		// en tareas periódicas cuyo periodo es conocido. 
		if(xSemaphoreTake(semSample, ((1000/frequency)*1.2)/portTICK_PERIOD_MS))
		{	
			/* ---------------------------------------------------------------------------------------------
			*	Lectura del termistor PRINCIPAL
			* ---------------------------------------------------------------------------------------------
			*/
			ESP_ERROR_CHECK(therm_read_t(&main_thermistor, &main_data.value));
 
			if (xRingbufferSendAcquire(*monitor_ring_buffer, &ptr, sizeof(main_data), pdMS_TO_TICKS(100)) != pdTRUE)
			{
				ESP_LOGI(TAG,"Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*monitor_ring_buffer));
			}
			else 
			{
				memcpy(ptr, &main_data, sizeof(main_data));
				xRingbufferSendComplete(*monitor_ring_buffer, ptr);
			}

			if (cycles%check_interval_cycles != 0) return;			

			/* ---------------------------------------------------------------------------------------------
			*	Lectura del termistor REPLICA
			* ---------------------------------------------------------------------------------------------
			*/
			ESP_ERROR_CHECK(therm_read_t(&replica_thermistor, &replica_data.value));

			if (xRingbufferSendAcquire(*checker_ring_buffer, &ptr, (sizeof(main_data)), pdMS_TO_TICKS(100)) != pdTRUE)
			{
				ESP_LOGI(TAG,"Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*checker_ring_buffer));
			}
			else 
			{
				memcpy(ptr, &main_data, sizeof(main_data));
				xRingbufferSendComplete(*checker_ring_buffer, ptr);
			}
			if (xRingbufferSendAcquire(*checker_ring_buffer, &ptr, (sizeof(replica_data)), pdMS_TO_TICKS(100)) != pdTRUE)
			{
				ESP_LOGI(TAG,"Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*checker_ring_buffer));
			}
			else 
			{
				memcpy(ptr, &replica_data, sizeof(replica_data));
				xRingbufferSendComplete(*checker_ring_buffer, ptr);
			}
		}
		else
		{
			ESP_LOGI(TAG,"Watchdog (soft) failed");
			esp_restart();
		}
	}
	
	ESP_LOGI(TAG,"Deteniendo la tarea...");
	// detención controlada de las estructuras que ha levantado la tarea
	ESP_ERROR_CHECK(esp_timer_stop(tmrSample));
	ESP_ERROR_CHECK(esp_timer_delete(tmrSample));
	TASK_END();
}