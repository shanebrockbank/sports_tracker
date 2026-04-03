idf_component_register(
    SRCS "ina226.c" "power.c"
    INCLUDE_DIRS "include"
    REQUIRES driver system
)
