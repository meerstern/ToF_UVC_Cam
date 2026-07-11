/**************************************************************************************************
* Copyright © 2024 ams-OSRAM AG                                                                   *
* All rights are reserved.                                                                        *
*                                                                                                 *
* FOR FULL LICENSE TEXT SEE LICENSES-MIT.TXT                                                      *
*                                                                                                 *
**************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/sync.h"
#include "tmf8829_shim.h"
#include "tmf8829.h"
#include "UVC_RP2040.h"

static void (*user_handler)(void) = NULL;
static uint32_t irq_state;

static void gpio_irq_callback(uint gpio, uint32_t events)
{
    if (gpio == INTERRUPT_PIN && (events & GPIO_IRQ_EDGE_FALL))
    {
        if (user_handler)
        {
            user_handler();
        }
    }
}

void delayInMicroseconds ( uint32_t wait )
{
  sleep_us( wait );
}

uint32_t getSysTick ( )
{
  return (uint32_t)time_us_64();
}

uint8_t readProgramMemoryByte ( uint32_t address )
{
  return *(const uint8_t *)address;
}

void enablePinHigh ( void * dptr )
{
  (void)dptr; // not used here
  gpio_put( ENABLE_PIN, HIGH );  
}

void enablePinLow ( void * dptr )
{
  (void)dptr; // not used here
  gpio_put( ENABLE_PIN, LOW );   
}

void configurePins ( void * dptr )
{
  (void)dptr; // not used here
  // configure ENABLE pin and interupt pin
  gpio_init(ENABLE_PIN);
  gpio_set_dir(ENABLE_PIN, GPIO_OUT);
  gpio_init(INTERRUPT_PIN);
  gpio_set_dir(INTERRUPT_PIN, GPIO_IN);
  gpio_init(TRIGGER_INTERRUPT_PIN);
  gpio_set_dir(TRIGGER_INTERRUPT_PIN, GPIO_IN);
}

void i2cOpen ( void * dptr, uint32_t i2cClockSpeedInHz )
{
  (void)dptr; // not used here
  i2c_init(I2C_PORT, i2cClockSpeedInHz);    
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);
}

void i2cClose ( void * dptr )
{
  (void)dptr; // not used here
  gpio_set_function(I2C_SDA, GPIO_FUNC_NULL);
  gpio_set_function(I2C_SCL, GPIO_FUNC_NULL);
}

void printChar ( char c ) 
{
  printf("%c", c );
}

void printInt ( int32_t i )
{
  printf("%d", i);
}

void printUint ( uint32_t i )
{
  printf("%d", i);
}

void printUintHex ( uint32_t i )
{
  printf("%X", i);
}

void printStr ( char * str )
{
  printf("%s", str);
}

void printLn ( void )
{
  printf("\n");
}

void inputOpen ( uint32_t baudrate )
{
  
}

void inputClose ( )
{
  
}

int8_t inputGetKey(char *c)
{
    int ch = getchar_timeout_us(0);
    if (ch != PICO_ERROR_TIMEOUT)
    {
        *c = (char)ch;
        return 1;
    }
    *c = 0;
    return 0;
}

void printConstStr ( const char * str )
{
  printf("%s", str);
}

void pinOutput ( uint8_t pin )
{
  /* define a pin as output */
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_OUT);
}

void pinInput ( uint8_t pin )
{ 
  /* define a pin as input */
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_IN);
}

void setInterruptHandler( void (* handler)( void ) )
{
    user_handler = handler;
    gpio_set_irq_enabled_with_callback(
        INTERRUPT_PIN,
        GPIO_IRQ_EDGE_FALL,  // FALLING
        true,
        &gpio_irq_callback
    );
}

void disableInterruptHandler( uint8_t pin )
{
  gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_FALL, false);
}

void disableInterrupts ( void )
{
  irq_state = save_and_disable_interrupts();
}

void enableInterrupts ( void )
{
  restore_interrupts(irq_state);
}

/*char inputGetKey ( )
{
  int ch = getchar_timeout_us(0);
  if (ch != PICO_ERROR_TIMEOUT)
  {
    return (char)ch;
  }
  return 0;
}*/

