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
	RingbufHandle_t* rbuf = ptr_args->rbuf; 
	uint8_t frequency = ptr_args->freq;
	uint64_t period_us = 1000000 / frequency;

	// Configuramos el termistor con el modulo encapsulado
	therm_t thermistor_configuration;
	ESP_ERROR_CHECK(therm_config(&thermistor_configuration, THERMISTOR_ADC_CHANNEL, NULL));

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
	float v;

	// Loop
	TASK_LOOP()
	{
		// Se bloquea a la espera del semáforo. Si el periodo establecido se retrasa un 20%
		// el sistema se reinicia por seguridad. Este mecanismo de watchdog software es útil
		// en tareas periódicas cuyo periodo es conocido. 
		if(xSemaphoreTake(semSample, ((1000/frequency)*1.2)/portTICK_PERIOD_MS))
		{	
			// lectura del sensor. Se obtiene el valor en grados centígrados
			ESP_ERROR_CHECK(therm_read_t(&thermistor_configuration, &v));
			ESP_LOGI(TAG, "valor medido (pre buffer): %f", v);


			// Uso del buffer cíclico entre la tarea monitor y sensor. Ver documentación en ESP-IDF
			// Pide al RingBuffer espacio para escribir un float. 
			if (xRingbufferSendAcquire(*rbuf, &ptr, sizeof(float), pdMS_TO_TICKS(100)) != pdTRUE)
			{
				// Si falla la reserva de memoria, notifica la pérdida del dato. Esto ocurre cuando 
				// una tarea productora es mucho más rápida que la tarea consumidora. Aquí no debe ocurrir.
				ESP_LOGI(TAG,"Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*rbuf));
			}
			else 
			{
				// Si xRingbufferSendAcquire tiene éxito, podemos escribir el número de bytes solicitados
				// en el puntero ptr. El espacio asignado estará bloqueado para su lectura hasta que 
				// se notifique que se ha completado la escritura
				memcpy(ptr,&v, sizeof(float));

				// Se notifica que la escritura ha completado. 
				xRingbufferSendComplete(*rbuf, ptr);
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