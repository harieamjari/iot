set(requires esp-tls nvs_flash esp_netif esp_http_server)
idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    list(APPEND requires esp_stubs protocol_examples_common)
else()
    list(APPEND requires esp_wifi esp_eth spi_flash esp_driver_gpio esp_adc)
endif()

idf_component_register(SRCS "main.c" "wifi.c" "voltage.c"
                    INCLUDE_DIRS "."
		    PRIV_REQUIRES ${requires}
                    EMBED_TXTFILES embed/index.html embed/body.js)
