#include "esp_random.h"
#include <stdint.h>

static const int nb_quotes = 346;
typedef struct quote_t quote_t;
struct quote_t {
  char *quote;
};
static const quote_t quotes[] = {
  #include "quotes.txt"
};

const char *quote_iot_get() {
  uint32_t r = esp_random() % nb_quotes;
  return quotes[r].quote
}
