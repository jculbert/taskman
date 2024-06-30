#include "AppConfig.h"

#ifdef OCCUPANCY_TASK

#include "em_gpio.h"
#include "gpiointerrupt.h"

#include "sl_emlib_gpio_init_motion_input_config.h"

extern "C" {
#include "app/framework/util/attribute-table.h"
#include "app/framework/plugin/reporting/reporting.h"
}

#include "OccupancyTask.h"

static OccupancyTask *task;
static bool interrupt = false;

static void int_callback(unsigned char intNo)
{
    task->log_debug("callback");
    GPIO_IntDisable(1<<SL_EMLIB_GPIO_INIT_MOTION_INPUT_PIN);
    interrupt = true;
    task->request_process(0);
}

extern "C" void occupancy_task_init(uint8_t endpoint, uint32_t timeout)
{
    task = new OccupancyTask(endpoint, OccupancyTask::LOG_DEBUG, timeout);
}

OccupancyTask::OccupancyTask(uint8_t endpoint, enum LOG_LEVEL log_level, uint32_t _timeout)
  : Task(endpoint, "Occupancy", log_level), occupancy(0), timeout(_timeout), blanking_time(3*_timeout/4), motion_state(MOTION_NONE)
{
    GPIO_ExtIntConfig(SL_EMLIB_GPIO_INIT_MOTION_INPUT_PORT, SL_EMLIB_GPIO_INIT_MOTION_INPUT_PIN,
        SL_EMLIB_GPIO_INIT_MOTION_INPUT_PIN, 1, 0, true); // rising edge only, interrupt enabled
    GPIOINT_CallbackRegister(SL_EMLIB_GPIO_INIT_MOTION_INPUT_PIN, int_callback);
}

void OccupancyTask::process()
{
    log_debug("process");

    // Override min report interval if not set to 0
    // Zigbee2mqtt sets it to 10
    EmberAfPluginReportingEntry entry;
    for(int i = 0; i < REPORT_TABLE_SIZE; i++){
        sli_zigbee_af_reporting_get_entry(i, &entry);
        if(entry.endpoint == endpoint && entry.clusterId == ZCL_OCCUPANCY_SENSING_CLUSTER_ID)
        {
            //log_debug("r %d %d", entry.attributeId, entry.data.reported.minInterval);
            if (entry.data.reported.minInterval != 0)
            {
                log_debug("updating rep int to 0");
                entry.data.reported.minInterval = 0;
                sli_zigbee_af_reporting_set_entry(i, &entry);
            }

            break;
        }
    }

    if (interrupt)
    {
        interrupt = false;
        log_debug("Motion interrupt");
        if (motion_state == MOTION_NONE)
        {
            motion_state = MOTION_BLANKING;
            update_occupancy(true);
        }
        else
            motion_state = MOTION_BLANKING;

        request_process(blanking_time); // Start blanking timeout, leave interrupts disabled
    }
    else
    {
        // Called due to timer
        switch (motion_state)
        {
        case MOTION_BLANKING:
            log_debug("End blanking");
            // End of blanking period
            GPIO_IntClear(1<<SL_EMLIB_GPIO_INIT_MOTION_INPUT_PIN); // Clear any spurious interrupt
            GPIO_IntEnable(1<<SL_EMLIB_GPIO_INIT_MOTION_INPUT_PIN);
            motion_state = MOTION_DELAY;
            request_process(timeout - blanking_time); // Delay state until the end of delay period
            break;
        case MOTION_DELAY:
            log_debug("End delay");
            motion_state = MOTION_NONE;
            update_occupancy(false);
            break;
        case MOTION_NONE:
        default:
            break;
    }
  }
}

void OccupancyTask::update_occupancy(bool _occupancy)
{
    occupancy = _occupancy ? 1: 0;
    EmberAfStatus result = emberAfWriteServerAttribute(endpoint, ZCL_OCCUPANCY_SENSING_CLUSTER_ID, ZCL_OCCUPANCY_ATTRIBUTE_ID,
        &occupancy, ZCL_BITMAP8_ATTRIBUTE_TYPE);
    log_debug("update_occupancy result=%d, state=%d", result, occupancy);
}

#endif
