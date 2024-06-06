#include "AppConfig.h"

#ifdef BATTERY_TASK

// IADC code copied from https://github.com/SiliconLabs/peripheral_examples/blob/master/series2/iadc/iadc_single_interrupt/src/main_single_interrupt.c

#include "em_cmu.h"
#include "em_emu.h"
#include "em_iadc.h"

extern "C" {
#include "app/framework/util/attribute-table.h"
}

#include "BatteryTask.h"

static BatteryTask *task;

static uint32_t vbat_mv;

extern "C" void battery_task_init(uint8_t endpoint, uint32_t nominal_mv, uint32_t refresh_minutes)
{
    new BatteryTask(endpoint, BatteryTask::LOG_DEBUG, nominal_mv, refresh_minutes);
}

void iadc_irqhandler(void)
{
  // Read a result from the FIFO
  IADC_Result_t sample = IADC_pullSingleFifoResult(IADC0);

  // 1210 for the bandgap reference, and 4 because we convert Vdd/4
  vbat_mv = (sample.data * 4 * 1210) / 0xFFF;
  task->log_debug("vbat %d", vbat_mv);

  /*
   * Clear the single conversion complete interrupt.  Reading FIFO
   * results does not do this automatically.
   */
  IADC_clearInt(IADC0, IADC_IF_SINGLEDONE);

  task->request_process(0);
}

static void init_iadc(void)
{
    IADC_Init_t init = IADC_INIT_DEFAULT;
    IADC_AllConfigs_t initAllConfigs = IADC_ALLCONFIGS_DEFAULT;
    IADC_InitSingle_t initSingle = IADC_INITSINGLE_DEFAULT;
    IADC_SingleInput_t singleInput = IADC_SINGLEINPUT_DEFAULT;

    CMU_ClockEnable(cmuClock_IADC0, true);

    // Use the FSRC0 as the IADC clock so it can run in EM2
    CMU_ClockSelectSet(cmuClock_IADCCLK, cmuSelect_FSRCO);

#if 0
    // Set the prescaler needed for the intended IADC clock frequency
    // Set CLK_ADC to 10 MHz
    const uint32_t CLK_SRC_ADC_FREQ = 20000000;
    init.srcClkPrescale = IADC_calcSrcClkPrescale(IADC0, CLK_SRC_ADC_FREQ, 0);
#endif

    // Set the prescaler needed for the intended IADC clock frequency
    //const uint32_t CLK_ADC_FREQ = 10000000;  // CLK_ADC - 10 MHz max in normal mode
    const uint32_t CLK_ADC_FREQ = 10000000;  // CLK_ADC - 10 MHz max in normal mode
    init.srcClkPrescale = IADC_calcSrcClkPrescale(IADC0, CLK_ADC_FREQ, 0);

    // Shutdown between conversions to reduce current
    init.warmup = iadcWarmupNormal;

    /*
     * Configuration 0 is used by both scan and single conversions by
     * default.  Use internal bandgap as the reference and specify the
     * reference voltage in mV.
     *
     * Resolution is not configurable directly but is based on the
     * selected oversampling ratio (osrHighSpeed), which defaults to
     * 2x and generates 12-bit results.
     */
    initAllConfigs.configs[0].reference = iadcCfgReferenceInt1V2;
    initAllConfigs.configs[0].vRef = 1210;
    initAllConfigs.configs[0].osrHighSpeed = iadcCfgOsrHighSpeed2x;
    initAllConfigs.configs[0].analogGain = iadcCfgAnalogGain1x;

    /*
     * CLK_SRC_ADC must be prescaled by some value greater than 1 to
     * derive the intended CLK_ADC frequency.
     *
     * Based on the default 2x oversampling rate (OSRHS)...
     *
     * conversion time = ((4 * OSRHS) + 2) / fCLK_ADC
     *
     * ...which results in a maximum sampling rate of 833 ksps with the
     * 2-clock input multiplexer switching time is included.
     */
    initAllConfigs.configs[0].adcClkPrescale = IADC_calcAdcClkPrescale(IADC0,
        CLK_ADC_FREQ, 0, iadcCfgModeNormal, init.srcClkPrescale);

    /*
     * Specify the input channel.  When negInput = iadcNegInputGnd, the
     * conversion is single-ended.
     */
    singleInput.posInput   = iadcPosInputAvdd;
    singleInput.negInput   = iadcNegInputGnd;

    // Allocate the analog bus for ADC0 inputs
    //GPIO->IADC_INPUT_0_BUS |= IADC_INPUT_0_BUSALLOC; // Needed?

    // Initialize IADC
    IADC_init(IADC0, &init, &initAllConfigs);

    // Initialize a single-channel conversion
    IADC_initSingle(IADC0, &initSingle, &singleInput);

    // Clear any previous interrupt flags
    IADC_clearInt(IADC0, _IADC_IF_MASK);

    // Enable single-channel done interrupts
    IADC_enableInt(IADC0, IADC_IEN_SINGLEDONE);

    // Enable IADC interrupts
    NVIC_ClearPendingIRQ(IADC_IRQn);
    NVIC_EnableIRQ(IADC_IRQn);
}

BatteryTask::BatteryTask(uint8_t endpoint, enum LOG_LEVEL log_level, uint32_t _nominal_mv, uint32_t _refresh_minutes)
  : Task(endpoint, "Battery", log_level),  nominal_mv(_nominal_mv), refresh_ms(_refresh_minutes * 60000),
    waiting_adc(false), battery_percent(0)
{
    task = this;
    request_process(10000);
}

void BatteryTask::process()
{
    log_debug("process");

    if (waiting_adc)
    {
        waiting_adc = false;

        // reset ADC and disable clock between measurements to reduce power
        IADC_reset(IADC0);
        CMU_ClockSelectSet(cmuClock_IADCCLK, cmuSelect_Disabled);
        CMU_ClockEnable(cmuClock_IADC0, false);

        // battery percent if 1/2 percent units, so 200 is 100 percent
        battery_percent = (200*vbat_mv + nominal_mv/2) / nominal_mv;
        if (battery_percent > 200)
            battery_percent = 200;

        EmberAfStatus result = emberAfWriteServerAttribute(endpoint, ZCL_POWER_CONFIG_CLUSTER_ID, ZCL_BATTERY_PERCENTAGE_REMAINING_ATTRIBUTE_ID,
            &battery_percent, ZCL_INT8U_ATTRIBUTE_TYPE);
        log_debug("update state result=%d, battery_pecent=%d", result, battery_percent);

        request_process(refresh_ms);
    }
    else
    {
        waiting_adc = true;
        init_iadc();
        IADC_command(IADC0, iadcCmdStartSingle);
    }
}

#endif // BATTERY_TASK
