#include "AppConfig.h"

#ifdef TMP102_TASK

#include "em_i2c.h"
#include "sl_i2cspm.h"
#include "sl_i2cspm_instances.h"

extern "C" {
#include "app/framework/util/attribute-table.h"
}

#include "Tmp102Task.h"

#define TMP102_ADDRESS (0x48)

static Tmp102Task *task;

extern "C" void tmp102_task_init(uint8_t endpoint, uint32_t period_mins)
{
    Tmp102Task* tmp = new Tmp102Task(endpoint, Tmp102Task::LOG_DEBUG, period_mins);
}

/***************************************************************************//**
 * Function to perform I2C transactions on the Tmp102
 *
 ******************************************************************************/
static I2C_TransferReturn_TypeDef Tmp102Transaction(uint16_t flag,
  uint8_t *writeCmd, size_t writeLen, uint8_t *readCmd, size_t readLen)
{
  I2C_TransferSeq_TypeDef seq;
  I2C_TransferReturn_TypeDef ret;

  seq.addr = TMP102_ADDRESS << 1;
  seq.flags = flag;

  switch (flag) {
    // Send the write command from writeCmd
    case I2C_FLAG_WRITE:
      seq.buf[0].data = writeCmd;
      seq.buf[0].len  = writeLen;
      break;

    // Receive data into readCmd of readLen
    case I2C_FLAG_READ:
      seq.buf[0].data = readCmd;
      seq.buf[0].len  = readLen;
      break;

      // Send the write command from writeCmd
      // and receive data into readCmd of readLen
    case I2C_FLAG_WRITE_READ:
      seq.buf[0].data = writeCmd;
      seq.buf[0].len  = writeLen;

      seq.buf[1].data = readCmd;
      seq.buf[1].len  = readLen;
      break;

    default:
      return i2cTransferUsageFault;
  }

  // Perform the transfer and return status from the transfer
  ret = I2CSPM_Transfer(sl_i2cspm_i2c_tmp102, &seq);

  return ret;
}

Tmp102Task::Tmp102Task(uint8_t endpoint, enum LOG_LEVEL log_level, uint32_t period_mins)
  : Task(endpoint, "Temp", log_level), temperature(-100), period_ms(period_mins*60000), state(STATE_TRIGGER_READING)
{
    task = this;

    // Put Tmp102 in powerdown mode
    // Write 0x01 in pointer register to address config register
    // and 0x01 to config register
    uint8_t write[] = {0x01, 0x01};
    I2C_TransferReturn_TypeDef status = Tmp102Transaction(I2C_FLAG_WRITE, write, sizeof(write), NULL, 0);
    log_debug("Tmp102 tran = %d", status);

    // First run
    request_process(10000);
}

void Tmp102Task::process()
{
    log_debug("process");

    switch(state)
    {
        case STATE_TRIGGER_READING:
        {
            // Write 0x81 to config register to trigger one shot reading
            // and remain in power down mode
            uint8_t write[] = {0x01, 0x81};
            Tmp102Transaction(I2C_FLAG_WRITE, write, sizeof(write), NULL, 0);
            //log_debug("TmpTMP102_TASK102 tran = %d", status);

            // read value after one second (conversion time is approx 25 ms
            state = STATE_READ;
            request_process(1000);
            break;
        }
        case STATE_READ:
        {
            // Read two bytes from address 0
            //  1. Write 0 to pointer register to set address for read
            //  2. read 2 bytes
            uint8_t write[] = {0x00};
            uint8_t read[2];
            Tmp102Transaction(I2C_FLAG_WRITE_READ, write, sizeof(write), read, sizeof(read));

            // Temperature is coded in signed 12 bits, 0.0625 C per bit
            // Construct raw value with sign extention down to lower 12 bits
            int16_t raw = ((int16_t)(read[0]<<8 | read[1])) / 16;
            // Temperature as C * 100 is: (raw to lower 12 bits) * 6.25
            temperature = (raw * 625) / 100;
            EmberAfStatus result = emberAfWriteServerAttribute(endpoint, ZCL_TEMP_MEASUREMENT_CLUSTER_ID, ZCL_TEMP_MEASURED_VALUE_ATTRIBUTE_ID,
                (uint8_t *) &temperature, ZCL_INT16S_ATTRIBUTE_TYPE);
            log_debug("update_temperature result=%d, state=%d", result, temperature);

            // trigger next reading after delay
            state = STATE_TRIGGER_READING;
            request_process(period_ms);
            break;
        }
        default:
            break;
    }
}

#endif // TMP102_TASK