// ----------------------------------------- rx/tx ---------------------------------------
int8_t txReg ( void *dptr, uint8_t slaveAddr, uint8_t regAddr, uint16_t toTx, const uint8_t *txData )
{
    return i2cTxReg(dptr, slaveAddr, regAddr, toTx, txData);
}

int8_t rxReg ( void *dptr, uint8_t slaveAddr, uint8_t regAddr, uint16_t toRx, uint8_t *rxData )
{
    return i2cRxReg(dptr, slaveAddr, regAddr, toRx, rxData);
}

// ----------------------------------------- i2c ---------------------------------------

static int8_t i2cTxOnly ( uint8_t logLevel, uint8_t slaveAddr, uint8_t regAddr, uint16_t toTx, const uint8_t * txData )
{
    int8_t res = I2C_SUCCESS;
    uint16_t addr = regAddr;

    uint8_t buffer[ARDUINO_MAX_I2C_TRANSFER];

    do
    {
        uint8_t tx;

        if (toTx > ARDUINO_MAX_I2C_TRANSFER - 1)
            tx = ARDUINO_MAX_I2C_TRANSFER - 1;
        else
            tx = toTx;

        buffer[0] = regAddr;
        if (tx)
        {
            memcpy(&buffer[1], txData, tx);
        }

        if (logLevel & TMF8829_LOG_LEVEL_I2C)
        {
            PRINT_STR("I2C-TX (0x");
            PRINT_UINT_HEX(slaveAddr);
            PRINT_STR(") tx=");
            PRINT_INT(tx + 1);
            PRINT_STR(" 0x");
            PRINT_UINT_HEX(regAddr);

            if (logLevel >= TMF8829_LOG_LEVEL_DEBUG)
            {
                for (uint8_t i = 0; i < tx; i++)
                {
                    PRINT_STR(" 0x");
                    PRINT_UINT_HEX(txData[i]);
                }
            }
            PRINT_LN();
        }

        int ret = i2c_write_blocking(I2C_PORT,
                                    slaveAddr,
                                    buffer,
                                    tx + 1,   // reg + data
                                    false);   // stop

        if (ret < 0)
        {
            return -1;
        }

        toTx -= tx;
        txData += tx;

        if ((addr <= 0xFF) && (addr + tx >= 0xFF))
            regAddr = 0xFF;
        else
            regAddr += tx;

        addr = regAddr;

    } while (toTx);

    return I2C_SUCCESS;
}

static int8_t i2cRxOnly ( uint8_t logLevel, uint8_t slaveAddr, uint16_t toRx, uint8_t * rxData )
{
    uint8_t expected = 0;
    int8_t res = I2C_SUCCESS;

    do
    {
        uint8_t *dump = rxData;

        if (toRx > ARDUINO_MAX_I2C_TRANSFER)
            expected = ARDUINO_MAX_I2C_TRANSFER;
        else
            expected = toRx;

        int ret = i2c_read_blocking(I2C_PORT,
                                   slaveAddr,
                                   rxData,
                                   expected,
                                   false);  // STOP

        if (ret < 0)
        {
            return I2C_ERR_TIMEOUT;
        }

        uint8_t rx = ret;

        rxData += rx;
        toRx -= rx;

        if (logLevel & TMF8829_LOG_LEVEL_I2C)
        {
            PRINT_STR("I2C-RX (0x");
            PRINT_UINT_HEX(slaveAddr);
            PRINT_STR(")");
            PRINT_STR(" toRx=");
            PRINT_INT(rx);

            if (logLevel >= TMF8829_LOG_LEVEL_DEBUG)
            {
                for (uint8_t i = 0; i < rx; i++)
                {
                    PRINT_STR(" 0x");
                    PRINT_UINT_HEX(dump[i]);
                }
            }
            PRINT_LN();
        }

        if (rx != expected)
        {
            res = I2C_ERR_TIMEOUT;
            break;
        }

    } while (toRx);

    return res;
}

