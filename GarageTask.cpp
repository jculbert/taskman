#include "AppConfig.h"

#ifdef GARAGE_TASK

#include "em_gpio.h"
#include "gpiointerrupt.h"
#include "sl_emlib_gpio_init_contact_input_config.h"
#include "sl_emlib_gpio_init_relay_output_config.h"

extern "C" {
#include "app/framework/util/attribute-table.h"
#include "app/framework/common/zigbee_app_framework_event.h"
}

#include "GarageTask.h"

static GarageTask *task;
static sl_zigbee_event_t relay_pulse_event;

extern "C" void garage_task_init(uint8_t endpoint)
{
    GarageTask* tmp = new GarageTask(endpoint, GarageTask::LOG_DEBUG);
}

static void relay_pulse_event_handler(sl_zigbee_event_t *event)
{
    GPIO_PinOutClear(SL_EMLIB_GPIO_INIT_RELAY_OUTPUT_PORT, SL_EMLIB_GPIO_INIT_RELAY_OUTPUT_PIN);
}

static void int_callback(unsigned char intNo)
{
    task->log_debug("contact interrupt");
    task->request_process(200); // Delay to allow for bounce
}

extern "C" {
uint32_t command_handler(sl_service_opcode_t opcode, sl_service_function_context_t *context)
{
    assert(opcode == SL_SERVICE_FUNCTION_TYPE_ZCL_COMMAND);
    EmberAfClusterCommand *cmd = (EmberAfClusterCommand *)context->data;
    switch (cmd->commandId)
    {
        case ZCL_WINDOW_COVERING_UP_OPEN_COMMAND_ID:
            task->command = GarageTask::COMMAND_OPEN;
            break;
        case ZCL_WINDOW_COVERING_DOWN_CLOSE_COMMAND_ID:
            task->command = GarageTask::COMMAND_CLOSE;
            break;
        case ZCL_WINDOW_COVERING_STOP_COMMAND_ID:
            task->command = GarageTask::COMMAND_STOP;
            break;
        default:
            return EMBER_ZCL_STATUS_UNSUP_COMMAND;
    }

    task->request_process(0);
    emberAfSendDefaultResponse(cmd, EMBER_ZCL_STATUS_SUCCESS);
    return  EMBER_ZCL_STATUS_SUCCESS;
}
}

GarageTask::GarageTask(uint8_t endpoint, enum LOG_LEVEL log_level)
  : Task(endpoint, "Garage", log_level),
    command(COMMAND_NONE), pulse_relay_duration(500), target_changed(false)
{
    task = this;
    sl_zigbee_event_init(&relay_pulse_event, relay_pulse_event_handler);

    sl_status_t status = sl_zigbee_subscribe_to_zcl_commands(ZCL_WINDOW_COVERING_CLUSTER_ID, 0xFFFF,
        ZCL_DIRECTION_CLIENT_TO_SERVER, command_handler);
    log_debug("subscribe cmd status = %d", status);

    GPIOINT_CallbackRegister(SL_EMLIB_GPIO_INIT_CONTACT_INPUT_PIN, int_callback);
    GPIO_ExtIntConfig(SL_EMLIB_GPIO_INIT_CONTACT_INPUT_PORT, SL_EMLIB_GPIO_INIT_CONTACT_INPUT_PIN, SL_EMLIB_GPIO_INIT_CONTACT_INPUT_PIN, 1, 1, true); // rising and falling edge, interrupt enabled

    door_closed = GPIO_PinInGet(SL_EMLIB_GPIO_INIT_CONTACT_INPUT_PORT, SL_EMLIB_GPIO_INIT_CONTACT_INPUT_PIN) == 1;
    request_process(30000);
}

void GarageTask::process()
{
    log_debug("process");

    // Request for periodic refresh
    request_process(60000);

    door_closed = GPIO_PinInGet(SL_EMLIB_GPIO_INIT_CONTACT_INPUT_PORT, SL_EMLIB_GPIO_INIT_CONTACT_INPUT_PIN) == 1;
    log_debug("door_closed = %d", door_closed);

    bool pulse_relay = false;
    switch (command)
    {
        case COMMAND_OPEN:
            command = COMMAND_NONE;
            log_debug("Process: OPEN");
            if (door_closed)
                pulse_relay = true;
            else
                log_debug("Door is open, ignoring command OPEN");
            break;
        case COMMAND_CLOSE:
            command = COMMAND_NONE;
            log_debug("Process: CLOSE");
            if (!door_closed)
                pulse_relay = true;
            else
                log_debug("Door is closed, ignoring command CLOSE");
            break;
        case COMMAND_STOP:
            command = COMMAND_NONE;
            log_debug("Process: STOP");
            pulse_relay = true;
            break;
        default:
            break;
    }
    if (pulse_relay)
    {
        GPIO_PinOutSet(SL_EMLIB_GPIO_INIT_RELAY_OUTPUT_PORT, SL_EMLIB_GPIO_INIT_RELAY_OUTPUT_PIN);
        sl_zigbee_event_set_delay_ms(&relay_pulse_event, pulse_relay_duration); // will clear the relay
    }

    uint8_t lift_percent = door_closed ? 100: 0;
    EmberAfStatus result = emberAfWriteServerAttribute(endpoint, ZCL_WINDOW_COVERING_CLUSTER_ID, ZCL_CURRENT_LIFT_PERCENTAGE_ATTRIBUTE_ID,
        &lift_percent, ZCL_INT8U_ATTRIBUTE_TYPE);
    log_debug("update state result=%d, closed=%d", result, door_closed);
}

#endif // GARAGE_TASK
