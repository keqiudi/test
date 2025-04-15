#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern void benchmark_helix_main(void);

static void benchmark_test_task(void *parameters)
{
    benchmark_helix_main();

    vTaskDelete(NULL);
}

int main(void)
{
    xTaskCreate(benchmark_test_task, "benchmark", 1024, NULL, configMAX_PRIORITIES - 1, NULL);

    return 0;
}
