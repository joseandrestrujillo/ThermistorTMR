#include "config.h"
#include "therm.h"

float _voltage_to_temperature(float v, const therm_t* thermistor);
float _lsb_to_voltage(uint16_t lsb, const therm_t* thermistor);


esp_err_t therm_config(therm_t* thermistor, adc_channel_t channel, therm_config_params_t* params) {
    if (!thermistor) {
        return ESP_ERR_INVALID_ARG;
    }

    // Configuración del ADC Unit
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = THERMISTOR_ADC_UNIT,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
    };
    if (params && params->custom_unit_cfg) {
        unit_cfg = *(params->custom_unit_cfg);
    }

    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &thermistor->adc_hdlr);
    if (err != ESP_OK) {
        return err;
    }

    // Configuración del Canal del ADC
    adc_oneshot_chan_cfg_t channel_cfg = {
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (params && params->custom_channel_cfg) {
        channel_cfg = *(params->custom_channel_cfg);
    }

    err = adc_oneshot_config_channel(thermistor->adc_hdlr, channel, &channel_cfg);
    if (err != ESP_OK) {
        return err;
    }

    thermistor->adc_channel = channel;

    // Configuración de parámetros opcionales
    thermistor->series_resistance = params && params->series_resistance ? params->series_resistance : SERIES_RESISTANCE;
    thermistor->nominal_resistance = params && params->nominal_resistance ? params->nominal_resistance : NOMINAL_RESISTANCE;
    thermistor->nominal_temperature = params && params->nominal_temperature ? params->nominal_temperature : NOMINAL_TEMPERATURE;
    thermistor->beta_coefficient = params && params->beta_coefficient ? params->beta_coefficient : BETA_COEFFICIENT;
    thermistor->reference_voltage = params && params->reference_voltage ? params->reference_voltage : 3.3f;

    return ESP_OK;
}

esp_err_t therm_read_t(const therm_t* thermistor, float* temperature) {
    if (!thermistor || !temperature) {
        return ESP_ERR_INVALID_ARG;
    }
    float voltage = 0;
    esp_err_t ret = therm_read_v(thermistor, &voltage);
    if (ret != ESP_OK) {
        return ret;
    }
    *temperature = _voltage_to_temperature(voltage, thermistor);
    return ESP_OK;
}

esp_err_t therm_read_v(const therm_t* thermistor, float* voltage) {
    if (!thermistor || !voltage) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t lsb = 0;
    esp_err_t ret = therm_read_lsb(thermistor, &lsb);
    if (ret != ESP_OK) {
        return ret;
    }
    *voltage = _lsb_to_voltage(lsb, thermistor);
    return ESP_OK;
}

esp_err_t therm_read_lsb(const therm_t* thermistor, uint16_t* lsb) {
    if (!thermistor || !lsb) {
        return ESP_ERR_INVALID_ARG;
    }

    int raw_value = 0;
    esp_err_t err = adc_oneshot_read(thermistor->adc_hdlr, thermistor->adc_channel, &raw_value);
    if (err != ESP_OK) {
        return err;
    }

    *lsb = (uint16_t)raw_value;
    return ESP_OK;
}

float _voltage_to_temperature(float v, const therm_t* thermistor) {
    // resistencia del termistor, obtenida por el voltaje medido en el adc.
    float r_ntc = thermistor->series_resistance * (thermistor->reference_voltage - v) / v;

    // Ecuación de Steinhart-Hart, que relaciona la resistencia que ofrece un material semiconductor 
	// con la variación de la temperatura en Kelvin, de acuerdo a unos coeficientes que caracterizan
	// al semiconductor en cuestión.
    float t_kelvin = 1.0f / (1.0f / thermistor->nominal_temperature + (1.0f / thermistor->beta_coefficient) * log(r_ntc / thermistor->nominal_resistance));
    
    // Resultado en grados centígrados
    return t_kelvin - 273.15f;
}

float _lsb_to_voltage(uint16_t lsb, const therm_t* thermistor) {
    return (float) ((lsb) * thermistor->reference_voltage / 4095.0f);
}
