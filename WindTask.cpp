#include "AppConfig.h"

#ifdef WIND_TASK

extern "C" {
#include "app/framework/util/attribute-table.h"
}

#include "em_chip.h"
#include "em_cmu.h"
#include "em_device.h"
#include "em_emu.h"
#include "em_gpio.h"
#include "em_pcnt.h"
#include "em_prs.h"

#include "sl_emlib_gpio_init_pulse_input_config.h"

#include "WindTask.h"

static WindTask *task;

// The PCNT (Pulse Counter) related code was copied from
// https://github.com/SiliconLabs/peripheral_examples/tree/master/series2/pcnt/pcnt_extclk_single_overflow

// Wind speed is calculated from a pulse count for an overall period
// Gust is determined by a max pulse count change over each sample period
#define SAMPLE_PERIOD_SECS (4)
#define NUM_SAMPLES (60)
#define TOTAL_PERIOD_SECS (SAMPLE_PERIOD_SECS * NUM_SAMPLES)

#define PCNT_PRS_Ch0    (0)

extern "C" void wind_task_init(uint8_t endpoint)
{
    new WindTask(endpoint, WindTask::LOG_DEBUG);
}

/***************************************************************************//**
 * @brief PCNT0 interrupt handler
 *        This function acknowledges the interrupt request.
 *        It replaces a default one with weak linkage.
 *        This is part of a builtin PRS mechanism where interrupts are used
 *        with GPIO inputs for signaling.
 ******************************************************************************/
void PCNT0_IRQHandler(void)
{
  // Acknowledge interrupt
  PCNT_IntClear(PCNT0, PCNT_IF_OF);
}

/***************************************************************************//**
 * @brief Initialize PCNT0
 *        This function initializes pulse counter 0 and sets up the
 *        external single mode; PCNT0 overflow interrupt is also configured
 *        in this function
 ******************************************************************************/
static void init_pcnt(void)
{
  PCNT_Init_TypeDef pcntInit = PCNT_INIT_DEFAULT;

  CMU_ClockEnable(cmuClock_PCNT0, true);

  pcntInit.mode     = pcntModeDisable;    // Initialize with PCNT disabled
  pcntInit.s1CntDir = false;        // S1 does not affect counter direction,
                                    // using default init setting; count up
  pcntInit.s0PRS    = PCNT_PRS_Ch0;

  pcntInit.top = 0xffff;

  // Enable PRS Channel 0
  PCNT_PRSInputEnable(PCNT0, pcntPRSInputS0, true);

  // Init PCNT0
  PCNT_Init(PCNT0, &pcntInit);

  // Set mode to externally clocked single input
  PCNT_Enable(PCNT0, pcntModeExtSingle);

  // Change to the external clock
  CMU->PCNT0CLKCTRL = CMU_PCNT0CLKCTRL_CLKSEL_PCNTS0;

  /*
   * When the PCNT operates in externally clocked mode and switching external
   * clock source, a few clock pulses are required on the external clock to
   * synchronize accesses to the externally clocked domain. This example uses
   * push button PB0 via GPIO/PRS/PCNT0_S0IN for the external clock, which would
   * require multiple button presses to sync the PCNT registers and clock
   * domain, during which button presses would not be counted.
   *
   * To get around this, such that each button push is recognized, firmware
   * can use the PRS software pulse triggering mechanism to generate
   * those first few pulses. This allows the first actual button press and
   * subsequent button presses to be properly counted.
   */
  while (PCNT0->SYNCBUSY) {
    PRS_PulseTrigger(1 << PCNT_PRS_Ch0);
  }

  // Enable Interrupt
  //PCNT_IntEnable(PCNT0, PCNT_IEN_OF);

  // Clear PCNT0 pending interrupt
  //NVIC_ClearPendingIRQ(PCNT0_IRQn);

  // Enable PCNT0 interrupt in the interrupt controller
  //NVIC_EnableIRQ(PCNT0_IRQn);
}

