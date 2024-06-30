#include "AppConfig.h"

#ifdef CONTACT_TASK

#include "em_gpio.h"
#include "gpiointerrupt.h"
#include "sl_i2cspm_instances.h"
#include "sl_si7210.h"

#include "ContactTask.h"

#include "sl_emlib_gpio_init_hall_input_config.h"

extern "C" {
#include "app/framework/util/attribute-table.h"
#include "app/framework/plugin/reporting/reporting.h"
}

static ContactTask *task;

extern "C" void contact_task_init(uint8_t endpoint)
{
    ContactTask* tmp = new ContactTask(endpoint, ContactTask::LOG_DEBUG);
}

static void int_callback(unsigned char intNo)
{
    task->request_process(0);
}

static sl_status_t hall_init()
{
    sl_status_t status = sl_si7210_init(sl_i2cspm_inst0);
    task->log_debug("sl_si7210_init: status %d", status);
    if (status == SL_STATUS_OK)
    {
        sl_si7210_configure_t config = {};
        config.threshold = 0.5;
        config.hysteresis = config.threshold / 5.0;

        // Configure sets threshold and histeresis and enables sleep with periodic measurements
        status = sl_si7210_configure(sl_i2cspm_inst0, &config);
        task->log_debug("sl_si7210_configure: status %d", status);
    }
    return status;
}

#if 0
static void read_hall()
{
  float value = 0.123;
  sl_status_t status = sl_si7210_measure(sl_i2cspm_inst0, 1000, &value);
  task->log_debug("read_hall: status %d, value %d", status, (int32_t)(value * 1000));

  // Had trouble getting periodic reading to work and calling
  // measure to we completely init hall after a reading
  hall_init();
}
#endif

ContactTask::ContactTask(uint8_t endpoint, enum LOG_LEVEL log_level)
  : Task(endpoint, "Contact", log_level), state(false)
{
    task = this;

    // We configure the SI7210 to measure periodically and control output according to a threshold
    // an interrupt will occur for each change in this output
    const int INT_NUM = 4; // Note, valid values depends on pin number (see GPIO_ExtIntConfig)
    GPIOINT_CallbackRegister(INT_NUM, int_callback);
    GPIO_ExtIntConfig(SL_EMLIB_GPIO_INIT_HALL_INPUT_PORT, SL_EMLIB_GPIO_INIT_HALL_INPUT_PIN, INT_NUM, 1, 1, true); // rising and falling edge, interrupt enabled

    hall_init();
}

void ContactTask::process()
{
    log_debug("process");

    // Override min report interval if not set to 1
    // Zigbee2mqtt sets it to 10
    EmberAfPluginReportingEntry entry;
    for(int i = 0; i < REPORT_TABLE_SIZE; i++){
        sli_zigbee_af_reporting_get_entry(i, &entry);
        if(entry.endpoint == endpoint && entry.clusterId == ZCL_OCCUPANCY_SENSING_CLUSTER_ID)
        {
            //log_debug("r %d %d", entry.attributeId, entry.data.reported.minInterval);
            if (entry.data.reported.minInterval != 1)
            {
                log_debug("updating rep int to 1");
                entry.data.reported.minInterval = 1;
                sli_zigbee_af_reporting_set_entry(i, &entry);
            }

            break;
        }
    }

    state = GPIO_PinInGet(SL_EMLIB_GPIO_INIT_HALL_INPUT_PORT, SL_EMLIB_GPIO_INIT_HALL_INPUT_PIN) == 0 ? false: true;
    uint8_t _state = state ? 1: 0;
    EmberAfStatus result = emberAfWriteServerAttribute(endpoint, ZCL_OCCUPANCY_SENSING_CLUSTER_ID, ZCL_OCCUPANCY_ATTRIBUTE_ID,
        &_state, ZCL_BITMAP8_ATTRIBUTE_TYPE);
    log_debug("update state result=%d, state=%d", result, state);
}

#endif // CONTACT_TASK
