#include <stdbool.h>

typedef struct {
    float co2_ppm;
    float temperature_c;
    float humidity_rh;
    int voc_index;
    int nox_index;
    float pressure_hpa;
} air_metrics_t;

bool air_sensors_init(void);
bool air_sensors_read(air_metrics_t *out);