int8_t i2cTxReg ( void * dptr, uint8_t slaveAddr, uint8_t regAddr, uint16_t toTx, const uint8_t * txData )
{  // split long transfers into max of 32-bytes
  tmf8829Driver * driver = (tmf8829Driver *)dptr;
  return i2cTxOnly( driver->logLevel, slaveAddr, regAddr, toTx, txData ); 
}

int8_t i2cRxReg ( void * dptr, uint8_t slaveAddr, uint8_t regAddr, uint16_t toRx, uint8_t * rxData )
{   // split long transfers into max of 32-bytes
  tmf8829Driver * driver = (tmf8829Driver *)dptr;
  int8_t res = i2cTxOnly( driver->logLevel, slaveAddr, regAddr, 0, 0 ); 
  if ( res == I2C_SUCCESS )
  {
    res = i2cRxOnly( driver->logLevel, slaveAddr, toRx, rxData );
  }
  return res;
}

int8_t i2cTxRx ( void * dptr, uint8_t slaveAddr, uint16_t toTx, const uint8_t * txData, uint16_t toRx, uint8_t * rxData )
{
  tmf8829Driver * driver = (tmf8829Driver *)dptr;
  int8_t res = I2C_SUCCESS;
  if ( toTx )
  {
    res = i2cTxOnly( driver->logLevel, slaveAddr, *txData, toTx-1, txData+1 );
  }
  if ( toRx && res == I2C_SUCCESS )
  {
    res = i2cRxOnly( driver->logLevel, slaveAddr, toRx, rxData );
  }
  return res;
}

void handleReceivedFrameHeaderData ( void * dptr, uint8_t * data )
{
  tmf8829Driver * driver = (tmf8829Driver *)dptr;  
 
  uint32_t sysTick = tmf8829GetUint32( data + 1 );
  uint16_t payload = tmf8829GetUint16( data + TMF8829_PRE_HEADER_SIZE + 2 );
  uint32_t frameNum = tmf8829GetUint32( data + TMF8829_PRE_HEADER_SIZE + 4 );
  uint16_t refPos1 = tmf8829GetUint16( data + TMF8829_PRE_HEADER_SIZE + 12 );
  uint16_t refPos2 = tmf8829GetUint16( data + TMF8829_PRE_HEADER_SIZE + 14 );
  tofIndex = (frameNum+1)%2;


  if ( driver->logLevel == TMF8829_LOG_LEVEL_RESULTS_HEADER )
  {
    printResultHeader( driver, data, TMF8829_PRE_HEADER_SIZE + TMF8829_FRAME_HEADER_SIZE );
  }
  if ( driver->logLevel > TMF8829_LOG_LEVEL_RESULTS_HEADER )
  {
    PRINT_STR( "#Obj " );
    printResults( driver, data, TMF8829_PRE_HEADER_SIZE + TMF8829_FRAME_HEADER_SIZE );   
  }
}

void handleReceivedResultData ( void * dptr, uint8_t * data, uint16_t size )
{
  tmf8829Driver * driver = (tmf8829Driver *)dptr;  
  (void)size;
  if ( driver->logLevel >= TMF8829_LOG_LEVEL_RESULTS )
  {
    printResults( driver, data, size ); 
  }

  if( (tofBuffIndex+size) < sizeof(tofBuff) )
  {
    for (uint16_t i =0; i < size; i++)
    {
      tofBuff[tofBuffIndex + i] = data[i];
    }
    tofBuffIndex += size;
  }
  // Note: data size ==1 Error for EOF check, change buffer size !
}

void handleReceivedHistogramData( void * dptr, uint8_t * data, uint16_t size )
{
  tmf8829Driver * driver = (tmf8829Driver *)dptr;
  (void)size;
  if ( driver->logLevel >= TMF8829_LOG_LEVEL_RESULTS )
  {
    printHistogram( driver, data, size );
  }
  // Note: data size ==1 Error for EOF check, change buffer size !
}

void handleReceivedResultDataEnd( void * dptr )
{
  tmf8829Driver * driver = (tmf8829Driver *)dptr;

  if (driver->logLevel >= TMF8829_LOG_LEVEL_RESULTS_HEADER )
  {
    PRINT_LN( );
  }
}

