#include "config.h"
#include "therm.h"

float _voltage_to_temperature(float v);
float _lsb_to_voltage(uint16_t lsb);


esp_err_t therm_config(therm_t* thermistor, adc_channel_t channel) {
    if (!thermistor) {
        return ESP_ERR_INVALID_ARG;
    }

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = THERMISTOR_ADC_UNIT,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
    };
    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &thermistor->adc_hdlr);
    if (err != ESP_OK) {
        return err;
    }

    adc_oneshot_chan_cfg_t channel_cfg = {
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_oneshot_config_channel(thermistor->adc_hdlr, channel, &channel_cfg);
    if (err != ESP_OK) {
        return err;
    }

    thermistor->adc_channel = channel;
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
    *temperature = _voltage_to_temperature(voltage);
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
    *voltage = _lsb_to_voltage(lsb);
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

float _voltage_to_temperature(float v) {
    // resistencia del termistor, obtenida por el voltaje medido en el adc.
    float r_ntc = SERIES_RESISTANCE * (3.3 - v) / v;

    // Ecuación de Steinhart-Hart, que relaciona la resistencia que ofrece un material semiconductor 
	// con la variación de la temperatura en Kelvin, de acuerdo a unos coeficientes que caracterizan
	// al semiconductor en cuestión (están definidos en config.h) 
    float t_kelvin = 1.0f / (1.0f / NOMINAL_TEMPERATURE + (1.0f / BETA_COEFFICIENT) * log(r_ntc / NOMINAL_RESISTANCE));
    
    // Resultado en grados centígrados
    return t_kelvin - 273.15f;
}

float _lsb_to_voltage(uint16_t lsb) {
    return (float) ((lsb) * 3.3f / 4095.0f);
}
