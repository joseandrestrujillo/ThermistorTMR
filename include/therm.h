#ifndef __THERM_H__
#define __THERM_H__

#include <esp_adc/adc_oneshot.h>
#include <hal/adc_types.h>
#include <math.h>
#include <esp_err.h>

// Constantes del termistor
#define SERIES_RESISTANCE 10000       ///< Resistencia de la serie (10K ohms)
#define NOMINAL_RESISTANCE 10000      ///< Resistencia nominal (10K ohms)
#define NOMINAL_TEMPERATURE 298.15    ///< Temperatura nominal (25°C en Kelvin)
#define BETA_COEFFICIENT 3950         ///< Constante B

// Estructura de configuración del termistor
typedef struct therm_conf_t {
    adc_oneshot_unit_handle_t adc_hdlr; ///< Handler del ADC
    adc_channel_t adc_channel;          ///< Canal del ADC asignado
} therm_t;

/**
 * @brief Configura un termistor con su propio handler ADC y canal específico.
 *
 * @param[out] thermistor Puntero a la estructura del termistor.
 * @param[in] channel Canal del ADC asociado al termistor.
 * @return
 * - ESP_OK: si la configuración es exitosa.
 * - ESP_ERR_INVALID_ARG: si el puntero del termistor es nulo.
 * - Otros códigos de error relacionados con la configuración del ADC.
 */
esp_err_t therm_config(therm_t* thermistor, adc_channel_t channel);

/**
 * @brief Lee la temperatura desde el termistor.
 *
 * @param[in] thermistor Puntero a la estructura configurada del termistor.
 * @param[out] temperature Puntero donde se almacenará la temperatura en grados Celsius.
 * @return
 * - ESP_OK: si la lectura es exitosa.
 * - ESP_ERR_INVALID_ARG: si el puntero del termistor o temperature es nulo.
 * - ESP_ERR_TIMEOUT: Timeout, el resultado del ADC es invalido
 */
esp_err_t therm_read_t(const therm_t* thermistor, float* temperature);

/**
 * @brief Lee el voltaje desde el termistor.
 *
 * @param[in] thermistor Puntero a la estructura configurada del termistor.
 * @param[out] voltage Puntero donde se almacenará el voltaje en voltios.
 * @return
 * - ESP_OK: si la lectura es exitosa.
 * - ESP_ERR_INVALID_ARG: si el puntero del termistor o voltage es nulo.
 * - ESP_ERR_TIMEOUT: Timeout, el resultado del ADC es invalido.
 */
esp_err_t therm_read_v(const therm_t* thermistor, float* voltage);

/**
 * @brief Lee el valor bruto del ADC (LSB).
 *
 * @param[in] thermistor Puntero a la estructura configurada del termistor.
 * @param[out] lsb Puntero donde se almacenará el valor bruto del ADC.
 * @return
 * - ESP_OK: si la lectura es exitosa.
 * - ESP_ERR_INVALID_ARG: si el puntero del termistor o lsb es nulo.
 * - ESP_ERR_TIMEOUT: Timeout, el resultado del ADC es invalido.
 */
esp_err_t therm_read_lsb(const therm_t* thermistor, uint16_t* lsb);

#endif