void handleReceivedHistogramDataEnd( void * dptr )
{
  tmf8829Driver * driver = (tmf8829Driver *)dptr;

  if (driver->logLevel >= TMF8829_LOG_LEVEL_RESULTS_HEADER )
  {
    PRINT_LN( );
  }
}

// Result Header printing:
// #Obj,<i2c_slave_address>,<fifostatus>,<systick>,
//      <frame_identifier>,<result_layout>,<payload>,<frameNumber>,
//      <temperature0>,<temperature1>,<temperature2>,<bdv_value>,<ref_peak_position1>, <ref_peak_position2>
void printResultHeader ( void * dptr, uint8_t * data, uint8_t len )
{
  tmf8829Driver * driver = (tmf8829Driver *)dptr;  
  if ( len == (TMF8829_PRE_HEADER_SIZE + TMF8829_FRAME_HEADER_SIZE) )
  {
    uint32_t sysTick = tmf8829GetUint32( data + 1 );
    uint16_t payload = tmf8829GetUint16( data + TMF8829_PRE_HEADER_SIZE + 2 );
    uint32_t frameNum = tmf8829GetUint32( data + TMF8829_PRE_HEADER_SIZE + 4 );
    uint16_t refPos1 = tmf8829GetUint16( data + TMF8829_PRE_HEADER_SIZE + 12 );
    uint16_t refPos2 = tmf8829GetUint16( data + TMF8829_PRE_HEADER_SIZE + 14 );
    PRINT_STR( "#Obj" );
    PRINT_CHAR( SEPARATOR );
    PRINT_UINT( driver->i2cSlaveAddress );
    PRINT_CHAR( SEPARATOR );
    PRINT_UINT( data[ 0 ] );
    PRINT_CHAR( SEPARATOR );
    PRINT_UINT( sysTick );
    PRINT_CHAR( SEPARATOR );
    PRINT_UINT( data[ TMF8829_PRE_HEADER_SIZE ] ); //ID
    PRINT_CHAR( SEPARATOR );
    PRINT_UINT( data[ TMF8829_PRE_HEADER_SIZE + 1 ] ); // Layout
    PRINT_CHAR( SEPARATOR );
    PRINT_UINT( payload ); // Payload
    PRINT_CHAR( SEPARATOR );
    PRINT_UINT( frameNum ); // Frame Number
    PRINT_CHAR( SEPARATOR );
    PRINT_UINT( data[ TMF8829_PRE_HEADER_SIZE + 8 ] ); // Temp0
    PRINT_CHAR( SEPARATOR );
    PRINT_UINT( data[ TMF8829_PRE_HEADER_SIZE + 9 ] ); // Temp1
    PRINT_CHAR( SEPARATOR );
    PRINT_UINT( data[ TMF8829_PRE_HEADER_SIZE + 10 ] ); // Temp2
    PRINT_CHAR( SEPARATOR );
    PRINT_UINT( data[ TMF8829_PRE_HEADER_SIZE + 11 ] ); // BDV
    PRINT_CHAR( SEPARATOR );
    PRINT_UINT( refPos1 ); // Ref Pos 1
    PRINT_CHAR( SEPARATOR );
    PRINT_UINT( refPos2 ); // Ref Pos 2
  }
  else // result structure too short
  {
    PRINT_STR( "#Err" );
    PRINT_CHAR( SEPARATOR );
    PRINT_STR( "header size length wrong" );
    PRINT_CHAR( SEPARATOR );
    PRINT_INT( len );
    PRINT_LN( );
  }
}

void printResults ( void * dptr, uint8_t * data, uint16_t len )
{
  (void) dptr; // not used for this platform
  uint16_t cnt;
  
  for ( cnt = 0 ; cnt < len ; cnt ++ )
  {
    PRINT_INT( data[cnt] );
    PRINT_CHAR( SEPARATOR );
  }
}

void printHistogram ( void * dptr, uint8_t * data, uint16_t len )
{
  (void) dptr; // not used for this platform
  uint16_t cnt;

  for ( cnt = 0 ; cnt < len ; cnt ++ )
  {
    PRINT_INT( data[cnt] );
    PRINT_CHAR( SEPARATOR );
  }

}
