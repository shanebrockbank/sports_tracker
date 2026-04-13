idf_component_register(
    SRCS "input.c"
    INCLUDE_DIRS "include"
    REQUIRES driver system
    PRIV_REQUIRES esp_timer
)
