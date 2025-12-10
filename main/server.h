#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define MAX_SCHEDULES 24
typedef struct schedule_t schedule_t;
struct schedule_t {
  int room;
  uint32_t start, end;
};
typedef struct server_ctx_t server_ctx_t;
struct server_ctx_t {
  int nb_schedules;
  schedule_t schedules[MAX_SCHEDULES];
  char pins_level[38];
  int voltage6;
  int voltage7;
  int voltage4;
  adc_oneshot_unit_handle_t adc1_handle;
  adc_oneshot_unit_init_cfg_t init_config1;
  //-------------ADC1 Config---------------//
  adc_oneshot_chan_cfg_t config;
  //-------------ADC1 Calibration Init---------------//
  adc_cali_handle_t adc1_cali_chan6_handle;
  adc_cali_handle_t adc1_cali_chan7_handle;
  adc_cali_handle_t adc1_cali_chan4_handle;
  bool do_calibration1_chan6;
  bool do_calibration1_chan7;
  bool do_calibration1_chan4;
};
