/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_tls_crypto.h"
#include "esp_flash.h"
#include "esp_chip_info.h"
#include "driver/gpio.h"
#include <esp_http_server.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#if !CONFIG_IDF_TARGET_LINUX
#include "esp_eth.h"
#include "nvs_flash.h"
#include <esp_system.h>
#include <esp_wifi.h>
#endif // !CONFIG_IDF_TARGET_LINUX
#include "server.h"

#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN (64)
static const char *TAG = "main";

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

#define MAX_TRIES 10
#define GGPIO(X) (1ULL << GPIO_NUM_##X)


static server_ctx_t server_ctx = {0};

extern void wifi_init_softap(void);
extern void adc_iot_init(server_ctx_t *);
extern void adc_iot_deinit(server_ctx_t *);
extern void adc_iot_read(server_ctx_t *);
extern const char *quote_iot_get();
/* embedded files */
extern const char root_html_start[] asm("_binary_index_html_start");
extern const char root_html_end[] asm("_binary_index_html_end");

extern const char body_js_start[] asm("_binary_body_js_start");
extern const char body_js_end[] asm("_binary_body_js_end");

static esp_err_t root_handler(httpd_req_t *req);

static esp_err_t add_schedule_handler(httpd_req_t *req);
static esp_err_t remove_schedule_handler(httpd_req_t *req);
static esp_err_t status_table_handler(httpd_req_t *req);
static esp_err_t schedule_table_handler(httpd_req_t *req);
static esp_err_t body_js_handler(httpd_req_t *req);
static esp_err_t gpio_handler(httpd_req_t *req);
static esp_err_t gpio_level_handler(httpd_req_t *req);
static esp_err_t quote_handler(httpd_req_t *req);
static esp_err_t update_datetime_handler(httpd_req_t *req);
static char *percent_decode(const uint8_t *value, size_t valuelen);

static const httpd_uri_t root = {
    /* queries, room, time start-end */
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = &server_ctx};
static const httpd_uri_t gpio = {
    /* queries, room, time start-end */
    .uri = "/gpio",
    .method = HTTP_POST,
    .handler = gpio_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = &server_ctx};
static const httpd_uri_t quote = {
    /* queries, room, time start-end */
    .uri = "/quote",
    .method = HTTP_GET,
    .handler = quote_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = &server_ctx};
static const httpd_uri_t update_datetime = {
    /* queries, room, time start-end */
    .uri = "/update_datetime",
    .method = HTTP_POST,
    .handler = update_datetime_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = &server_ctx};
static const httpd_uri_t gpio_level = {
    /* queries, room, time start-end */
    .uri = "/gpio_level",
    .method = HTTP_GET,
    .handler = gpio_level_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = &server_ctx};
static const httpd_uri_t status_table = {
    /* queries, room, time start-end */
    .uri = "/status_table",
    .method = HTTP_GET,
    .handler = status_table_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = NULL};
static const httpd_uri_t schedule_table = {
    /* queries, room, time start-end */
    .uri = "/schedule_table",
    .method = HTTP_GET,
    .handler = schedule_table_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = NULL};
static const httpd_uri_t body_js = {
    /* queries, room, time start-end */
    .uri = "/body.js",
    .method = HTTP_GET,
    .handler = body_js_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = &server_ctx};
static const httpd_uri_t add_schedule = {
    /* queries, room, time start-end */
    .uri = "/add_schedule",
    .method = HTTP_POST,
    .handler = add_schedule_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = &server_ctx};
static const httpd_uri_t remove_schedule = {
    /* queries, index */
    .uri = "/remove_schedule",
    .method = HTTP_POST,
    .handler = remove_schedule_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = &server_ctx};

