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
#if !CONFIG_IDF_TARGET_LINUX
#include "esp_eth.h"
#include "nvs_flash.h"
#include <esp_system.h>
#include <esp_wifi.h>
#endif // !CONFIG_IDF_TARGET_LINUX

#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN (64)
static const char *TAG = "main";

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

#define MAX_SCHEDULES 24
#define MAX_TRIES 10
#define GGPIO(X) (1ULL << GPIO_NUM_##X)

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
};

static server_ctx_t server_ctx = {0};

extern void wifi_init_softap(void);
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

static esp_err_t status_table_handler(httpd_req_t *req) {
  esp_chip_info_t chip_info;
  uint32_t flash_size;
  esp_chip_info(&chip_info);
  char buf[256];
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

  format_table_u32(buf, sizeof(buf), "Flash size", esp_get_minimum_free_heap_size());
  httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
  
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK; 
}

static esp_err_t schedule_table_handler(httpd_req_t *req) {
  esp_chip_info_t chip_info;
  uint32_t flash_size;
  esp_chip_info(&chip_info);
  char buf[256];
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

  format_table_u32(buf, sizeof(buf), "Flash size", esp_get_minimum_free_heap_size());
  httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
  
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK; 
}

static esp_err_t root_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;

  httpd_resp_set_hdr(req, "Connection", "close");
  httpd_resp_send(req, root_html_start, HTTPD_RESP_USE_STRLEN);
  httpd_resp_send_chunk(req, NULL, 0);
#if 0
  /* Get header value string length and allocate memory for length + 1,
   * extra byte for null termination */
  buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
    /* Copy null terminated value string into buffer */
    if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
      ESP_LOGI(TAG, "Found header => Host: %s", buf);
    }
    free(buf);
  }

  buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
    if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) ==
        ESP_OK) {
      ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
    }
    free(buf);
  }

  buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
    if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) ==
        ESP_OK) {
      ESP_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
    }
    free(buf);
  }

  /* Read URL query string length and allocate memory for length + 1,
   * extra byte for null termination */
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      ESP_LOGI(TAG, "Found URL query => %s", buf);
      char param[EXAMPLE_HTTP_QUERY_KEY_MAX_LEN],
          dec_param[EXAMPLE_HTTP_QUERY_KEY_MAX_LEN] = {0};
      /* Get value of expected key from query string */
      if (httpd_query_key_value(buf, "query1", param, sizeof(param)) ==
          ESP_OK) {
        ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
        example_uri_decode(dec_param, param,
                           strnlen(param, EXAMPLE_HTTP_QUERY_KEY_MAX_LEN));
        ESP_LOGI(TAG, "Decoded query parameter => %s", dec_param);
      }
      if (httpd_query_key_value(buf, "query3", param, sizeof(param)) ==
          ESP_OK) {
        ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
        example_uri_decode(dec_param, param,
                           strnlen(param, EXAMPLE_HTTP_QUERY_KEY_MAX_LEN));
        ESP_LOGI(TAG, "Decoded query parameter => %s", dec_param);
      }
      if (httpd_query_key_value(buf, "query2", param, sizeof(param)) ==
          ESP_OK) {
        ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
        example_uri_decode(dec_param, param,
                           strnlen(param, EXAMPLE_HTTP_QUERY_KEY_MAX_LEN));
        ESP_LOGI(TAG, "Decoded query parameter => %s", dec_param);
      }
    }
    free(buf);
  }

  /* Set some custom headers */
  httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
  httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

  /* Send response with custom headers and body set as the
   * string passed in user context*/
  const char *resp_str = (const char *)req->user_ctx;
  httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

  /* After sending the HTTP response the old HTTP request
   * headers are lost. Check if HTTP request headers can be read now. */
  if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
    ESP_LOGI(TAG, "Request headers lost");
  }
#endif
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
static esp_err_t add_schedule_handler(httpd_req_t *req) {
  int len;
  char buf[256] = {0};
  if ((len = read_buf(req, buf, sizeof(buf))) < 0)
    return ESP_FAIL;
  /* Log data received */
  ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
  ESP_LOGI(TAG, "%.*s", len, buf);
  ESP_LOGI(TAG, "====================================");
  httpd_resp_set_status(req, "201 Created");
  httpd_resp_set_hdr(req, "Location", "/?tab=lighting");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}
static esp_err_t gpio_level_handler(httpd_req_t *req) {
  char* buf = NULL;
  size_t buf_len;

  // Get the length of the query string
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
      buf = malloc(buf_len);
      if (buf == NULL) {
          ESP_LOGE(TAG, "Failed to allocate memory");
          // Respond with an internal server error
          httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
          return ESP_FAIL;
      }
      // Get the query string itself
      if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
          ESP_LOGI(TAG, "Found URL query => %s", buf);
          char param[32];
          // Extract the value for a specific key "param1"
          if (httpd_query_key_value(buf, "gpio", param, sizeof(param)) == ESP_OK) {
              ESP_LOGI(TAG, "Found key 'param1' = %s", param);
              uint64_t g = gpiov_to_macro(atoi(param));
	      if (!g) goto err;
	      int level = server_ctx.pins_level[atoi(param)];
	      char str[32] = {0};
	      str[0] = '0' + level;
              httpd_resp_send(req, str, HTTPD_RESP_USE_STRLEN);
              // Use 'param' value in your application logic
          }
      }
  }
  free(buf);
  return ESP_OK;
err:
  free(buf);
  return ESP_FAIL;
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
    if (!g) goto err;
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
  config.lru_purge_enable = true;

  // Start the httpd server
  ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK) {
    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &body_js);
    httpd_register_uri_handler(server, &status_table);
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


  //gpio_set_level(GPIO_NUM_25, 1);
  /* Start the server for the first time */
  server = start_webserver();

  while (server) {
    sleep(5);
  }
}
