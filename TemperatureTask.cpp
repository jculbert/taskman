#include "AppConfig.h"

#ifdef TEMPERATURE_TASK

#include "sl_i2cspm_instances.h"
#include "sl_si70xx.h"

extern "C" {
#include "app/framework/util/attribute-table.h"
}

#include "TemperatureTask.h"

static TemperatureTask *task;

extern "C" void temperature_task_init(uint8_t endpoint, uint32_t period_mins)
{
    TemperatureTask* tmp = new TemperatureTask(endpoint, TemperatureTask::LOG_DEBUG, period_mins);
}

TemperatureTask::TemperatureTask(uint8_t endpoint, enum LOG_LEVEL log_level, uint32_t period_mins)
  : Task(endpoint, "Temp", log_level), temperature(-100), period_ms(period_mins*60000)
{
    task = this;
    sl_status_t status = sl_si70xx_init(sl_i2cspm_inst0, SI7021_ADDR);
    log_debug("sl_si7021_init status = %d\n", status);

    // First run
    request_process(10000);
}

void TemperatureTask::process()
{
    log_debug("process");

    int32_t temp_data;
    uint32_t rh_data;
    sl_si70xx_measure_rh_and_temp(sl_i2cspm_inst0, SI7021_ADDR, &rh_data, &temp_data);
    update_temperature((int16_t) (temp_data / 10));

    request_process(period_ms);
}

void TemperatureTask::update_temperature(int16_t t)
{
    temperature = t;
    EmberAfStatus result = emberAfWriteServerAttribute(endpoint, ZCL_TEMP_MEASUREMENT_CLUSTER_ID, ZCL_TEMP_MEASURED_VALUE_ATTRIBUTE_ID,
        (uint8_t *) &temperature, ZCL_INT16S_ATTRIBUTE_TYPE);
    log_debug("update_temperature result=%d, state=%d", result, temperature);
}

#endif // TEMPERATURE