/* An HTTP GET handler */
static void format_table_str(char *buf, int len, char *name, char *value) {
  snprintf(buf, len, "<tr><td>%s</td><td>%s</td></tr>", name, value);
}
static void format_table_int(char *buf, int len, char *name, int value) {
  snprintf(buf, len, "<tr><td>%s</td><td>%d</td></tr>", name, value);
}
static void format_table_u32(char *buf, int len, char *name, uint32_t value) {
  snprintf(buf, len, "<tr><td>%s</td><td>%"PRIu32"</td></tr>", name, value);
}
//uint32_t read_voltage(adc1_channel_t channel){
//  esp_adc_cal_characteristics_t *adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
//  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
//  uint32_t reading =  adc1_get_raw(channel);
//  uint32_t raw_voltage = esp_adc_cal_raw_to_voltage(reading, adc_chars)
//  free(adc_chars);
//  return voltage;
//    
//}
static void format_time(char *buf, size_t size) {
  time_t now;
  struct tm timeinfo;

  time(&now);

  localtime_r(&now, &timeinfo);
  strftime(buf, size, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

static void reply_500(httpd_req_t *req) {
  httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
}
static void reply_400(httpd_req_t *req) {
  httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad request");
}
static esp_err_t status_table_handler(httpd_req_t *req) {
  esp_chip_info_t chip_info;
  uint32_t flash_size;
  esp_chip_info(&chip_info);
  char buf[256], time[64];
  format_time(time, sizeof(time));
  format_table_str(buf, sizeof(buf), "Time", time);
  httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);

  format_table_str(buf, sizeof(buf), "Target", CONFIG_IDF_TARGET);
  httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
  format_table_int(buf, sizeof(buf), "Cores", chip_info.cores);
  httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
  if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
    format_table_u32(buf, sizeof(buf), "Flash size", flash_size);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
  }
  format_table_str(buf, sizeof(buf) , "Flash size location", chip_info.features & CHIP_FEATURE_EMB_FLASH ? "embedded" : "external");
  httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);

  format_table_u32(buf, sizeof(buf), "Free heap size", esp_get_minimum_free_heap_size());
  httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);

  adc_iot_read(&server_ctx);
  format_table_int(buf, sizeof(buf), "Battery voltage divider (normal 1650 mV)", server_ctx.voltage4);
  httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
//
  format_table_int(buf, sizeof(buf), "LDR 1 voltage (mV)", server_ctx.voltage7);
  httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);

  format_table_int(buf, sizeof(buf), "LDR 2 voltage (mV)", server_ctx.voltage6);
  httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
  
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK; 
}

static void format_schedule_table_str(char *buf, int len, char *name, uint8_t hours, uint8_t minutes, uint8_t switchv ) {
  snprintf(buf, len, "<tr><td>%s</td><td>%"PRIu8":%"PRIu8"</td><td>%"PRIu8"</td></tr>", name, hours, minutes, switchv);
}

static esp_err_t schedule_table_handler(httpd_req_t *req) {
  esp_chip_info_t chip_info;
  uint32_t flash_size;
  esp_chip_info(&chip_info);
  char buf[256];
  for (int i = 0; i < MAX_SCHEDULES; i++) {
    if (server_ctx.schedules[i].is_active) {
      schedule_t *sched = &server_ctx.schedules[i];
        format_schedule_table_str(buf, sizeof(buf), sched->sched_config.name, sched->sched_config.triger.hours, sched->sched_config.trigger.minutes, sched->switchv);
        httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    }
      
  }
  
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK; 
}

static esp_err_t root_handler(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "Connection", "close");
  httpd_resp_send(req, root_html_start, HTTPD_RESP_USE_STRLEN);
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

static int read_buf(httpd_req_t *req, char *buf, size_t len) {
  int ret;
  int tries = 0, read = 0;
  if (req->content_len > 255)
    return ESP_FAIL;

  while (read < req->content_len) {
    /* Read the data for the request */
    if ((ret = httpd_req_recv(req, buf + read, req->content_len - read)) <= 0) {
      if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
        /* Retry receiving if timeout occurred */
	tries++;
	if (tries < MAX_TRIES)
          continue;
      }
      ESP_LOGI(TAG, "failed to read req %p\n", (void*)req);
      return -1;
    }

    /* Send back the same data */
    httpd_resp_send_chunk(req, buf, ret);
    read += ret;

  }
  return read;
}

static uint64_t gpiov_to_macro(int gpio){
  // .pin_bit_mask = (GGPIO(22) | GGPIO(23) | GGPIO(25) | GGPIO(26) | GGPIO(27) | GGPIO(32) | GGPIO(33)),
  switch (gpio) {
    case 22: return GPIO_NUM_22;
    case 23: return GPIO_NUM_23;
    case 25: return GPIO_NUM_25;
    case 26: return GPIO_NUM_26;
    case 27: return GPIO_NUM_27;
    case 32: return GPIO_NUM_32;
    case 33: return GPIO_NUM_33;
    default: return 0;
  }
}
static int macro_to_gpiov(uint64_t gpio){
  // .pin_bit_mask = (GGPIO(22) | GGPIO(23) | GGPIO(25) | GGPIO(26) | GGPIO(27) | GGPIO(32) | GGPIO(33)),
  switch (gpio) {
    case GPIO_NUM_22: return 22;
    case GPIO_NUM_23: return 23;
    case GPIO_NUM_25: return 25;
    case GPIO_NUM_26: return 26;
    case GPIO_NUM_27: return 27;
    case GPIO_NUM_32: return 32;
    case GPIO_NUM_33: return 33;
    default: return -1;
  }
}
static int schedule_get_unused_ndx() {
  for (int i = 0; i < MAX_SCHEDULES; i++)
    if (!server_ctx.schedules[i].is_active)
      return i;
  return -1;
}

