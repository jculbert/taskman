#include "AppConfig.h"

#ifdef EXCELITAS_OCCUPANCY_TASK

#include "em_gpio.h"
#include "gpiointerrupt.h"
#include "sl_emlib_gpio_init_serin_config.h"
#include "sl_emlib_gpio_init_motion_input_config.h"

#include "sl_udelay.h"

extern "C" {
#include "app/framework/util/attribute-table.h"
#include "app/framework/plugin/reporting/reporting.h"
}

#include "ExcelOccupancyTask.h"

static ExcelOccupancyTask *task;
static bool interrupt = false;

extern "C" void exceloccupancy_task_init(uint8_t endpoint, uint32_t timeout)
{
    ExcelOccupancyTask* tmp = new ExcelOccupancyTask(endpoint, ExcelOccupancyTask::LOG_DEBUG, timeout);
}

static void int_callback(unsigned char intNo)
{
    task->log_debug("interrupt");
    GPIO_IntDisable(1<<SL_EMLIB_GPIO_INIT_MOTION_INPUT_PIN);
    interrupt = true;
    task->request_process(0);
}

static void inline set_serin()
{
    GPIO_PinOutSet(SL_EMLIB_GPIO_INIT_SERIN_PORT, SL_EMLIB_GPIO_INIT_SERIN_PIN);
}

static void inline clear_serin()
{
    GPIO_PinOutClear(SL_EMLIB_GPIO_INIT_SERIN_PORT, SL_EMLIB_GPIO_INIT_SERIN_PIN);
}

static void clear_direct_link()
{
    // Pull the detector interrupt output low to clear interrupt state
    GPIO_PinModeSet(SL_EMLIB_GPIO_INIT_MOTION_INPUT_PORT,
        SL_EMLIB_GPIO_INIT_MOTION_INPUT_PIN, gpioModePushPull, 0);

    // Restore for interrupt input
    GPIO_PinModeSet(SL_EMLIB_GPIO_INIT_MOTION_INPUT_PORT,
        SL_EMLIB_GPIO_INIT_MOTION_INPUT_PIN, SL_EMLIB_GPIO_INIT_MOTION_INPUT_MODE,
        SL_EMLIB_GPIO_INIT_MOTION_INPUT_DOUT);
}

static void init_detector()
{
    uint32_t config25bits = 0;

    // BPF threshold 8 bits, 24:17
    config25bits |= 100 << 17;

    // Blind Time, 4 bits, 16:13, 0.5 s and 8 s in steps of 0.5 s.
    config25bits |= 0x01 << 13; // Small value, not needed, we implement blanking logic

    // Pulse counter, 2 bits, 12:11, 1 to 4 pulses to trigger detection
    config25bits |= 1 << 11;

    // Window time, 2 bits, 10:9, 2 s up to 8 s in intervals of 2 s
    //config25bits |= 0; // min window for now

    // Operation Modes, 2 bits, 8:7
     config25bits |= 2 << 7; // 2 for Wake Up, interrupt, mode

    // Signal Source, 2 bits, 6:5,
    //config25bits |= 0; // PIR BPF

    // Reserved, 2 bits, 4:3, must be set to 2
    config25bits |= 2 << 3;

    // HPF Cut-Off, 1 bit, bit 2
    config25bits |= 0; // 0 for 0.4 Hz

    // Reserver, 1 bit, bit 1, must be set to 0
    //config25bits |= 0;

    // Count Mode, 1 bit, bit 0
    config25bits |= 1; // no zero crossing required

    // Send the 25 bit bit train in a loop
    // Start of each bit is marked by low to high
    // for a 0 value, output returns to zero after start pulse
    // for a 1 value, output is held for 80 usec or more
    clear_serin();
    sl_udelay_wait(1000); // Start output low for 1 ms

    const int32_t num_bits = 25;
    uint32_t mask = 1 << 24;
    for (int32_t i = 0; i < num_bits; i++)
    {
        clear_serin();
        sl_udelay_wait(5);
        set_serin();
        sl_udelay_wait(5);

        if ((config25bits & mask) == 0)
        {
            // bit value is zero
            clear_serin();
        }
        sl_udelay_wait(100); // hold bit value
        mask >>= 1;
    }

    // Latch the data
    clear_serin();
    sl_udelay_wait(1000);
    task->log_debug("PIR init done");
}

ExcelOccupancyTask::ExcelOccupancyTask(uint8_t endpoint, enum LOG_LEVEL log_level, uint32_t _timeout)
  : Task(endpoint, "Occupancy", log_level), occupancy(0), timeout(_timeout), blanking_time(3*_timeout/4), state(STATE_IDLE)
{
    task = this;
    init_detector();

    // Note, the input GPIO has been setup by app config to input no pull
    GPIOINT_CallbackRegister(SL_EMLIB_GPIO_INIT_MOTION_INPUT_PIN, int_callback);
    GPIO_ExtIntConfig(SL_EMLIB_GPIO_INIT_MOTION_INPUT_PORT, SL_EMLIB_GPIO_INIT_MOTION_INPUT_PIN,
        SL_EMLIB_GPIO_INIT_MOTION_INPUT_PIN, 1, 0, true); // rising edge only, interrupt enabled
    clear_direct_link();
}

void ExcelOccupancyTask::process()
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
      if (state == STATE_IDLE)
      {
        state = STATE_BLANKING;
        update_occupancy(true);
      }
      else
        state = STATE_BLANKING;

      log_debug("PIR Blanking");
      request_process(blanking_time); // Start/restart blanking timeout, leave interrupts disabled
      return;
    }

    // Called due to timer
    switch (state)
    {
    case STATE_BLANKING:
        // End of blanking period
        clear_direct_link();
        GPIO_IntClear(1<<SL_EMLIB_GPIO_INIT_MOTION_INPUT_PIN); // Clear any spurious interrupt
        GPIO_IntEnable(1<<SL_EMLIB_GPIO_INIT_MOTION_INPUT_PIN);
        state = STATE_DELAY;
        log_debug("PIR Delay");
        request_process(timeout - blanking_time); // Delay state until the end of delay period
        break;
    case STATE_DELAY:
        state = STATE_IDLE;
        update_occupancy(false);
        break;
    case STATE_IDLE:
    default:
        break;
    }
}

void ExcelOccupancyTask::update_occupancy(bool _occupancy)
{
    occupancy = _occupancy ? 1: 0;
    EmberAfStatus result = emberAfWriteServerAttribute(endpoint, ZCL_OCCUPANCY_SENSING_CLUSTER_ID, ZCL_OCCUPANCY_ATTRIBUTE_ID,
        &occupancy, ZCL_BITMAP8_ATTRIBUTE_TYPE);
    log_debug("update_occupancy result=%d, state=%d", result, occupancy);
}

#endif // EXCELITAS_OCCUPANCY
