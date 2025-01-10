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

	task_sensor_args_t* ptr_args = (task_sensor_args_t*) TASK_ARGS;
	RingbufHandle_t* voter_ring_buffer = ptr_args->voter_ring_buffer; 
	uint8_t frequency = ptr_args->freq;
	uint64_t period_us = 1000000 / frequency;

	adc_oneshot_unit_handle_t adc_hdlr_unit_1;
	adc_oneshot_unit_init_cfg_t adc_unit_1_default_cfg = {
		.unit_id = ADC_UNIT_1,
		.clk_src = ADC_RTC_CLK_SRC_DEFAULT,
	};

	ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_unit_1_default_cfg, &adc_hdlr_unit_1));

	adc_oneshot_unit_handle_t adc_hdlr_unit_2;
	adc_oneshot_unit_init_cfg_t adc_unit_2_default_cfg = {
		.unit_id = ADC_UNIT_2,
		.clk_src = ADC_RTC_CLK_SRC_DEFAULT,
	};

	ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_unit_2_default_cfg, &adc_hdlr_unit_2));

	therm_t thermistor_a = { .adc_hdlr = adc_hdlr_unit_1 };
	therm_t thermistor_b = { .adc_hdlr = adc_hdlr_unit_1 };
	therm_t thermistor_c = { .adc_hdlr = adc_hdlr_unit_2 };

	ESP_ERROR_CHECK(therm_config(&thermistor_a, THERMISTOR_A_ADC_CHANNEL, THERMISTOR_A_ADC_CHANNEL_POWER_GPIO, NULL));
	ESP_ERROR_CHECK(therm_config(&thermistor_b, THERMISTOR_B_ADC_CHANNEL, THERMISTOR_B_ADC_CHANNEL_POWER_GPIO, NULL));
	ESP_ERROR_CHECK(therm_config(&thermistor_c, THERMISTOR_C_ADC_CHANNEL, THERMISTOR_C_ADC_CHANNEL_POWER_GPIO, NULL));

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
	uint16_t thermistor_reads[3];

	// Loop
	TASK_LOOP()
	{
		// Se bloquea a la espera del semáforo. Si el periodo establecido se retrasa un 20%
		// el sistema se reinicia por seguridad. Este mecanismo de watchdog software es útil
		// en tareas periódicas cuyo periodo es conocido. 
		if(xSemaphoreTake(semSample, ((1000/frequency)*1.2)/portTICK_PERIOD_MS))
		{
			ESP_ERROR_CHECK(therm_read_lsb(&thermistor_a, &thermistor_reads[0]));
			ESP_ERROR_CHECK(therm_read_lsb(&thermistor_b, &thermistor_reads[1]));
			ESP_ERROR_CHECK(therm_read_lsb(&thermistor_c, &thermistor_reads[2]));


			if (xRingbufferSendAcquire(*voter_ring_buffer, &ptr, 3*sizeof(uint16_t), pdMS_TO_TICKS(100)) != pdTRUE)
			{
				ESP_LOGI(TAG,"Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*voter_ring_buffer));
			}
			else 
			{
				memcpy(ptr, &thermistor_reads, 3*sizeof(uint16_t));
				xRingbufferSendComplete(*voter_ring_buffer, ptr);
			}
		}
		else
		{
			ESP_LOGI(TAG,"Watchdog (soft) failed");
			esp_restart();
		}
	}
	
	ESP_LOGI(TAG,"Deteniendo la tarea...");
	ESP_ERROR_CHECK(esp_timer_stop(tmrSample));
	ESP_ERROR_CHECK(esp_timer_delete(tmrSample));
	TASK_END();
}