static void schedule_trigger_cb(esp_schedule_handle_t handle, void *priv_data)
{
  schedule_t *schedule = priv_data;
  if (schedule->switchv) {
    gpio_set_level(schedule->gpio, 1);
    server_ctx.pins_level[macro_to_gpiov(schedule->gpio)] = 1;
  }
  else {
    gpio_set_level(schedule->gpio, 0);
    server_ctx.pins_level[macro_to_gpiov(schedule->gpio)] = 0;
  
  }
}
static void schedule_timestamp_cb(esp_schedule_handle_t handle, uint32_t next_timestamp, void *priv_data)
{
	return;
}



static esp_err_t add_schedule_handler(httpd_req_t *req) {
  int len;
  char buf[256] = {0}, *decoded, value[32];
  if ((len = read_buf(req, buf, sizeof(buf))) < 0) {
    return ESP_FAIL;
  }
  /* Log data received */
  ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
  ESP_LOGI(TAG, "%.*s", len, buf);
  ESP_LOGI(TAG, "====================================");
  decoded = (char *)percent_decode((uint8_t*)buf, sizeof(buf));
  uint64_t gpio;
  uint8_t hour, minute;
  char switchv;

  int schedule_ndx = schedule_get_unused_ndx();
  if (schedule_ndx < 0) {
    reply_500(req);
    return ESP_FAIL;
  }

  schedule_t *schedule_ctx = &server_ctx.schedules[i];
  esp_schedule_config_t sched_conf = {
    .name = "",
    .trigger.type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK,
    .trigger.hours = 0,
    .trigger.minutes = 0,
    .trigger.day.repeat_days = ESP_SCHEDULE_DAY_EVERYDAY,
    .trigger_cb = schedule_trigger_cb,
    .timestamp_cb = schedule_timestamp_cb,
    .priv_data = schedule_ctx,
    .validity = {
        .start_time = 0,  // Start immediately
        .end_time = 0     // No end time (run indefinitely)
    }
  };

  if (httpd_query_key_value(decoded, "gpio", value, sizeof(value)) != ESP_OK) {
    free(decoded);
    reply_400(req);
    return ESP_FAIL;
  }
  gpio = gpiov_to_macro(atoi(value));
  if (!gpio){
    free(decoded);
    reply_400(req);
    return ESP_FAIL;
  }
  memcpy(sched_conf.name, value, 2);
  if (httpd_query_key_value(decoded, "time", value, sizeof(value)) != ESP_OK) {
    free(decoded);
    reply_400(req);
    return ESP_FAIL;
  }
  sscanf(value, "%"SCNu8":%"SCNu8, &hour, &minute);
//  hour = (value[0] - '0')*10 + (value[1] - '0');
//  minute = (value[3] - '0')*10 + (value[4] - '0');
  if (httpd_query_key_value(decoded, "switchv", value, sizeof(value)) != ESP_OK) {
    free(decoded);
    reply_400(req);
    return ESP_FAIL;
  }
  free(decoded);
  switchv = value[0] - '0';
  sched_conf.trigger.hours = (hour < 0 || hour > 23) ? 0 : hour;
  sched_conf.trigger.minutes = (minute < 0 || minute > 59) ? 0 : minute;
  schedule_ctx->switchv = switchv;
  snprintf(sched_conf.name, sizeof(sched_conf.name), "schedule%d", schedule_ndx);


  schedule_ctx->sched_handle = esp_schedule_create(&sched_conf);
  if (schedule_ctx->sched_handle) {
    ESP_LOGI(TAG, "Created days of week schedule successfully");
    if (esp_schedule_enable(schedule_ctx->sched_handle) != ESP_OK) {
      reply_500(req);
      return ESP_FAIL;
    }
    schedule_ctx->is_active = 1;
    memcpy(&schedule_ctx->sched_config, &sched_conf, sizeof(sched_conf));

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/?tab=lighting");
    httpd_resp_send(req, "Redirecting...", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  reply_500(req);
  return ESP_FAIL;
}

static esp_err_t gpio_level_handler(httpd_req_t *req) {
  char* buf = NULL;
  size_t buf_len;

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len < 1)
    reply_400(req);

  buf = malloc(buf_len);
  if (buf == NULL) {
      reply_500(req);
      return ESP_FAIL;
  }
  // Get the query string itself
  if (httpd_req_get_url_query_str(req, buf, buf_len) != ESP_OK)
    free(buf);
    reply_400(req);
    return ESP_FAIL;
  
  ESP_LOGI(TAG, "Found URL query => %s", buf);
  char param[32];
  // Extract the value for a specific key "param1"
  if (httpd_query_key_value(buf, "gpio", param, sizeof(param)) != ESP_OK){
    free(buf);
    reply_400(req);
    return ESP_FAIL;
  }
  free(buf);

  ESP_LOGI(TAG, "Found key 'param1' = %s", param);
  uint64_t g = gpiov_to_macro(atoi(param));
  if (!g) {
    reply_400(req);
    return ESP_FAIL;
  }
  int level = server_ctx.pins_level[atoi(param)];
  char str[32] = {0};
  str[0] = '0' + level;
  httpd_resp_send(req, str, HTTPD_RESP_USE_STRLEN);
  // Use 'param' value in your application logic
  return ESP_OK;
}
static esp_err_t gpio_handler(httpd_req_t *req) {
  int len;
  char buf[256] = {0}, gpiov[16], value[16];
  if ((len = read_buf(req, buf, sizeof(buf))) < 0)
    return ESP_FAIL;
  /* Log data received */
  ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
  ESP_LOGI(TAG, "%.*s", len, buf);
  ESP_LOGI(TAG, "====================================");
  // .pin_bit_mask = (GGPIO(22) | GGPIO(23) | GGPIO(25) | GGPIO(26) | GGPIO(27) | GGPIO(32) | GGPIO(33)),
  if (httpd_query_key_value(buf, "gpio", gpiov, sizeof(gpiov)) == ESP_OK) {
    ESP_LOGI(TAG, "found param gpio=%s",gpiov);
    uint64_t g = gpiov_to_macro(atoi(gpiov));
    if (!g){
      reply_400(req);
      goto err;
    }
    if (httpd_query_key_value(buf, "value", value, sizeof(value)) == ESP_OK) {
      ESP_LOGI(TAG, "found param switch=%s", value);
      if (!strcmp(value, "off")) {
        gpio_set_level(g, 0);
	server_ctx.pins_level[atoi(gpiov)] = 0;
      } else if (!strcmp(value, "on")) {
        gpio_set_level(g, 1);
	server_ctx.pins_level[atoi(gpiov)] = 1;
      }
      else goto err;
      httpd_resp_set_status(req, "201 Created");
      httpd_resp_set_hdr(req, "Location", "/?tab=lighting");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    }
  }
err:
  return ESP_FAIL;
}
static esp_err_t quote_handler(httpd_req_t *req) {
  httpd_resp_send(req, quote_iot_get(), HTTPD_RESP_USE_STRLEN);
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

static struct tm parse_time_buf(char *buf, size_t len) {
  struct tm date_time = {0};
  char temp[32] = {0};
  memcpy(temp, buf, 4);
  temp[4] = 0;
  date_time.tm_year = (atoi(temp) < 0 ? 0 : atoi(temp)) - 1900;
  int mon = (buf[5] - '0')*10 + (buf[6]-'0');
  date_time.tm_mon = mon < 1 ? 0 : mon - 1;
  int mday = (buf[8] - '0')*10 + (buf[9]-'0');
  date_time.tm_mday = (mday < 1 || mday > 31) ? 1 : mday;
  int hour = (buf[11] - '0')*10 + (buf[12]-'0');
  date_time.tm_hour = (hour < 0 || hour > 23) ? 0 : hour;
  int min = (buf[14] - '0')*10 + (buf[15]-'0');
  date_time.tm_min = (min < 0 || min > 59) ? 0 : min;
  date_time.tm_sec = 0;
  return date_time;
}
/* Returns int value of hex string character |c| */
static uint8_t hex_to_uint(uint8_t c) {
  if ('0' <= c && c <= '9') {
    return (uint8_t)(c - '0');
  }
  if ('A' <= c && c <= 'F') {
    return (uint8_t)(c - 'A' + 10);
  }
  if ('a' <= c && c <= 'f') {
    return (uint8_t)(c - 'a' + 10);
  }
  return 0;
}
static char *percent_decode(const uint8_t *value, size_t valuelen) {
  char *res;

  res = malloc(valuelen + 1);
  if (valuelen > 3) {
    size_t i, j;
    for (i = 0, j = 0; i < valuelen - 2;) {
      if (value[i] != '%' || !isxdigit(value[i + 1]) ||
          !isxdigit(value[i + 2])) {
        res[j++] = (char)value[i++];
        continue;
      }
      res[j++] =
        (char)((hex_to_uint(value[i + 1]) << 4) + hex_to_uint(value[i + 2]));
      i += 3;
    }
    memcpy(&res[j], &value[i], 2);
    res[j + 2] = '\0';
  } else {
    memcpy(res, value, valuelen);
    res[valuelen] = '\0';
  }
  return res;
}

static esp_err_t update_datetime_handler(httpd_req_t *req) {
  int len;
  char buf[64] = {0}, datetimev[64], *decoded;
  if ((len = read_buf(req, buf, sizeof(buf))) < 0)
    return ESP_FAIL;
  if (httpd_query_key_value(buf, "datetime", datetimev, sizeof(datetimev)) != ESP_OK)
    return ESP_FAIL;
  ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
  ESP_LOGI(TAG, "%.*s", len, buf);
  ESP_LOGI(TAG, "====================================");
  decoded = percent_decode((uint8_t*)datetimev, sizeof(datetimev));
  if (decoded == NULL) return ESP_FAIL;
  ESP_LOGI(TAG, "=========== DECODED DATA ==========");
  ESP_LOGI(TAG, "%.*s", strlen(decoded), decoded);
  ESP_LOGI(TAG, "====================================");
  int year, mon, day, hour, min;
  int ret = sscanf(decoded, "%04u-%02u-%02uT%02u:%02u", &year, &mon, &day, &hour, &min);
  free(decoded);
  if (ret < 5) return ESP_FAIL;
  struct tm date_time = {
    .tm_year = year < 1900 ? 1900 : year - 1900,
    .tm_mon = mon < 1 ? 0 : mon - 1,
    .tm_mday = (day < 1 || mday > 31) ? 1 : mday,
    .tm_hour = (hour < 0 || hour > 23) ? 0 : hour,
    .tm_min = (min < 0 || min > 59) ? 0 : min,
    .tm_sec = 0
  };
  time_t epoch = mktime(&date_time);
  settimeofday(&(const struct timeval){epoch, 0}, NULL);
  if (1) goto err;
err:
  return ESP_FAIL;
}

static esp_err_t remove_schedule_handler(httpd_req_t *req) {
  int len;
  char buf[256] = {0};
  if ((len = read_buf(req, buf, sizeof(buf))) < 0)
    return ESP_FAIL;
  ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
  ESP_LOGI(TAG, "%.*s", len, buf);
  ESP_LOGI(TAG, "====================================");

  httpd_resp_set_status(req, "201 Created");
  httpd_resp_set_hdr(req, "Location", "/?tab=lighting");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

static esp_err_t body_js_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/javascript");
  httpd_resp_send(req, body_js_start, HTTPD_RESP_USE_STRLEN);
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

/* An HTTP POST handler */
static esp_err_t echo_post_handler(httpd_req_t *req) {
  char buf[100];
  int ret, remaining = req->content_len;

  while (remaining > 0) {
    /* Read the data for the request */
    if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
      if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
        /* Retry receiving if timeout occurred */
        continue;
      }
      return ESP_FAIL;
    }

    /* Send back the same data */
    httpd_resp_send_chunk(req, buf, ret);
    remaining -= ret;

    /* Log data received */
    ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
    ESP_LOGI(TAG, "%.*s", ret, buf);
    ESP_LOGI(TAG, "====================================");
  }

  // End response
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

#if 0

static const httpd_uri_t echo = {.uri = "/echo",
                                 .method = HTTP_POST,
                                 .handler = echo_post_handler,
                                 .user_ctx = NULL};

/* An HTTP_ANY handler */
static esp_err_t any_handler(httpd_req_t *req) {
  /* Send response with body set as the
   * string passed in user context*/
  const char *resp_str = (const char *)req->user_ctx;
  httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

  // End response
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

static const httpd_uri_t any = {.uri = "/any",
                                .method = HTTP_ANY,
                                .handler = any_handler,
                                /* Let's pass response string in user
                                 * context to demonstrate it's usage */
                                .user_ctx = "Hello World!"};
#endif

/* This handler allows the custom error handling functionality to be
 * tested from client side. For that, when a PUT request 0 is sent to
 * URI /ctrl, the /hello and /echo URIs are unregistered and following
 * custom error handler http_404_error_handler() is registered.
 * Afterwards, when /hello or /echo is requested, this custom error
 * handler is invoked which, after sending an error message to client,
 * either closes the underlying socket (when requested URI is /echo)
 * or keeps it open (when requested URI is /hello). This allows the
 * client to infer if the custom error handler is functioning as expected
 * by observing the socket state.
 */
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err) {
  if (strcmp("/hello", req->uri) == 0) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND,
                        "/hello URI is not available");
    /* Return ESP_OK to keep underlying socket open */
    return ESP_OK;
  } else if (strcmp("/echo", req->uri) == 0) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
    /* Return ESP_FAIL to close underlying socket */
    return ESP_FAIL;
  }
  /* For any other URI send 404 and close socket */
  httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
  return ESP_FAIL;
}
#if 0
static const httpd_uri_t ctrl = {.uri = "/ctrl",
                                 .method = HTTP_PUT,
                                 .handler = ctrl_put_handler,
                                 .user_ctx = NULL};