/***************************************************************************//**
 * @brief Initialize PRS
 *        This function sets up GPIO PRS which links pulse_input to PCNT0 PRS0
 ******************************************************************************/
static void init_prs(void)
{
  CMU_ClockEnable(cmuClock_PRS, true);

  // Set up GPIO PRS
  PRS_SourceAsyncSignalSet(PCNT_PRS_Ch0, PRS_ASYNC_CH_CTRL_SOURCESEL_GPIO,
                           SL_EMLIB_GPIO_INIT_PULSE_INPUT_PIN);
}

WindTask::WindTask(uint8_t endpoint, enum LOG_LEVEL log_level)
  : Task(endpoint, "Battery", log_level), pulse_cnt_start(0), pulse_cnt(0), pulse_cnt_max_delta(0),
    num_samples(0), wind(0), gust(0)
{
    task = this;

    // Configure pulse_input for external interrupt
    // Needed for GPIO to work with PRS?
    GPIO_ExtIntConfig(SL_EMLIB_GPIO_INIT_PULSE_INPUT_PORT, SL_EMLIB_GPIO_INIT_PULSE_INPUT_PIN,
        SL_EMLIB_GPIO_INIT_PULSE_INPUT_PIN, false, false, false);

    init_prs();
    init_pcnt();

    request_process(SAMPLE_PERIOD_SECS * 1000);

    pulse_cnt_start = PCNT_CounterGet(PCNT0);

}

void WindTask::process_wind_data()
{
    // Based on pulse counts every 4 seconds for 240 seconds (60 samples)
    // Wind is calculated from finalSample and gust from maxSample
    const uint16_t hzToKph = 4; // (2.5 * (8.0 / 5.0)), // From wind sensor spec 1 Hz = 2.5 mph
    static uint16_t gustHistory[] = {0, 0, 0};
    const uint16_t gustHistoryLen = sizeof(gustHistory) / sizeof(gustHistory[0]);
    static uint16_t gustHistoryHead = 0;

    wind = (hzToKph * pulse_cnt + (TOTAL_PERIOD_SECS/2) ) / TOTAL_PERIOD_SECS;
    gustHistory[gustHistoryHead] = (hzToKph * pulse_cnt_max_delta + (SAMPLE_PERIOD_SECS/2) ) / SAMPLE_PERIOD_SECS;
    gustHistoryHead = (gustHistoryHead + 1) % gustHistoryLen;
    gust = 0;
    for (uint16_t i = 0; i < gustHistoryLen; i++)
    {
        if (gust < gustHistory[i])
            gust = gustHistory[i];
    }
    //chip::DeviceLayer::PlatformMgr().ScheduleWork(UpdateClusterState, reinterpret_cast<intptr_t>(nullptr));
}

void WindTask::process()
{
    uint16_t last_cnt = pulse_cnt;

    // Note, the pulse counter runs without restart and will wrap at 0xffff
    // Here we get the count since start of cycle by removing an offset
    // Note, the counter wrapping does not affect this calculation
    pulse_cnt = PCNT_CounterGet(PCNT0) - pulse_cnt_start;

    uint16_t delta = pulse_cnt - last_cnt;
    log_debug("Wind Process cnt=%d, delta=%d", pulse_cnt, delta);
    if (delta > pulse_cnt_max_delta)
        pulse_cnt_max_delta = delta;

    if (++num_samples >= NUM_SAMPLES)
    {
        process_wind_data();
        num_samples = 0;
        pulse_cnt_start = PCNT_CounterGet(PCNT0);
        pulse_cnt = 0;
        pulse_cnt_max_delta = 0;

        // Note, resetting the counter was found to cause a long spin wait
        // and in some cases it did not return. So we let the counter
        // run and wrap and take this into account as explained above.
        //PCNT_CounterReset(PCNT0); // This reloads the default top value
        //PCNT_CounterTopSet(PCNT0, 0, 0xffff);
        //SILABS_LOG("Wind reset done");
    }

    request_process(SAMPLE_PERIOD_SECS * 1000);
}


#endif //WIND_TASK
