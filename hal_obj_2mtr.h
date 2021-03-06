#ifndef _HAL_OBJ_H_
#define _HAL_OBJ_H_
/* --COPYRIGHT--,BSD
 * Copyright (c) 2012, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/

//! \file   solutions/instaspin_foc/src/hal_obj_2mtr.h
//! \brief Defines the structures for the HAL object 
//!
//! (C) Copyright 2012, Texas Instruments, Inc.


// drivers
#include "drivers/adc/adc.h"
#include "drivers/clk/clk.h"
#include "drivers/cpu/cpu.h"
#include "drivers/flash/flash.h"
#include "drivers/gpio/gpio.h"
#include "drivers/osc/osc.h"
#include "drivers/pie/pie.h"
#include "drivers/pll/pll.h"
#include "drivers/pwm/pwm.h"
#include "drivers/pwmdac/pwmdac.h"
#include "drivers/pwr/pwr.h"
#include "drivers/spi/spi.h"
#include "drivers/sci/sci.h"
#include "drivers/timer/timer.h"
#include "drivers/wdog/wdog.h"
#include "drivers/drvic/drv8305.h"


#ifdef QEP
#include "drivers/qep/qep.h"
#endif

// modules
#include "modules/offset/offset.h"
#include "modules/types/types.h"
#include "modules/usDelay/usDelay.h"


// platforms
#include "user1.h"
#include "user2.h"


//!
//!
//! \defgroup HAL_OBJ HAL_OBJ
//!
//@{


#ifdef __cplusplus
extern "C" {
#endif

// **************************************************************************
// the typedefs

//! \brief Enumeration for the Motor setup
//!
typedef enum
{
  HAL_MTR1=0,  //!< Select Motor 1
  HAL_MTR2=1   //!< Select Motor 2
} HAL_MtrSelect_e;


//! \brief      Defines the ADC data
//! \details    This data structure contains the voltage and current values that are used when 
//!             performing a HAL_AdcRead and then this structure is passed to the CTRL controller
//!             and the FAST estimator.
//!
typedef struct _HAL_AdcData_t_
{
  MATH_vec3 I;          //!< the current values

  MATH_vec3 V;          //!< the voltage values

  _iq       dcBus;      //!< the dcBus value

} HAL_AdcData_t;


//! \brief      Defines the DAC data
//! \details    This data structure contains the pwm values that are used for the DAC output
//!             on a lot of the hardware kits for debugging.
//!
typedef struct _HAL_DacData_t_
{
  _iq  value[4];      //!< the DAC data

} HAL_DacData_t;


//! \brief      Defines the PWM data
//! \details    This structure contains the pwm voltage values for the three phases.  A
//!             HAL_PwmData_t variable is filled with values from, for example, a space
//!             vector modulator and then sent to functions like HAL_writePwmData() to
//!             write to the PWM peripheral.
//!
typedef struct _HAL_PwmData_t_
{
  MATH_vec3  Tabc;      //!< the PWM time-durations for each motor phase

} HAL_PwmData_t;


//! \brief      Defines the hardware abstraction layer (HAL) data
//! \details    The HAL object contains all handles to peripherals.  When accessing a
//!             peripheral on a processor, use a HAL function along with the HAL handle
//!             for that processor to access its peripherals.
//!
typedef struct _HAL_Obj_
{
  ADC_Handle    adcHandle;        //!< the ADC handle

  CLK_Handle    clkHandle;        //!< the clock handle
 
  CPU_Handle    cpuHandle;        //!< the CPU handle

  FLASH_Handle  flashHandle;      //!< the flash handle

  GPIO_Handle   gpioHandle;       //!< the GPIO handle

  OSC_Handle    oscHandle;        //!< the oscillator handlefs

  PIE_Handle    pieHandle;        //<! the PIE handle

  PLL_Handle    pllHandle;        //!< the PLL handle

//  PWM_Handle    pwmHandle[6];     //!< the PWM handles

  PWMDAC_Handle pwmDacHandle[2];  //<! the PWMDAC handles

  PWR_Handle    pwrHandle;        //<! the power handle

  TIMER_Handle  timerHandle[3];   //<! the timer handles

  WDOG_Handle   wdogHandle;       //!< the watchdog handle

  SPI_Handle    spiAHandle;       //!< the SPI handle
  SPI_Handle    spiBHandle;       //!< the SPI handle

  SCI_Handle    sciAHandle;
  SCI_Handle    sciBHandle;

} HAL_Obj;


//! \brief      Defines the HAL handle
//! \details    The HAL handle is a pointer to a HAL object.  In all HAL functions
//!             the HAL handle is passed so that the function knows what peripherals
//!             are to be accessed.
//!
typedef struct _HAL_Obj_ *HAL_Handle;


////! \brief      Defines the HAL object
////!
//extern HAL_Obj hal;



//! \brief      Defines the hardware abstraction layer (HAL) data
//! \details    The HAL object contains all handles to peripherals.  When accessing a
//!             peripheral on a processor, use a HAL function along with the HAL handle
//!             for that processor to access its peripherals.
//!
typedef struct _HAL_Obj_MTR_
{
  _iq           current_sf;       //!< the current scale factor, amps_pu/cnt

  _iq           voltage_sf;       //!< the voltage scale factor, volts_pu/cnt

  uint_least8_t numCurrentSensors; //!< the number of current sensors
  uint_least8_t numVoltageSensors; //!< the number of voltage sensors

  HAL_MtrSelect_e mtrNum;         //!< the motor number

  PWM_Handle    pwmHandle[3];     //<! the PWM handles for two motors

  DRV8305_Handle drv8305Handle;   //!< the drv8305 interface handle
  DRV8305_Obj    drv8305;         //!< the drv8305 interface object

#ifdef QEP
  QEP_Handle    qepHandle;        //!< the QEP handles
#endif

} HAL_Obj_mtr;


//! \brief      Defines the HAL handle
//! \details    The HAL handle is a pointer to a HAL object.  In all HAL functions
//!             the HAL handle is passed so that the function knows what peripherals
//!             are to be accessed.
//!
typedef struct _HAL_Obj_MTR_ *HAL_Handle_mtr;


#ifdef __cplusplus
}
#endif // extern "C"

//@} // ingroup
#endif // end of _HAL_OBJ_H_ definition