#endif

static httpd_handle_t start_webserver(void) {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 12;
  config.lru_purge_enable = true;

  // Start the httpd server
  ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK) {
    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &quote);
    httpd_register_uri_handler(server, &body_js);
    httpd_register_uri_handler(server, &status_table);
    httpd_register_uri_handler(server, &update_datetime);
    httpd_register_uri_handler(server, &schedule_table);
    httpd_register_uri_handler(server, &add_schedule);
    httpd_register_uri_handler(server, &remove_schedule);
    httpd_register_uri_handler(server, &gpio);
    httpd_register_uri_handler(server, &gpio_level);
    ESP_LOGI(TAG, "Ok");
    return server;
  }

  ESP_LOGI(TAG, "Error starting server!");
  return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server) {
  return httpd_stop(server);
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  httpd_handle_t *server = arg;
  if (*server) {
    ESP_LOGI(TAG, "Stopping webserver");
    if (stop_webserver(*server) == ESP_OK) {
      *server = NULL;
    } else {
      ESP_LOGE(TAG, "Failed to stop http server");
    }
  }
}

static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data) {
  httpd_handle_t *server = arg;
  if (*server == NULL) {
    ESP_LOGI(TAG, "Starting webserver");
    *server = start_webserver();
  }
}

static void gpio_conf(){
  gpio_config_t io_conf_output = {
      .pin_bit_mask = (GGPIO(22) | GGPIO(23) | GGPIO(25) | GGPIO(26) | GGPIO(27) | GGPIO(33)),
      .mode = GPIO_MODE_OUTPUT,                 // Set as output
      .pull_up_en = GPIO_PULLUP_DISABLE,        // Disable internal pull-up
      .pull_down_en = GPIO_PULLDOWN_DISABLE,    // Disable internal pull-down
      .intr_type = GPIO_INTR_DISABLE            // Disable interrupts
  };
  gpio_config(&io_conf_output);
  gpio_config_t io_conf_input = {
      .pin_bit_mask = (GGPIO(32) | GGPIO(34) | GGPIO(35)),
      .mode = GPIO_MODE_INPUT,                 // Set as output
      .pull_up_en = GPIO_PULLUP_DISABLE,        // Disable internal pull-up
      .pull_down_en = GPIO_PULLDOWN_DISABLE,    // Disable internal pull-down
      .intr_type = GPIO_INTR_DISABLE            // Disable interrupts
  };
  gpio_config(&io_conf_input);
}

void app_main(void) {
  memset(&server_ctx, 0, sizeof(server_ctx));
  httpd_handle_t server;
  esp_err_t err = nvs_flash_init();

  if ((err == ESP_ERR_NVS_NO_FREE_PAGES) ||
      (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  wifi_init_softap();

  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &connect_handler, &server));
  ESP_ERROR_CHECK(esp_event_handler_register(
      WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
  /* init gpio pins */
  gpio_conf();
  adc_iot_init(&server_ctx);



  //gpio_set_level(GPIO_NUM_25, 1);
  /* Start the server for the first time */
  server = start_webserver();

  while (server) {
    sleep(5);
  }
  adc_iot_deinit(&server_ctx);
}
