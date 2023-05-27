#include "nosedive.h"

#define ADC_CALIBRATION_TAG "ADC"

adc_cali_handle_t
get_adc_channel_calibration_profile(adc_unit_t id, adc_channel_t ch, adc_bitwidth_t res, adc_atten_t db)
{
	adc_cali_handle_t handle = NULL;
	esp_err_t status = ESP_FAIL;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
	ESP_LOGI(ADC_CALIBRATION_TAG, "calibration scheme version is curve fitting");
	adc_cali_curve_fitting_config_t cali_curve_cfg = {
		.unit_id = id,
		.chan = ch,
		.atten = db,
		.bitwidth = res,
	};
	status = adc_cali_create_scheme_curve_fitting(&cali_curve_cfg, &handle);
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
	if (status != ESP_OK) {
		ESP_LOGI(ADC_CALIBRATION_TAG, "calibration scheme version is line fitting");
		adc_cali_line_fitting_config_t cali_line_cfg = {
			.unit_id = id,
			.atten = db,
			.bitwidth = res,
		};
		status = adc_cali_create_scheme_line_fitting(&cali_line_cfg, &handle);
	}
#endif

	if (status == ESP_OK) {
		ESP_LOGI(ADC_CALIBRATION_TAG, "Calibration Success");
		return handle;
	} else if (status == ESP_ERR_NOT_SUPPORTED) {
		ESP_LOGW(ADC_CALIBRATION_TAG, "No software calibration");
	} else {
		ESP_LOGE(ADC_CALIBRATION_TAG, "Invalid arg or OOM");
	}

	return NULL;
}
