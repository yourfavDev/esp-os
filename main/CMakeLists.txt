idf_component_register(SRCS "hello.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_wifi bt esp_netif)
