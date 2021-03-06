// --COPYRIGHT--,BSD
// Copyright (c) 2015, Texas Instruments Incorporated
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// *  Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// *  Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// *  Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// --/COPYRIGHT
//! \file   solutions/instaspin_motion/src/proj_lab013f.c
//! \brief Dual position control using SpinTAC
//!
//! (C) Copyright 2015, Texas Instruments, Inc.

//! \defgroup PROJ_LAB13F PROJ_LAB13F
//@{

//! \defgroup PROJ_LAB13F_OVERVIEW Project Overview
//!
//! Basic implementation of FOC by using the estimator for angle and speed
//! feedback only.  Adds in SpinTAC Position Contol and SpinTAC Position Move
//!	Dual position control using SpinTAC

// **************************************************************************
// the includes

// system includes
#include <math.h>
#include "main_position_2mtr.h"

#ifdef FLASH
#pragma CODE_SECTION(motor1_ISR,"ramfuncs");
#pragma CODE_SECTION(motor2_ISR,"ramfuncs");
#endif

// Include header files used in the main function

// **************************************************************************
// the defines

// **************************************************************************
// the globals

CLARKE_Handle clarkeHandle_I[2];  //!< the handle for the current Clarke
//!< transform
CLARKE_Obj clarke_I[2];        //!< the current Clarke transform object

PARK_Handle parkHandle[2];      //!< the handle for the current Parke
//!< transform
PARK_Obj park[2];            //!< the current Parke transform object

CLARKE_Handle clarkeHandle_V[2];  //!< the handle for the voltage Clarke
//!< transform
CLARKE_Obj clarke_V[2];        //!< the voltage Clarke transform object

EST_Handle estHandle[2];       //!< the handle for the estimator

PID_Obj pid[2][3];          //!< three objects for PID controllers
//!< 0 - Speed, 1 - Id, 2 - Iq
PID_Handle pidHandle[2][3];    //!< three handles for PID controllers
//!< 0 - Speed, 1 - Id, 2 - Iq
uint16_t stCntPosition[2];   //!< count variable to decimate the execution
//!< of SpinTAC Position Control

IPARK_Handle iparkHandle[2];     //!< the handle for the inverse Park
//!< transform
IPARK_Obj ipark[2];           //!< the inverse Park transform object

SVGEN_Handle svgenHandle[2];     //!< the handle for the space vector generator
SVGEN_Obj svgen[2];           //!< the space vector generator object

ENC_Handle encHandle[2];      //!< the handle for the encoder
ENC_Obj enc[2];            //!< the encoder object

SLIP_Handle slipHandle[2];     //!< the handle for the slip compensator
SLIP_Obj slip[2];           //!< the slip compensator object

HAL_Handle halHandle;         //!< the handle for the hardware abstraction
//!< layer for common CPU setup
HAL_Obj hal;               //!< the hardware abstraction layer object

ANGLE_COMP_Handle angleCompHandle[2];  //!< the handle for the angle compensation
ANGLE_COMP_Obj angleComp[2];        //!< the angle compensation object

HAL_Handle_mtr halHandleMtr[2]; //!< the handle for the hardware abstraction
//!< layer specific to the motor board.
HAL_Obj_mtr halMtr[2];       //!< the hardware abstraction layer object
//!< specific to the motor board.

HAL_PwmData_t gPwmData[2] = { { _IQ(0.0), _IQ(0.0), _IQ(0.0) },   //!< contains the
		{ _IQ(0.0), _IQ(0.0), _IQ(0.0) } };  //!< pwm values for each phase.
//!< -1.0 is 0%, 1.0 is 100%

HAL_AdcData_t gAdcData[2];       //!< contains three current values, three
//!< voltage values and one DC buss value

MATH_vec3 gOffsets_I_pu[2] = { { _IQ(0.0), _IQ(0.0), _IQ(0.0) },  //!< contains
		{ _IQ(0.0), _IQ(0.0), _IQ(0.0) } }; //!< the offsets for the current feedback

MATH_vec3 gOffsets_V_pu[2] = { { _IQ(0.0), _IQ(0.0), _IQ(0.0) },  //!< contains
		{ _IQ(0.0), _IQ(0.0), _IQ(0.0) } }; //!< the offsets for the voltage feedback

MATH_vec2 gIdq_ref_pu[2] = { { _IQ(0.0), _IQ(0.0) },  //!< contains the Id and
		{ _IQ(0.0), _IQ(0.0) } }; //!< Iq references

MATH_vec2 gVdq_out_pu[2] = { { _IQ(0.0), _IQ(0.0) },  //!< contains the output
		{ _IQ(0.0), _IQ(0.0) } }; //!< Vd and Vq from the current controllers

MATH_vec2 gIdq_pu[2] = { { _IQ(0.0), _IQ(0.0) },   //!< contains the Id and Iq
		{ _IQ(0.0), _IQ(0.0) } };  //!< measured values

FILTER_FO_Handle filterHandle[2][6];     //!< the handles for the 3-current and 3-voltage filters for offset calculation
FILTER_FO_Obj filter[2][6];                  //!< the 3-current and 3-voltage filters for offset calculation
uint32_t gOffsetCalcCount[2] = { 0, 0 };

USER_Params gUserParams[2];

uint32_t gAlignCount[2] = { 0, 0 };

ST_Obj st_obj[2];      //!< the SpinTAC objects
ST_Handle stHandle[2];    //!< the handles for the SpinTAC objects

volatile MOTOR_Vars_t gMotorVars[2] = { MOTOR_Vars_INIT_Mtr1, MOTOR_Vars_INIT_Mtr2 };   //!< the global motor
//!< variables that are defined in main.h and
//!< used for display in the debugger's watch
//!< window

#ifdef FLASH
// Used for running BackGround in flash, and ISR in RAM
extern uint16_t *RamfuncsLoadStart, *RamfuncsLoadEnd, *RamfuncsRunStart;
#endif

#ifdef DRV8301_SPI
// Watch window interface to the 8301 SPI
DRV_SPI_8301_Vars_t gDrvSpi8301Vars[2];
#endif

#ifdef DRV8305_SPI
// Watch window interface to the 8305 SPI
DRV_SPI_8305_Vars_t gDrvSpi8305Vars[2];
#endif

_iq gFlux_pu_to_Wb_sf[2];

_iq gFlux_pu_to_VpHz_sf[2];

_iq gTorque_Ls_Id_Iq_pu_to_Nm_sf[2];

_iq gTorque_Flux_Iq_pu_to_Nm_sf[2];

_iq gSpeed_krpm_to_pu_sf[2];

_iq gSpeed_pu_to_krpm_sf[2];

_iq gSpeed_hz_to_krpm_sf[2];

_iq gCurrent_A_to_pu_sf[2];


#define ST_SPEED_PU_PER_Hz (USER_MOTOR_NUM_POLE_PAIRS / USER_IQ_FULL_SCALE_FREQ_Hz)

#define ST_SPEED_Hz_PER_PU (USER_IQ_FULL_SCALE_FREQ_Hz / USER_MOTOR_NUM_POLE_PAIRS)

volatile bool voltageTooLowList[2] = { true, true };
_iq lowVoltageThreshold = _IQ(0.01);

uint16_t dataRx;
uint16_t success;

char buf[32];
char returnBuf[32];
int counter = 0;
int rxIntCounter = 0;
int commandReceived = 0;
int commandStart = 0;
int sendFeedback = 0;

uint32_t enc1StableAlignCount = 0;
uint32_t enc2StableAlignCount = 0;
uint32_t enc1PrevValue = 0;
uint32_t enc2PrevValue = 0;
uint32_t enc1StableAlignLimit = 10000;
uint32_t enc2StableAlignLimit = 10000;

typedef struct PositionParams {
	_iq20 posRef;
	_iq20 transitionPosRef;
	_iq20 speedRef_rps;
	_iq20 maxSpeed_rps;
	_iq20 minSpeed_rps;
	_iq20 acc_rpsps;
	_iq20 dec_rpsps;
	_iq20 posDiff;
	_iq20 requiredDeceleration_rpsps;
	_iq20 prevSpeed_rps;
	_iq20 currentAcc_rpsps;
	_iq20 posSampleTime_sec;
	_iq prevPosMrev;
	uint32_t zeroSpeedMoveFailCount;
	uint32_t zeroSpeedMoveFailLimit;
} PositionParams;

PositionParams positionParamsList[2] = {
	{
		.posRef = _IQ20(0.0),
		.transitionPosRef = _IQ20(0.0),
		.speedRef_rps = _IQ20(0.0),
		.maxSpeed_rps = _IQ20(0.0),
		.minSpeed_rps = _IQ20(0.001),
		.acc_rpsps = _IQ20(200.0),
		.dec_rpsps = _IQ20(200.0),
		.posDiff = _IQ20(0.0),
		.requiredDeceleration_rpsps = _IQ20(0.0),
		.prevSpeed_rps = _IQ20(0.0),
		.currentAcc_rpsps = _IQ20(0.0),
		.posSampleTime_sec = _IQ20(ST_SAMPLE_TIME),
		.prevPosMrev = _IQ(0.0),
		.zeroSpeedMoveFailCount = 0,
		.zeroSpeedMoveFailLimit = 3
	},
	{
		.posRef = _IQ20(0.0),
		.transitionPosRef = _IQ20(0.0),
		.speedRef_rps = _IQ20(0.0),
		.maxSpeed_rps = _IQ20(0.0),
		.minSpeed_rps = _IQ20(0.001),
		.acc_rpsps = _IQ20(200.0),
		.dec_rpsps = _IQ20(200.0),
		.posDiff = _IQ20(0.0),
		.requiredDeceleration_rpsps = _IQ20(0.0),
		.prevSpeed_rps = _IQ20(0.0),
		.currentAcc_rpsps = _IQ20(0.0),
		.posSampleTime_sec = _IQ20(ST_SAMPLE_TIME_2),
		.prevPosMrev = _IQ(0.0),
		.zeroSpeedMoveFailCount = 0,
		.zeroSpeedMoveFailLimit = 3
	}
};

_iq runPosCtl(HAL_MtrSelect_e mtrNum);

void calcTransitionPosRef(HAL_MtrSelect_e mtrNum);

void serialWrite(char *sendData, int length);

bool isMotorReady(HAL_MtrSelect_e mtrNum);

bool isMotorActive(HAL_MtrSelect_e mtrNum);


// **************************************************************************
// the functions
void main(void) {
	// IMPORTANT NOTE: If you are not familiar with MotorWare coding guidelines
	// please refer to the following document:
	// C:/ti/motorware/motorware_1_01_00_1x/docs/motorware_coding_standards.pdf

	// Only used if running from FLASH
	// Note that the variable FLASH is defined by the project

#ifdef FLASH
	// Copy time critical code and Flash setup code to RAM
	// The RamfuncsLoadStart, RamfuncsLoadEnd, and RamfuncsRunStart
	// symbols are created by the linker. Refer to the linker files.
	memCopy((uint16_t *) &RamfuncsLoadStart, (uint16_t *) &RamfuncsLoadEnd, (uint16_t *) &RamfuncsRunStart);
#endif

	// initialize the Hardware Abstraction Layer  (HAL)
	// halHandle will be used throughout the code to interface with the HAL
	// (set parameters, get and set functions, etc) halHandle is required since
	// this is how all objects are interfaced, and it allows interface with
	// multiple objects by simply passing a different handle. The use of
	// handles is explained in this document:
	// C:/ti/motorware/motorware_1_01_00_1x/docs/motorware_coding_standards.pdf
	halHandle = HAL_init(&hal, sizeof(hal));

	// initialize the user parameters
	// This function initializes all values of structure gUserParams with
	// values defined in user.h. The values in gUserParams will be then used by
	// the hardware abstraction layer (HAL) to configure peripherals such as
	// PWM, ADC, interrupts, etc.
	USER_setParamsMtr1(&gUserParams[HAL_MTR1]);
	USER_setParamsMtr2(&gUserParams[HAL_MTR2]);

	// set the hardware abstraction layer parameters
	// This function initializes all peripherals through a Hardware Abstraction
	// Layer (HAL). It uses all values stored in gUserParams.
	HAL_setParams(halHandle, &gUserParams[HAL_MTR1]);

	// initialize the estimator
	estHandle[HAL_MTR1] = EST_init((void *) USER_EST_HANDLE_ADDRESS, 0x200);
	estHandle[HAL_MTR2] = EST_init((void *) USER_EST_HANDLE_ADDRESS_1, 0x200);

	{
		uint_least8_t mtrNum;

		for (mtrNum = HAL_MTR1; mtrNum <= HAL_MTR2; mtrNum++) {

			// initialize the individual motor hal files
			halHandleMtr[mtrNum] = HAL_init_mtr(&halMtr[mtrNum], sizeof(halMtr[mtrNum]), (HAL_MtrSelect_e) mtrNum);

			// Setup each motor board to its specific setting
			HAL_setParamsMtr(halHandleMtr[mtrNum], halHandle, &gUserParams[mtrNum]);

			{
				// These function calls are used to initialize the estimator with ROM
				// function calls. It needs the specific address where the controller
				// object is declared by the ROM code.
				CTRL_Handle ctrlHandle = CTRL_init((void *) USER_CTRL_HANDLE_ADDRESS, 0x200);
				CTRL_Obj *obj = (CTRL_Obj *) ctrlHandle;

				// this sets the estimator handle (part of the controller object) to
				// the same value initialized above by the EST_init() function call.
				// This is done so the next function implemented in ROM, can
				// successfully initialize the estimator as part of the controller
				// object.
				obj->estHandle = estHandle[mtrNum];

				// initialize the estimator through the controller. These three
				// function calls are needed for the F2806xF/M implementation of
				// InstaSPIN.
				CTRL_setParams(ctrlHandle, &gUserParams[mtrNum]);
				CTRL_setUserMotorParams(ctrlHandle);
				CTRL_setupEstIdleState(ctrlHandle);
			}

			//Compensates for the delay introduced
			//from the time when the system inputs are sampled to when the PWM
			//voltages are applied to the motor windings.
			angleCompHandle[mtrNum] = ANGLE_COMP_init(&angleComp[mtrNum], sizeof(angleComp[mtrNum]));
			ANGLE_COMP_setParams(angleCompHandle[mtrNum], gUserParams[mtrNum].iqFullScaleFreq_Hz,
					gUserParams[mtrNum].pwmPeriod_usec, gUserParams[mtrNum].numPwmTicksPerIsrTick);

			// initialize the Clarke modules
			// Clarke handle initialization for current signals
			clarkeHandle_I[mtrNum] = CLARKE_init(&clarke_I[mtrNum], sizeof(clarke_I[mtrNum]));

			// Clarke handle initialization for voltage signals
			clarkeHandle_V[mtrNum] = CLARKE_init(&clarke_V[mtrNum], sizeof(clarke_V[mtrNum]));

			// Park handle initialization for current signals
			parkHandle[mtrNum] = PARK_init(&park[mtrNum], sizeof(park[mtrNum]));

			// compute scaling factors for flux and torque calculations
			gFlux_pu_to_Wb_sf[mtrNum] = USER_computeFlux_pu_to_Wb_sf(&gUserParams[mtrNum]);

			gFlux_pu_to_VpHz_sf[mtrNum] = USER_computeFlux_pu_to_VpHz_sf(&gUserParams[mtrNum]);

			gTorque_Ls_Id_Iq_pu_to_Nm_sf[mtrNum] = USER_computeTorque_Ls_Id_Iq_pu_to_Nm_sf(&gUserParams[mtrNum]);

			gTorque_Flux_Iq_pu_to_Nm_sf[mtrNum] = USER_computeTorque_Flux_Iq_pu_to_Nm_sf(&gUserParams[mtrNum]);

			gSpeed_krpm_to_pu_sf[mtrNum] = _IQ(
					(float_t )gUserParams[mtrNum].motor_numPolePairs * 1000.0
							/ (gUserParams[mtrNum].iqFullScaleFreq_Hz * 60.0));

			gSpeed_pu_to_krpm_sf[mtrNum] = _IQ(
					(gUserParams[mtrNum].iqFullScaleFreq_Hz * 60.0)
							/ ((float_t )gUserParams[mtrNum].motor_numPolePairs * 1000.0));

			gSpeed_hz_to_krpm_sf[mtrNum] = _IQ(60.0 / (float_t )gUserParams[mtrNum].motor_numPolePairs / 1000.0);

			gCurrent_A_to_pu_sf[mtrNum] = _IQ(1.0 / gUserParams[mtrNum].iqFullScaleCurrent_A);

			// disable Rs recalculation
			EST_setFlag_enableRsRecalc(estHandle[mtrNum], false);

			// set the number of current sensors
			setupClarke_I(clarkeHandle_I[mtrNum], gUserParams[mtrNum].numCurrentSensors);

			// set the number of voltage sensors
			setupClarke_V(clarkeHandle_V[mtrNum], gUserParams[mtrNum].numVoltageSensors);

			// initialize the PID controllers
			pidSetup((HAL_MtrSelect_e) mtrNum);

			// initialize the inverse Park module
			iparkHandle[mtrNum] = IPARK_init(&ipark[mtrNum], sizeof(ipark[mtrNum]));

			// initialize the space vector generator module
			svgenHandle[mtrNum] = SVGEN_init(&svgen[mtrNum], sizeof(svgen[mtrNum]));

			// initialize and configure offsets using filters
			{
				uint16_t cnt = 0;
				_iq b0 = _IQ(gUserParams[mtrNum].offsetPole_rps / (float_t )gUserParams[mtrNum].ctrlFreq_Hz);
				_iq a1 = (b0 - _IQ(1.0));
				_iq b1 = _IQ(0.0);

				for (cnt = 0; cnt < 6; cnt++) {
					filterHandle[mtrNum][cnt] = FILTER_FO_init(&filter[mtrNum][cnt], sizeof(filter[mtrNum][0]));
					FILTER_FO_setDenCoeffs(filterHandle[mtrNum][cnt], a1);
					FILTER_FO_setNumCoeffs(filterHandle[mtrNum][cnt], b0, b1);
					FILTER_FO_setInitialConditions(filterHandle[mtrNum][cnt], _IQ(0.0), _IQ(0.0));
				}

				gMotorVars[mtrNum].Flag_enableOffsetcalc = false;
			}

			// initialize the encoder module
			encHandle[mtrNum] = ENC_init(&enc[mtrNum], sizeof(enc[mtrNum]));

			// initialize the slip compensation module
			slipHandle[mtrNum] = SLIP_init(&slip[mtrNum], sizeof(slip[mtrNum]));
			// setup the SLIP module
			SLIP_setup(slipHandle[mtrNum], _IQ(gUserParams[mtrNum].ctrlPeriod_sec));

			// setup faults
			HAL_setupFaults(halHandleMtr[mtrNum]);

			// initialize the SpinTAC Components
			stHandle[mtrNum] = ST_init(&st_obj[mtrNum], sizeof(st_obj[mtrNum]));
		} // End of for loop
	}

	// setup the encoder module
	ENC_setup(encHandle[HAL_MTR1], 1, USER_MOTOR_NUM_POLE_PAIRS, USER_MOTOR_ENCODER_LINES, 0,
			USER_IQ_FULL_SCALE_FREQ_Hz, USER_ISR_FREQ_Hz, 8000.0);
	ENC_setup(encHandle[HAL_MTR2], 1, USER_MOTOR_NUM_POLE_PAIRS_2, USER_MOTOR_ENCODER_LINES_2, 0,
			USER_IQ_FULL_SCALE_FREQ_Hz_2, USER_ISR_FREQ_Hz_2, 8000.0);

	// setup the SpinTAC Components
	ST_setupPosConv_mtr1(stHandle[HAL_MTR1]);
	ST_setupPosConv_mtr2(stHandle[HAL_MTR2]);
	ST_setupPosCtl_mtr1(stHandle[HAL_MTR1]);
	//ST_setupPosMove_mtr1(stHandle[HAL_MTR1]);
	ST_setupPosCtl_mtr2(stHandle[HAL_MTR2]);
	//ST_setupPosMove_mtr2(stHandle[HAL_MTR2]);

	// set the pre-determined current and voltage feeback offset values
	gOffsets_I_pu[HAL_MTR1].value[0] = _IQ(I_A_offset);
	gOffsets_I_pu[HAL_MTR1].value[1] = _IQ(I_B_offset);
	gOffsets_I_pu[HAL_MTR1].value[2] = _IQ(I_C_offset);
	gOffsets_V_pu[HAL_MTR1].value[0] = _IQ(V_A_offset);
	gOffsets_V_pu[HAL_MTR1].value[1] = _IQ(V_B_offset);
	gOffsets_V_pu[HAL_MTR1].value[2] = _IQ(V_C_offset);

	gOffsets_I_pu[HAL_MTR2].value[0] = _IQ(I_A_offset_2);
	gOffsets_I_pu[HAL_MTR2].value[1] = _IQ(I_B_offset_2);
	gOffsets_I_pu[HAL_MTR2].value[2] = _IQ(I_C_offset_2);
	gOffsets_V_pu[HAL_MTR2].value[0] = _IQ(V_A_offset_2);
	gOffsets_V_pu[HAL_MTR2].value[1] = _IQ(V_B_offset_2);
	gOffsets_V_pu[HAL_MTR2].value[2] = _IQ(V_C_offset_2);

	// initialize the interrupt vector table
	HAL_initIntVectorTable(halHandle);

	// enable the ADC interrupts
	HAL_enableAdcInts(halHandle);

	// enable the SCI interrupts
	HAL_enableSciInts(halHandle);

	// enable global interrupts
	HAL_enableGlobalInts(halHandle);

	// enable debug interrupts
	HAL_enableDebugInt(halHandle);

	// disable the PWM
	HAL_disablePwm(halHandleMtr[HAL_MTR1]);
	HAL_disablePwm(halHandleMtr[HAL_MTR2]);

	// initialize SpinTAC Velocity Control watch window variables
	gMotorVars[HAL_MTR1].SpinTAC.PosCtlBw_radps = STPOSCTL_getBandwidth_radps(st_obj[HAL_MTR1].posCtlHandle);
	gMotorVars[HAL_MTR2].SpinTAC.PosCtlBw_radps = STPOSCTL_getBandwidth_radps(st_obj[HAL_MTR2].posCtlHandle);
	// initialize the watch window with maximum and minimum Iq reference
	gMotorVars[HAL_MTR1].SpinTAC.PosCtlOutputMax_A = _IQmpy(STPOSCTL_getOutputMaximum(st_obj[HAL_MTR1].posCtlHandle),
			_IQ(gUserParams[HAL_MTR1].iqFullScaleCurrent_A));
	gMotorVars[HAL_MTR1].SpinTAC.PosCtlOutputMin_A = _IQmpy(STPOSCTL_getOutputMinimum(st_obj[HAL_MTR1].posCtlHandle),
			_IQ(gUserParams[HAL_MTR1].iqFullScaleCurrent_A));
	gMotorVars[HAL_MTR2].SpinTAC.PosCtlOutputMax_A = _IQmpy(STPOSCTL_getOutputMaximum(st_obj[HAL_MTR2].posCtlHandle),
			_IQ(gUserParams[HAL_MTR2].iqFullScaleCurrent_A));
	gMotorVars[HAL_MTR2].SpinTAC.PosCtlOutputMin_A = _IQmpy(STPOSCTL_getOutputMinimum(st_obj[HAL_MTR2].posCtlHandle),
			_IQ(gUserParams[HAL_MTR2].iqFullScaleCurrent_A));

	// enable the system by default
	gMotorVars[HAL_MTR1].Flag_enableSys = true;

#ifdef DRV8301_SPI
	// turn on the DRV8301 if present
	HAL_enableDrv(halHandleMtr[HAL_MTR1]);
	HAL_enableDrv(halHandleMtr[HAL_MTR2]);
	// initialize the DRV8301 interface
	HAL_setupDrvSpi(halHandleMtr[HAL_MTR1],&gDrvSpi8301Vars[HAL_MTR1]);
	HAL_setupDrvSpi(halHandleMtr[HAL_MTR2],&gDrvSpi8301Vars[HAL_MTR2]);
#endif

#ifdef DRV8305_SPI
	// turn on the DRV8305 if present
	HAL_enableDrv(halHandleMtr[HAL_MTR1]);
	HAL_enableDrv(halHandleMtr[HAL_MTR2]);
	// initialize the DRV8305 interface
	HAL_setupDrvSpi(halHandleMtr[HAL_MTR1], &gDrvSpi8305Vars[HAL_MTR1]);
	HAL_setupDrvSpi(halHandleMtr[HAL_MTR2], &gDrvSpi8305Vars[HAL_MTR2]);
#endif

	// Begin the background loop
	for (;;) {
		// Waiting for enable system flag to be set
		// Motor 1 Flag_enableSys is the master control.
		while (!(gMotorVars[HAL_MTR1].Flag_enableSys));

		// loop while the enable system flag is true
		// Motor 1 Flag_enableSys is the master control.
		while (gMotorVars[HAL_MTR1].Flag_enableSys) {
			uint_least8_t mtrNum = HAL_MTR1;

			for (mtrNum = HAL_MTR1; mtrNum <= HAL_MTR2; mtrNum++) {

				if (voltageTooLowList[mtrNum] && gMotorVars[mtrNum].VdcBus_kV > lowVoltageThreshold) {
					voltageTooLowList[mtrNum] = false;

				} else if (!voltageTooLowList[mtrNum] && gMotorVars[mtrNum].VdcBus_kV < lowVoltageThreshold) {
					voltageTooLowList[mtrNum] = true;

					gMotorVars[mtrNum].Flag_Run_Identify = false;
				}

				if (gMotorVars[mtrNum].Flag_Run_Identify) {
					// If Flag_enableSys is set AND Flag_Run_Identify is set THEN
					// enable PWMs and set the speed reference

					// update estimator state
					EST_updateState(estHandle[mtrNum], 0);

#ifdef FAST_ROM_V1p6
					// call this function to fix 1p6. This is only used for
					// F2806xF/M implementation of InstaSPIN (version 1.6 of
					// ROM), since the inductance calculation is not done
					// correctly in ROM, so this function fixes that ROM bug.
					softwareUpdate1p6(estHandle[mtrNum], &gUserParams[mtrNum]);
#endif

					// enable the PWM
					HAL_enablePwm(halHandleMtr[mtrNum]);

					// enable SpinTAC Velocity Control
					STPOSCTL_setEnable(st_obj[mtrNum].posCtlHandle, true);
					// provide bandwidth setting to SpinTAC Velocity Control
					STPOSCTL_setBandwidth_radps(st_obj[mtrNum].posCtlHandle, gMotorVars[mtrNum].SpinTAC.PosCtlBw_radps);
					// provide output maximum and minimum setting to SpinTAC Velocity Control
					STPOSCTL_setOutputMaximums(st_obj[mtrNum].posCtlHandle,
							_IQmpy(gMotorVars[mtrNum].SpinTAC.PosCtlOutputMax_A, gCurrent_A_to_pu_sf[mtrNum]),
							_IQmpy(gMotorVars[mtrNum].SpinTAC.PosCtlOutputMin_A, gCurrent_A_to_pu_sf[mtrNum]));
				} else {
			    	//Flag_enableSys is set AND Flag_Run_Identify is not set

					// set estimator to Idle
					EST_setIdle(estHandle[mtrNum]);

					// disable the PWM
					HAL_disablePwm(halHandleMtr[mtrNum]);

					// clear integrator outputs
					PID_setUi(pidHandle[mtrNum][0], _IQ(0.0));
					PID_setUi(pidHandle[mtrNum][1], _IQ(0.0));
					PID_setUi(pidHandle[mtrNum][2], _IQ(0.0));

					// clear Id and Iq references
					gIdq_ref_pu[mtrNum].value[0] = _IQ(0.0);
					gIdq_ref_pu[mtrNum].value[1] = _IQ(0.0);

					// place SpinTAC Velocity Control into reset
					STPOSCTL_setEnable(st_obj[mtrNum].posCtlHandle, false);
					// If motor is not running, feed the position feedback into SpinTAC Position Move
					//STPOSMOVE_setPositionStart_mrev(st_obj[mtrNum].posMoveHandle, STPOSCONV_getPosition_mrev(st_obj[mtrNum].posConvHandle));
				}

				// update the global variables
				updateGlobalVariables(estHandle[mtrNum], mtrNum);

				// enable/disable the forced angle
				EST_setFlag_enableForceAngle(estHandle[mtrNum], gMotorVars[mtrNum].Flag_enableForceAngle);

#ifdef DRV8301_SPI
				HAL_writeDrvData(halHandleMtr[mtrNum],&gDrvSpi8301Vars[mtrNum]);

				HAL_readDrvData(halHandleMtr[mtrNum],&gDrvSpi8301Vars[mtrNum]);
#endif
#ifdef DRV8305_SPI
				HAL_writeDrvData(halHandleMtr[mtrNum], &gDrvSpi8305Vars[mtrNum]);

				HAL_readDrvData(halHandleMtr[mtrNum], &gDrvSpi8305Vars[mtrNum]);
#endif

			} // end of for loop

			if (sendFeedback) {
				sendFeedback = 0;

				gMotorVars[HAL_MTR1].Flag_Run_Identify = true;
				gMotorVars[HAL_MTR2].Flag_Run_Identify = true;

				returnBuf[0] = '<';

				long motor1Position = _IQ20mpyI32(_IQ20(20.0), st_obj[HAL_MTR1].pos.conv.PosROCounts) + _IQtoIQ20(st_obj[HAL_MTR1].pos.conv.Pos_mrev);
				long motor1Speed = _IQ20mpy(_IQtoIQ20(STPOSCONV_getVelocityFiltered(st_obj[HAL_MTR1].posConvHandle)), _IQ20(ST_SPEED_Hz_PER_PU));

				long motor2Position = _IQ20mpyI32(_IQ20(20.0), st_obj[HAL_MTR2].pos.conv.PosROCounts) + _IQtoIQ20(st_obj[HAL_MTR2].pos.conv.Pos_mrev);
				long motor2Speed = _IQ20mpy(_IQtoIQ20(STPOSCONV_getVelocityFiltered(st_obj[HAL_MTR2].posConvHandle)), _IQ20(ST_SPEED_Hz_PER_PU));

				returnBuf[1] = motor1Position;
				returnBuf[2] = motor1Position >> 8;
				returnBuf[3] = motor1Position >> 16;
				returnBuf[4] = motor1Position >> 24;

				returnBuf[5] = motor1Speed;
				returnBuf[6] = motor1Speed >> 8;
				returnBuf[7] = motor1Speed >> 16;
				returnBuf[8] = motor1Speed >> 24;

				returnBuf[9] = motor2Position;
				returnBuf[10] = motor2Position >> 8;
				returnBuf[11] = motor2Position >> 16;
				returnBuf[12] = motor2Position >> 24;

				returnBuf[13] = motor2Speed;
				returnBuf[14] = motor2Speed >> 8;
				returnBuf[15] = motor2Speed >> 16;
				returnBuf[16] = motor2Speed >> 24;

				returnBuf[17] = '>';

				serialWrite(returnBuf, 18);
			}

		} // end of while(gFlag_enableSys) loop

		// disable the PWM
		HAL_disablePwm(halHandleMtr[HAL_MTR1]);
		HAL_disablePwm(halHandleMtr[HAL_MTR2]);

		gMotorVars[HAL_MTR1].Flag_Run_Identify = false;
		gMotorVars[HAL_MTR2].Flag_Run_Identify = false;

	} // end of for(;;) loop
} // end of main() function

//! \brief     The main ISR that implements the motor control.
interrupt void motor1_ISR(void) {
	// Declaration of local variables
	static _iq angle_pu = _IQ(0.0);
	_iq speed_pu = _IQ(0.0);
	_iq oneOverDcBus;
	MATH_vec2 Iab_pu;
	MATH_vec2 Vab_pu;
	MATH_vec2 phasor;

	HAL_setGpioHigh(halHandle, GPIO_Number_12);

	// acknowledge the ADC interrupt
	HAL_acqAdcInt(halHandle, ADC_IntNumber_1);

	// convert the ADC data
	HAL_readAdcDataWithOffsets(halHandle, halHandleMtr[HAL_MTR1], &gAdcData[HAL_MTR1]);

	// remove offsets
	gAdcData[HAL_MTR1].I.value[0] = gAdcData[HAL_MTR1].I.value[0] - gOffsets_I_pu[HAL_MTR1].value[0];
	gAdcData[HAL_MTR1].I.value[1] = gAdcData[HAL_MTR1].I.value[1] - gOffsets_I_pu[HAL_MTR1].value[1];
	gAdcData[HAL_MTR1].I.value[2] = gAdcData[HAL_MTR1].I.value[2] - gOffsets_I_pu[HAL_MTR1].value[2];
	gAdcData[HAL_MTR1].V.value[0] = gAdcData[HAL_MTR1].V.value[0] - gOffsets_V_pu[HAL_MTR1].value[0];
	gAdcData[HAL_MTR1].V.value[1] = gAdcData[HAL_MTR1].V.value[1] - gOffsets_V_pu[HAL_MTR1].value[1];
	gAdcData[HAL_MTR1].V.value[2] = gAdcData[HAL_MTR1].V.value[2] - gOffsets_V_pu[HAL_MTR1].value[2];

	// run Clarke transform on current.  Three values are passed, two values
	// are returned.
	CLARKE_run(clarkeHandle_I[HAL_MTR1], &gAdcData[HAL_MTR1].I, &Iab_pu);

	// compute the sine and cosine phasor values which are part of the
	// Park transform calculations. Once these values are computed,
	// they are copied into the PARK module, which then uses them to
	// transform the voltages from Alpha/Beta to DQ reference frames.
	phasor.value[0] = _IQcosPU(angle_pu);
	phasor.value[1] = _IQsinPU(angle_pu);

	// set the phasor in the Park transform
	PARK_setPhasor(parkHandle[HAL_MTR1], &phasor);

	// Run the Park module.  This converts the current vector from
	// stationary frame values to synchronous frame values.
	PARK_run(parkHandle[HAL_MTR1], &Iab_pu, &gIdq_pu[HAL_MTR1]);

	// compute the electrical angle
	ENC_calcElecAngle(encHandle[HAL_MTR1], HAL_getQepPosnCounts(halHandleMtr[HAL_MTR1]));

	//bool shouldRunPosCtl = false;

	if (stCntPosition[HAL_MTR1] >= gUserParams[HAL_MTR1].numCtrlTicksPerSpeedTick) {
		// Calculate the feedback position, this must always be ran in order to ensure there are no jumps
		ST_runPosConv(stHandle[HAL_MTR1], encHandle[HAL_MTR1], slipHandle[HAL_MTR1], &gIdq_pu[HAL_MTR1],
				gUserParams[HAL_MTR1].motor_type);
	}

	// run the appropriate controller
	if (gMotorVars[HAL_MTR1].Flag_Run_Identify) {
		// Declaration of local variables.
		_iq refValue;
		_iq fbackValue;
		_iq outMax_pu;

		// check if the motor should be forced into encoder slignment
		if (gMotorVars[HAL_MTR1].Flag_enableAlignment == false) {
			// when appropriate, run SpinTAC Position Control
			// This mechanism provides the decimation for the speed loop.
			if (stCntPosition[HAL_MTR1] >= gUserParams[HAL_MTR1].numCtrlTicksPerSpeedTick) {
				// Reset the Position execution counter.
				stCntPosition[HAL_MTR1] = 0;

				/*if ((STPOSMOVE_getStatus(st_obj[HAL_MTR1].posMoveHandle) == ST_MOVE_IDLE)
						&& (gMotorVars[HAL_MTR1].RunPositionProfile == true)) {
					// Get the configuration for SpinTAC Position Move
					STPOSMOVE_setCurveType(st_obj[HAL_MTR1].posMoveHandle,
							gMotorVars[HAL_MTR1].SpinTAC.PosMoveCurveType);
					STPOSMOVE_setPositionStep_mrev(st_obj[HAL_MTR1].posMoveHandle, gMotorVars[HAL_MTR1].PosStepInt_MRev,
							gMotorVars[HAL_MTR1].PosStepFrac_MRev);
					STPOSMOVE_setVelocityLimit(st_obj[HAL_MTR1].posMoveHandle,
							_IQmpy(gMotorVars[HAL_MTR1].MaxVel_krpm, gSpeed_krpm_to_pu_sf[HAL_MTR1]));
					STPOSMOVE_setAccelerationLimit(st_obj[HAL_MTR1].posMoveHandle,
							_IQmpy(gMotorVars[HAL_MTR1].MaxAccel_krpmps, gSpeed_krpm_to_pu_sf[HAL_MTR1]));
					STPOSMOVE_setDecelerationLimit(st_obj[HAL_MTR1].posMoveHandle,
							_IQmpy(gMotorVars[HAL_MTR1].MaxDecel_krpmps, gSpeed_krpm_to_pu_sf[HAL_MTR1]));
					STPOSMOVE_setJerkLimit(st_obj[HAL_MTR1].posMoveHandle,
							_IQ20mpy(gMotorVars[HAL_MTR1].MaxJrk_krpmps2, _IQtoIQ20(gSpeed_krpm_to_pu_sf[HAL_MTR1])));
					// Enable the SpinTAC Position Profile Generator
					STPOSMOVE_setEnable(st_obj[HAL_MTR1].posMoveHandle, true);
					// clear the position step command
					gMotorVars[HAL_MTR1].PosStepInt_MRev = 0;
					gMotorVars[HAL_MTR1].PosStepFrac_MRev = 0;
					gMotorVars[HAL_MTR1].RunPositionProfile = false;
				}*/

				// The next instruction executes SpinTAC Velocity Move
				// This is the speed profile generation
				//STPOSMOVE_run(st_obj[HAL_MTR1].posMoveHandle);

				calcTransitionPosRef(HAL_MTR1);

				// The next instruction executes SpinTAC Position Control and places
				// its output in Idq_ref_pu.value[1], which is the input reference
				// value for the q-axis current controller.
				gIdq_ref_pu[HAL_MTR1].value[1] = runPosCtl(HAL_MTR1);
			} else {
				// increment counter
				stCntPosition[HAL_MTR1]++;
			}

			// generate the motor electrical angle
			if (gUserParams[HAL_MTR1].motor_type == MOTOR_Type_Induction) {
				// update the electrical angle for the SLIP module
				SLIP_setElectricalAngle(slipHandle[HAL_MTR1], ENC_getElecAngle(encHandle[HAL_MTR1]));
				// compute the amount of slip
				SLIP_run(slipHandle[HAL_MTR1]);
				// set magnetic angle
				angle_pu = SLIP_getMagneticAngle(slipHandle[HAL_MTR1]);
			} else {
				angle_pu = ENC_getElecAngle(encHandle[HAL_MTR1]);
			}

			speed_pu = STPOSCONV_getVelocity(st_obj[HAL_MTR1].posConvHandle);
		} else {  // the alignment procedure is in effect

			// force motor angle and speed to 0
			angle_pu = _IQ(0.0);
			speed_pu = _IQ(0.0);

			// set D-axis current to Rs estimation current
			gIdq_ref_pu[HAL_MTR1].value[0] = _IQ(USER_MOTOR_RES_EST_CURRENT/USER_IQ_FULL_SCALE_CURRENT_A);
			// set Q-axis current to 0
			gIdq_ref_pu[HAL_MTR1].value[1] = _IQ(0.0);

			uint32_t encValue = HAL_getQepPosnCounts(halHandleMtr[HAL_MTR1]);

			// save encoder reading when forcing motor into alignment
			if (gUserParams[HAL_MTR1].motor_type == MOTOR_Type_Pm) {
				ENC_setZeroOffset(encHandle[HAL_MTR1],
						(uint32_t) (HAL_getQepPosnMaximum(halHandleMtr[HAL_MTR1])
								- encValue));
			}

			if (enc1PrevValue == encValue) {
				enc1StableAlignCount++;
			} else {
				enc1StableAlignCount = 0;
			}

			enc1PrevValue = encValue;

			if (enc1StableAlignCount >= enc1StableAlignLimit) {
				// alignment done
				gMotorVars[HAL_MTR1].Flag_enableAlignment = false;
				enc1StableAlignCount = 0;
				gAlignCount[HAL_MTR1] = 0;
				gIdq_ref_pu[HAL_MTR1].value[0] = _IQ(0.0);
			}

			if (gAlignCount[HAL_MTR1]++ >= gUserParams[HAL_MTR1].ctrlWaitTime[CTRL_State_OffLine]) {
				// failed to align
				gMotorVars[HAL_MTR1].Flag_Run_Identify = false;
				gMotorVars[HAL_MTR1].Flag_enableAlignment = true;
				enc1StableAlignCount = 0;
				gAlignCount[HAL_MTR1] = 0;
				gIdq_ref_pu[HAL_MTR1].value[0] = _IQ(0.0);
			}

			// if alignment counter exceeds threshold, exit alignment
			/*if (gAlignCount[HAL_MTR1]++ >= gUserParams[HAL_MTR1].ctrlWaitTime[CTRL_State_OffLine]) {
				gMotorVars[HAL_MTR1].Flag_enableAlignment = false;
				gAlignCount[HAL_MTR1] = 0;
				gIdq_ref_pu[HAL_MTR1].value[0] = _IQ(0.0);
			}*/
		}

		// Get the reference value for the d-axis current controller.
		refValue = gIdq_ref_pu[HAL_MTR1].value[0];

		// Get the actual value of Id
		fbackValue = gIdq_pu[HAL_MTR1].value[0];

		// The next instruction executes the PI current controller for the
		// d axis and places its output in Vdq_pu.value[0], which is the
		// control voltage along the d-axis (Vd)
		PID_run(pidHandle[HAL_MTR1][1], refValue, fbackValue, &(gVdq_out_pu[HAL_MTR1].value[0]));

		// get the Iq reference value
		refValue = gIdq_ref_pu[HAL_MTR1].value[1];

		// get the actual value of Iq
		fbackValue = gIdq_pu[HAL_MTR1].value[1];

		// The voltage limits on the output of the q-axis current controller
		// are dynamic, and are dependent on the output voltage from the d-axis
		// current controller.  In other words, the d-axis current controller
		// gets first dibs on the available voltage, and the q-axis current
		// controller gets what's left over.  That is why the d-axis current
		// controller executes first. The next instruction calculates the
		// maximum limits for this voltage as:
		// Vq_min_max = +/- sqrt(Vbus^2 - Vd^2)
		outMax_pu =
				_IQsqrt(
						_IQ(USER_MAX_VS_MAG_PU * USER_MAX_VS_MAG_PU) - _IQmpy(gVdq_out_pu[HAL_MTR1].value[0],gVdq_out_pu[HAL_MTR1].value[0]));

		// Set the limits to +/- outMax_pu
		PID_setMinMax(pidHandle[HAL_MTR1][2], -outMax_pu, outMax_pu);

		// The next instruction executes the PI current controller for the
		// q axis and places its output in Vdq_pu.value[1], which is the
		// control voltage vector along the q-axis (Vq)
		PID_run(pidHandle[HAL_MTR1][2], refValue, fbackValue, &(gVdq_out_pu[HAL_MTR1].value[1]));

		// The voltage vector is now calculated and ready to be applied to the
		// motor in the form of three PWM signals.  However, even though the
		// voltages may be supplied to the PWM module now, they won't be
		// applied to the motor until the next PWM cycle. By this point, the
		// motor will have moved away from the angle that the voltage vector
		// was calculated for, by an amount which is proportional to the
		// sampling frequency and the speed of the motor.  For steady-state
		// speeds, we can calculate this angle delay and compensate for it.
		ANGLE_COMP_run(angleCompHandle[HAL_MTR1], speed_pu, angle_pu);
		angle_pu = ANGLE_COMP_getAngleComp_pu(angleCompHandle[HAL_MTR1]);

		// compute the sine and cosine phasor values which are part of the inverse
		// Park transform calculations. Once these values are computed,
		// they are copied into the IPARK module, which then uses them to
		// transform the voltages from DQ to Alpha/Beta reference frames.
		phasor.value[0] = _IQcosPU(angle_pu);
		phasor.value[1] = _IQsinPU(angle_pu);

		// set the phasor in the inverse Park transform
		IPARK_setPhasor(iparkHandle[HAL_MTR1], &phasor);

		// Run the inverse Park module.  This converts the voltage vector from
		// synchronous frame values to stationary frame values.
		IPARK_run(iparkHandle[HAL_MTR1], &gVdq_out_pu[HAL_MTR1], &Vab_pu);

		// These 3 statements compensate for variations in the DC bus by adjusting the
		// PWM duty cycle. The goal is to achieve the same volt-second product
		// regardless of the DC bus value.  To do this, we must divide the desired voltage
		// values by the DC bus value.  Or...it is easier to multiply by 1/(DC bus value).
		oneOverDcBus = _IQdiv(_IQ(1.0), gAdcData[HAL_MTR1].dcBus);        //EST_getOneOverDcBus_pu(estHandle[HAL_MTR1]);
		Vab_pu.value[0] = _IQmpy(Vab_pu.value[0], oneOverDcBus);
		Vab_pu.value[1] = _IQmpy(Vab_pu.value[1], oneOverDcBus);

		// Now run the space vector generator (SVGEN) module.
		// There is no need to do an inverse CLARKE transform, as this is
		// handled in the SVGEN_run function.
		SVGEN_run(svgenHandle[HAL_MTR1], &Vab_pu, &(gPwmData[HAL_MTR1].Tabc));
	} else if (gMotorVars[HAL_MTR1].Flag_enableOffsetcalc == true) {
		runOffsetsCalculation(HAL_MTR1);
	} else  // gMotorVars.Flag_Run_Identify = 0
	{
		// disable the PWM
		HAL_disablePwm(halHandleMtr[HAL_MTR1]);

		// Set the PWMs to 50% duty cycle
		gPwmData[HAL_MTR1].Tabc.value[0] = _IQ(0.0);
		gPwmData[HAL_MTR1].Tabc.value[1] = _IQ(0.0);
		gPwmData[HAL_MTR1].Tabc.value[2] = _IQ(0.0);
	}

	// write to the PWM compare registers, and then we are done!
	HAL_writePwmData(halHandleMtr[HAL_MTR1], &gPwmData[HAL_MTR1]);

	HAL_setGpioLow(halHandle, GPIO_Number_12);

	return;
} // end of motor1_ISR() function

interrupt void motor2_ISR(void) {
	// Declaration of local variables
	static _iq angle_pu = _IQ(0.0);
	_iq speed_pu = _IQ(0.0);
	_iq oneOverDcBus;
	MATH_vec2 Iab_pu;
	MATH_vec2 Vab_pu;
	MATH_vec2 phasor;

	HAL_setGpioHigh(halHandle, GPIO_Number_20);

	// acknowledge the ADC interrupt
	HAL_acqAdcInt(halHandle, ADC_IntNumber_2);

	// convert the ADC data
	HAL_readAdcDataWithOffsets(halHandle, halHandleMtr[HAL_MTR2], &gAdcData[HAL_MTR2]);

	// remove offsets
	gAdcData[HAL_MTR2].I.value[0] = gAdcData[HAL_MTR2].I.value[0] - gOffsets_I_pu[HAL_MTR2].value[0];
	gAdcData[HAL_MTR2].I.value[1] = gAdcData[HAL_MTR2].I.value[1] - gOffsets_I_pu[HAL_MTR2].value[1];
	gAdcData[HAL_MTR2].I.value[2] = gAdcData[HAL_MTR2].I.value[2] - gOffsets_I_pu[HAL_MTR2].value[2];
	gAdcData[HAL_MTR2].V.value[0] = gAdcData[HAL_MTR2].V.value[0] - gOffsets_V_pu[HAL_MTR2].value[0];
	gAdcData[HAL_MTR2].V.value[1] = gAdcData[HAL_MTR2].V.value[1] - gOffsets_V_pu[HAL_MTR2].value[1];
	gAdcData[HAL_MTR2].V.value[2] = gAdcData[HAL_MTR2].V.value[2] - gOffsets_V_pu[HAL_MTR2].value[2];

	// run Clarke transform on current.  Three values are passed, two values
	// are returned.
	CLARKE_run(clarkeHandle_I[HAL_MTR2], &gAdcData[HAL_MTR2].I, &Iab_pu);

	// compute the sine and cosine phasor values which are part of the
	// Park transform calculations. Once these values are computed,
	// they are copied into the PARK module, which then uses them to
	// transform the voltages from Alpha/Beta to DQ reference frames.
	phasor.value[0] = _IQcosPU(angle_pu);
	phasor.value[1] = _IQsinPU(angle_pu);

	// set the phasor in the Park transform
	PARK_setPhasor(parkHandle[HAL_MTR2], &phasor);

	// Run the Park module.  This converts the current vector from
	// stationary frame values to synchronous frame values.
	PARK_run(parkHandle[HAL_MTR2], &Iab_pu, &gIdq_pu[HAL_MTR2]);

	// compute the electrical angle
	ENC_calcElecAngle(encHandle[HAL_MTR2], HAL_getQepPosnCounts(halHandleMtr[HAL_MTR2]));

	//bool shouldRunPosCtl = false;

	if (stCntPosition[HAL_MTR2] >= gUserParams[HAL_MTR2].numCtrlTicksPerSpeedTick) {
		// Calculate the feedback position, this must always be ran in order to ensure there are no jumps
		ST_runPosConv(stHandle[HAL_MTR2], encHandle[HAL_MTR2], slipHandle[HAL_MTR2], &gIdq_pu[HAL_MTR2],
				gUserParams[HAL_MTR2].motor_type);
	}

	// run the appropriate controller
	if (gMotorVars[HAL_MTR2].Flag_Run_Identify) {
		// Declaration of local variables.
		_iq refValue;
		_iq fbackValue;
		_iq outMax_pu;

		// check if the motor should be forced into encoder slignment
		if (gMotorVars[HAL_MTR2].Flag_enableAlignment == false) {
			// when appropriate, run SpinTAC Position Control
			// This mechanism provides the decimation for the speed loop.
			if (stCntPosition[HAL_MTR2] >= gUserParams[HAL_MTR2].numCtrlTicksPerSpeedTick) {
				// Reset the Position execution counter.
				stCntPosition[HAL_MTR2] = 0;

				/*if ((STPOSMOVE_getStatus(st_obj[HAL_MTR2].posMoveHandle) == ST_MOVE_IDLE)
						&& (gMotorVars[HAL_MTR2].RunPositionProfile == true)) {
					// Get the configuration for SpinTAC Position Move
					STPOSMOVE_setCurveType(st_obj[HAL_MTR2].posMoveHandle,
							gMotorVars[HAL_MTR2].SpinTAC.PosMoveCurveType);
					STPOSMOVE_setPositionStep_mrev(st_obj[HAL_MTR2].posMoveHandle, gMotorVars[HAL_MTR2].PosStepInt_MRev,
							gMotorVars[HAL_MTR2].PosStepFrac_MRev);
					STPOSMOVE_setVelocityLimit(st_obj[HAL_MTR2].posMoveHandle,
							_IQmpy(gMotorVars[HAL_MTR2].MaxVel_krpm, gSpeed_krpm_to_pu_sf[HAL_MTR2]));
					STPOSMOVE_setAccelerationLimit(st_obj[HAL_MTR2].posMoveHandle,
							_IQmpy(gMotorVars[HAL_MTR2].MaxAccel_krpmps, gSpeed_krpm_to_pu_sf[HAL_MTR2]));
					STPOSMOVE_setDecelerationLimit(st_obj[HAL_MTR2].posMoveHandle,
							_IQmpy(gMotorVars[HAL_MTR2].MaxDecel_krpmps, gSpeed_krpm_to_pu_sf[HAL_MTR2]));
					STPOSMOVE_setJerkLimit(st_obj[HAL_MTR2].posMoveHandle,
							_IQ20mpy(gMotorVars[HAL_MTR2].MaxJrk_krpmps2, _IQtoIQ20(gSpeed_krpm_to_pu_sf[HAL_MTR2])));
					// Enable the SpinTAC Position Profile Generator
					STPOSMOVE_setEnable(st_obj[HAL_MTR2].posMoveHandle, true);
					// clear the position step command
					gMotorVars[HAL_MTR2].PosStepInt_MRev = 0;
					gMotorVars[HAL_MTR2].PosStepFrac_MRev = 0;
					gMotorVars[HAL_MTR2].RunPositionProfile = false;
				}*/

				// The next instruction executes SpinTAC Velocity Move
				// This is the speed profile generation
				//STPOSMOVE_run(st_obj[HAL_MTR2].posMoveHandle);

				calcTransitionPosRef(HAL_MTR2);

				// The next instruction executes SpinTAC Position Control and places
				// its output in Idq_ref_pu.value[1], which is the input reference
				// value for the q-axis current controller.
				gIdq_ref_pu[HAL_MTR2].value[1] = runPosCtl(HAL_MTR2);
			} else {
				// increment counter
				stCntPosition[HAL_MTR2]++;
			}

			// generate the motor electrical angle
			if (gUserParams[HAL_MTR2].motor_type == MOTOR_Type_Induction) {
				// update the electrical angle for the SLIP module
				SLIP_setElectricalAngle(slipHandle[HAL_MTR2], ENC_getElecAngle(encHandle[HAL_MTR2]));
				// compute the amount of slip
				SLIP_run(slipHandle[HAL_MTR2]);
				// set magnetic angle
				angle_pu = SLIP_getMagneticAngle(slipHandle[HAL_MTR2]);
			} else {
				angle_pu = ENC_getElecAngle(encHandle[HAL_MTR2]);
			}

			speed_pu = STPOSCONV_getVelocity(st_obj[HAL_MTR2].posConvHandle);
		} else {  // the alignment procedure is in effect

			// force motor angle and speed to 0
			angle_pu = _IQ(0.0);
			speed_pu = _IQ(0.0);

			// set D-axis current to Rs estimation current
			gIdq_ref_pu[HAL_MTR2].value[0] = _IQ(USER_MOTOR_RES_EST_CURRENT_2/USER_IQ_FULL_SCALE_CURRENT_A_2);
			// set Q-axis current to 0
			gIdq_ref_pu[HAL_MTR2].value[1] = _IQ(0.0);

			uint32_t encValue = HAL_getQepPosnCounts(halHandleMtr[HAL_MTR2]);

			// save encoder reading when forcing motor into alignment
			if (gUserParams[HAL_MTR2].motor_type == MOTOR_Type_Pm) {
				ENC_setZeroOffset(encHandle[HAL_MTR2],
						(uint32_t) (HAL_getQepPosnMaximum(halHandleMtr[HAL_MTR2])
								- encValue));
			}

			if (enc2PrevValue == encValue) {
				enc2StableAlignCount++;
			} else {
				enc2StableAlignCount = 0;
			}

			enc2PrevValue = encValue;

			if (enc2StableAlignCount >= enc2StableAlignLimit) {
				// alignment done
				gMotorVars[HAL_MTR2].Flag_enableAlignment = false;
				enc2StableAlignCount = 0;
				gAlignCount[HAL_MTR2] = 0;
				gIdq_ref_pu[HAL_MTR2].value[0] = _IQ(0.0);
			}

			if (gAlignCount[HAL_MTR2]++ >= gUserParams[HAL_MTR2].ctrlWaitTime[CTRL_State_OffLine]) {
				// failed to align
				gMotorVars[HAL_MTR2].Flag_Run_Identify = false;
				gMotorVars[HAL_MTR2].Flag_enableAlignment = true;
				enc2StableAlignCount = 0;
				gAlignCount[HAL_MTR2] = 0;
				gIdq_ref_pu[HAL_MTR2].value[0] = _IQ(0.0);
			}

			// if alignment counter exceeds threshold, exit alignment
			/*if (gAlignCount[HAL_MTR2]++ >= gUserParams[HAL_MTR2].ctrlWaitTime[CTRL_State_OffLine]) {
				gMotorVars[HAL_MTR2].Flag_enableAlignment = false;
				gAlignCount[HAL_MTR2] = 0;
				gIdq_ref_pu[HAL_MTR2].value[0] = _IQ(0.0);
			}*/
		}

		// Get the reference value for the d-axis current controller.
		refValue = gIdq_ref_pu[HAL_MTR2].value[0];

		// Get the actual value of Id
		fbackValue = gIdq_pu[HAL_MTR2].value[0];

		// The next instruction executes the PI current controller for the
		// d axis and places its output in Vdq_pu.value[0], which is the
		// control voltage along the d-axis (Vd)
		PID_run(pidHandle[HAL_MTR2][1], refValue, fbackValue, &(gVdq_out_pu[HAL_MTR2].value[0]));

		// get the Iq reference value
		refValue = gIdq_ref_pu[HAL_MTR2].value[1];

		// get the actual value of Iq
		fbackValue = gIdq_pu[HAL_MTR2].value[1];

		// The voltage limits on the output of the q-axis current controller
		// are dynamic, and are dependent on the output voltage from the d-axis
		// current controller.  In other words, the d-axis current controller
		// gets first dibs on the available voltage, and the q-axis current
		// controller gets what's left over.  That is why the d-axis current
		// controller executes first. The next instruction calculates the
		// maximum limits for this voltage as:
		// Vq_min_max = +/- sqrt(Vbus^2 - Vd^2)
		outMax_pu =
				_IQsqrt(
						_IQ(USER_MAX_VS_MAG_PU_2 * USER_MAX_VS_MAG_PU_2) - _IQmpy(gVdq_out_pu[HAL_MTR2].value[0],gVdq_out_pu[HAL_MTR2].value[0]));

		// Set the limits to +/- outMax_pu
		PID_setMinMax(pidHandle[HAL_MTR2][2], -outMax_pu, outMax_pu);

		// The next instruction executes the PI current controller for the
		// q axis and places its output in Vdq_pu.value[1], which is the
		// control voltage vector along the q-axis (Vq)
		PID_run(pidHandle[HAL_MTR2][2], refValue, fbackValue, &(gVdq_out_pu[HAL_MTR2].value[1]));

		// The voltage vector is now calculated and ready to be applied to the
		// motor in the form of three PWM signals.  However, even though the
		// voltages may be supplied to the PWM module now, they won't be
		// applied to the motor until the next PWM cycle. By this point, the
		// motor will have moved away from the angle that the voltage vector
		// was calculated for, by an amount which is proportional to the
		// sampling frequency and the speed of the motor.  For steady-state
		// speeds, we can calculate this angle delay and compensate for it.
		ANGLE_COMP_run(angleCompHandle[HAL_MTR2], speed_pu, angle_pu);
		angle_pu = ANGLE_COMP_getAngleComp_pu(angleCompHandle[HAL_MTR2]);

		// compute the sine and cosine phasor values which are part of the inverse
		// Park transform calculations. Once these values are computed,
		// they are copied into the IPARK module, which then uses them to
		// transform the voltages from DQ to Alpha/Beta reference frames.
		phasor.value[0] = _IQcosPU(angle_pu);
		phasor.value[1] = _IQsinPU(angle_pu);

		// set the phasor in the inverse Park transform
		IPARK_setPhasor(iparkHandle[HAL_MTR2], &phasor);

		// Run the inverse Park module.  This converts the voltage vector from
		// synchronous frame values to stationary frame values.
		IPARK_run(iparkHandle[HAL_MTR2], &gVdq_out_pu[HAL_MTR2], &Vab_pu);

		// These 3 statements compensate for variations in the DC bus by adjusting the
		// PWM duty cycle. The goal is to achieve the same volt-second product
		// regardless of the DC bus value.  To do this, we must divide the desired voltage
		// values by the DC bus value.  Or...it is easier to multiply by 1/(DC bus value).
		oneOverDcBus = _IQdiv(_IQ(1.0), gAdcData[HAL_MTR2].dcBus);
		Vab_pu.value[0] = _IQmpy(Vab_pu.value[0], oneOverDcBus);
		Vab_pu.value[1] = _IQmpy(Vab_pu.value[1], oneOverDcBus);

		// Now run the space vector generator (SVGEN) module.
		// There is no need to do an inverse CLARKE transform, as this is
		// handled in the SVGEN_run function.
		SVGEN_run(svgenHandle[HAL_MTR2], &Vab_pu, &(gPwmData[HAL_MTR2].Tabc));
	} else if (gMotorVars[HAL_MTR2].Flag_enableOffsetcalc == true) {
		runOffsetsCalculation(HAL_MTR2);
	} else  // gMotorVars.Flag_Run_Identify = 0
	{
		// disable the PWM
		HAL_disablePwm(halHandleMtr[HAL_MTR2]);

		// Set the PWMs to 50% duty cycle
		gPwmData[HAL_MTR2].Tabc.value[0] = _IQ(0.0);
		gPwmData[HAL_MTR2].Tabc.value[1] = _IQ(0.0);
		gPwmData[HAL_MTR2].Tabc.value[2] = _IQ(0.0);
	}

	// write to the PWM compare registers, and then we are done!
	HAL_writePwmData(halHandleMtr[HAL_MTR2], &gPwmData[HAL_MTR2]);

	HAL_setGpioLow(halHandle, GPIO_Number_20);

	return;
} // end of motor2_ISR() function

//! \brief the ISR for SCI-B receive interrupt
interrupt void sciBRxISR(void) {
	HAL_Obj *obj = (HAL_Obj *) halHandle;
	//CTRL_Obj *ctrlObj = (CTRL_Obj *) ctrlHandle;
	//ST_Obj *stObj = (ST_Obj *)stHandle;

	//dataRx = SCI_getDataNonBlocking(halHandle->sciBHandle, &success);
	//success = SCI_putDataNonBlocking(halHandle->sciBHandle, dataRx);

	// acknowledge interrupt from SCI group so that SCI interrupt
	// is not received twice
	PIE_clearInt(obj->pieHandle, PIE_GroupNumber_9);

	while (SCI_rxDataReady(halHandle->sciBHandle)) {
		dataRx = SCI_read(halHandle->sciBHandle);

		if (counter < 18) {
			if (counter == 0) {
				if (dataRx == '<') {
					buf[counter] = dataRx;
					counter++;
				} else {
					counter = 0;
				}
			} else if (counter >= 1 && counter <= 16) {
				buf[counter] = dataRx;
				counter++;
			} else if (counter == 17) {
				buf[counter] = dataRx;

				if (dataRx == '>') {
					//buf[counter] = dataRx;
					//counter++;

					if (isMotorActive(HAL_MTR1)) {
						positionParamsList[HAL_MTR1].posRef =
								((long) buf[1])
								| ((long) buf[2] << 8)
								| ((long) buf[3] << 16)
								| ((long) buf[4] << 24);
						positionParamsList[HAL_MTR1].maxSpeed_rps =
								((long) buf[5])
								| ((long) buf[6] << 8)
								| ((long) buf[7] << 16)
								| ((long) buf[8] << 24);
					}

					if (isMotorActive(HAL_MTR2)) {
						positionParamsList[HAL_MTR2].posRef =
								((long) buf[9])
								| ((long) buf[10] << 8)
								| ((long) buf[11] << 16)
								| ((long) buf[12] << 24);
						positionParamsList[HAL_MTR2].maxSpeed_rps =
								((long) buf[13])
								| ((long) buf[14] << 8)
								| ((long) buf[15] << 16)
								| ((long) buf[16] << 24);
					}

					counter = 0;

					sendFeedback = 1;

				} else {
					counter = 0;
				}
			} else {
				counter = 0;
			}
		}
	}

} // end of sciBRxISR() function

void pidSetup(HAL_MtrSelect_e mtrNum) {
	// This equation uses the scaled maximum voltage vector, which is
	// already in per units, hence there is no need to include the #define
	// for USER_IQ_FULL_SCALE_VOLTAGE_V
	_iq maxVoltage_pu = _IQ(gUserParams[mtrNum].maxVsMag_pu * gUserParams[mtrNum].voltage_sf);

	float_t fullScaleCurrent = gUserParams[mtrNum].iqFullScaleCurrent_A;
	float_t fullScaleVoltage = gUserParams[mtrNum].iqFullScaleVoltage_V;
	float_t IsrPeriod_sec = 1.0e-6 * gUserParams[mtrNum].pwmPeriod_usec * gUserParams[mtrNum].numPwmTicksPerIsrTick;
	float_t Ls_d = gUserParams[mtrNum].motor_Ls_d;
	float_t Ls_q = gUserParams[mtrNum].motor_Ls_q;
	float_t Rs = gUserParams[mtrNum].motor_Rs;

	// This lab assumes that motor parameters are known, and it does not
	// perform motor ID, so the R/L parameters are known and defined in
	// user.h
	float_t RoverLs_d = Rs / Ls_d;
	float_t RoverLs_q = Rs / Ls_q;

	// For the current controller, Kp = Ls*bandwidth(rad/sec)  But in order
	// to be used, it must be converted to per unit values by multiplying
	// by fullScaleCurrent and then dividing by fullScaleVoltage.  From the
	// statement below, we see that the bandwidth in rad/sec is equal to
	// 0.25/IsrPeriod_sec, which is equal to USER_ISR_FREQ_HZ/4. This means
	// that by setting Kp as described below, the bandwidth in Hz is
	// USER_ISR_FREQ_HZ/(8*pi).
	_iq Kp_Id = _IQ((0.25 * Ls_d * fullScaleCurrent) / (IsrPeriod_sec * fullScaleVoltage));

	// In order to achieve pole/zero cancellation (which reduces the
	// closed-loop transfer function from a second-order system to a
	// first-order system), Ki must equal Rs/Ls.  Since the output of the
	// Ki gain stage is integrated by a DIGITAL integrator, the integrator
	// input must be scaled by 1/IsrPeriod_sec.  That's just the way
	// digital integrators work.  But, since IsrPeriod_sec is a constant,
	// we can save an additional multiplication operation by lumping this
	// term with the Ki value.
	_iq Ki_Id = _IQ(RoverLs_d * IsrPeriod_sec);

	// Now do the same thing for Kp for the q-axis current controller.
	// If the motor is not an IPM motor, Ld and Lq are the same, which
	// means that Kp_Iq = Kp_Id
	_iq Kp_Iq = _IQ((0.25 * Ls_q * fullScaleCurrent) / (IsrPeriod_sec * fullScaleVoltage));

	// Do the same thing for Ki for the q-axis current controller.  If the
	// motor is not an IPM motor, Ld and Lq are the same, which means that
	// Ki_Iq = Ki_Id.
	_iq Ki_Iq = _IQ(RoverLs_q * IsrPeriod_sec);

	// There are two PI controllers; two current
	// controllers.  Each PI controller has two coefficients; Kp and Ki.
	// So you have a total of four coefficients that must be defined.
	// This is for the Id current controller
	pidHandle[mtrNum][1] = PID_init(&pid[mtrNum][1], sizeof(pid[mtrNum][1]));
	// This is for the Iq current controller
	pidHandle[mtrNum][2] = PID_init(&pid[mtrNum][2], sizeof(pid[mtrNum][2]));

	stCntPosition[mtrNum] = 0;  // Set the counter for decimating the speed
								// controller to 0

	// The following instructions load the parameters for the d-axis
	// current controller.
	// P term = Kp_Id, I term = Ki_Id, D term = 0
	PID_setGains(pidHandle[mtrNum][1], Kp_Id, Ki_Id, _IQ(0.0));

	// Largest negative voltage = -maxVoltage_pu, largest positive
	// voltage = maxVoltage_pu
	PID_setMinMax(pidHandle[mtrNum][1], -maxVoltage_pu, maxVoltage_pu);

	// Set the initial condition value for the integrator output to 0
	PID_setUi(pidHandle[mtrNum][1], _IQ(0.0));

	// The following instructions load the parameters for the q-axis
	// current controller.
	// P term = Kp_Iq, I term = Ki_Iq, D term = 0
	PID_setGains(pidHandle[mtrNum][2], Kp_Iq, Ki_Iq, _IQ(0.0));

	// The largest negative voltage = 0 and the largest positive
	// voltage = 0.  But these limits are updated every single ISR before
	// actually executing the Iq controller. The limits depend on how much
	// voltage is left over after the Id controller executes. So having an
	// initial value of 0 does not affect Iq current controller execution.
	PID_setMinMax(pidHandle[mtrNum][2], _IQ(0.0), _IQ(0.0));

	// Set the initial condition value for the integrator output to 0
	PID_setUi(pidHandle[mtrNum][2], _IQ(0.0));
}

void runOffsetsCalculation(HAL_MtrSelect_e mtrNum) {
	uint16_t cnt;

	// enable the PWM
	HAL_enablePwm(halHandleMtr[mtrNum]);

	for (cnt = 0; cnt < 3; cnt++) {
		// Set the PWMs to 50% duty cycle
		gPwmData[mtrNum].Tabc.value[cnt] = _IQ(0.0);

		// reset offsets used
		gOffsets_I_pu[mtrNum].value[cnt] = _IQ(0.0);
		gOffsets_V_pu[mtrNum].value[cnt] = _IQ(0.0);

		// run offset estimation
		FILTER_FO_run(filterHandle[mtrNum][cnt], gAdcData[mtrNum].I.value[cnt]);
		FILTER_FO_run(filterHandle[mtrNum][cnt + 3], gAdcData[mtrNum].V.value[cnt]);
	}

	if (gOffsetCalcCount[mtrNum]++ >= gUserParams[mtrNum].ctrlWaitTime[CTRL_State_OffLine]) {
		gMotorVars[mtrNum].Flag_enableOffsetcalc = false;
		gOffsetCalcCount[mtrNum] = 0;

		for (cnt = 0; cnt < 3; cnt++) {
			// get calculated offsets from filter
			gOffsets_I_pu[mtrNum].value[cnt] = FILTER_FO_get_y1(filterHandle[mtrNum][cnt]);
			gOffsets_V_pu[mtrNum].value[cnt] = FILTER_FO_get_y1(filterHandle[mtrNum][cnt + 3]);

			// clear filters
			FILTER_FO_setInitialConditions(filterHandle[mtrNum][cnt], _IQ(0.0), _IQ(0.0));
			FILTER_FO_setInitialConditions(filterHandle[mtrNum][cnt + 3], _IQ(0.0), _IQ(0.0));
		}
	}

	return;
} // end of runOffsetsCalculation() function

//! \brief  Call this function to fix 1p6. This is only used for F2806xF/M
//! \brief  implementation of InstaSPIN (version 1.6 of ROM) since the
//! \brief  inductance calculation is not done correctly in ROM, so this
//! \brief  function fixes that ROM bug.
void softwareUpdate1p6(EST_Handle handle, USER_Params *pUserParams) {
	float_t iqFullScaleVoltage_V = pUserParams->iqFullScaleVoltage_V;
	float_t iqFullScaleCurrent_A = pUserParams->iqFullScaleCurrent_A;
	float_t voltageFilterPole_rps = pUserParams->voltageFilterPole_rps;
	float_t motorLs_d = pUserParams->motor_Ls_d;
	float_t motorLs_q = pUserParams->motor_Ls_q;

	float_t fullScaleInductance = iqFullScaleVoltage_V / (iqFullScaleCurrent_A * voltageFilterPole_rps);
	float_t Ls_coarse_max = _IQ30toF(EST_getLs_coarse_max_pu(handle));
	int_least8_t lShift = ceil(log(motorLs_d / (Ls_coarse_max * fullScaleInductance)) / log(2.0));
	uint_least8_t Ls_qFmt = 30 - lShift;
	float_t L_max = fullScaleInductance * pow(2.0, lShift);
	_iq Ls_d_pu = _IQ30(motorLs_d / L_max);
	_iq Ls_q_pu = _IQ30(motorLs_q / L_max);

	// store the results
	EST_setLs_d_pu(handle, Ls_d_pu);
	EST_setLs_q_pu(handle, Ls_q_pu);
	EST_setLs_qFmt(handle, Ls_qFmt);

	return;
} // end of softwareUpdate1p6() function

//! \brief     Setup the Clarke transform for either 2 or 3 sensors.
//! \param[in] handle             The clarke (CLARKE) handle
//! \param[in] numCurrentSensors  The number of current sensors
void setupClarke_I(CLARKE_Handle handle, const uint_least8_t numCurrentSensors) {
	_iq alpha_sf, beta_sf;

	// initialize the Clarke transform module for current
	if (numCurrentSensors == 3) {
		alpha_sf = _IQ(MATH_ONE_OVER_THREE);
		beta_sf = _IQ(MATH_ONE_OVER_SQRT_THREE);
	} else if (numCurrentSensors == 2) {
		alpha_sf = _IQ(1.0);
		beta_sf = _IQ(MATH_ONE_OVER_SQRT_THREE);
	} else {
		alpha_sf = _IQ(0.0);
		beta_sf = _IQ(0.0);
	}

	// set the parameters
	CLARKE_setScaleFactors(handle, alpha_sf, beta_sf);
	CLARKE_setNumSensors(handle, numCurrentSensors);

	return;
} // end of setupClarke_I() function

//! \brief     Setup the Clarke transform for either 2 or 3 sensors.
//! \param[in] handle             The clarke (CLARKE) handle
//! \param[in] numVoltageSensors  The number of voltage sensors
void setupClarke_V(CLARKE_Handle handle, const uint_least8_t numVoltageSensors) {
	_iq alpha_sf, beta_sf;

	// initialize the Clarke transform module for voltage
	if (numVoltageSensors == 3) {
		alpha_sf = _IQ(MATH_ONE_OVER_THREE);
		beta_sf = _IQ(MATH_ONE_OVER_SQRT_THREE);
	} else {
		alpha_sf = _IQ(0.0);
		beta_sf = _IQ(0.0);
	}

	// In other words, the only acceptable number of voltage sensors is three.
	// set the parameters
	CLARKE_setScaleFactors(handle, alpha_sf, beta_sf);
	CLARKE_setNumSensors(handle, numVoltageSensors);

	return;
} // end of setupClarke_V() function

//! \brief     Update the global variables (gMotorVars).
//! \param[in] handle  The estimator (EST) handle
void updateGlobalVariables(EST_Handle handle, const uint_least8_t mtrNum) {
	//uint32_t profile_mticks, profile_ticks;

	// get the speed estimate
	gMotorVars[mtrNum].Speed_krpm = _IQmpy(STPOSCONV_getVelocityFiltered(st_obj[mtrNum].posConvHandle),
			gSpeed_pu_to_krpm_sf[mtrNum]);

	// Get the DC buss voltage
	gMotorVars[mtrNum].VdcBus_kV = _IQmpy(gAdcData[mtrNum].dcBus,
			_IQ(gUserParams[mtrNum].iqFullScaleVoltage_V / 1000.0));

	// read Vd and Vq vectors per units
	gMotorVars[mtrNum].Vd = gVdq_out_pu[mtrNum].value[0];
	gMotorVars[mtrNum].Vq = gVdq_out_pu[mtrNum].value[1];

	// calculate vector Vs in per units: (Vs = sqrt(Vd^2 + Vq^2))
	gMotorVars[mtrNum].Vs = _IQsqrt(
			_IQmpy(gMotorVars[mtrNum].Vd,gMotorVars[mtrNum].Vd) + _IQmpy(gMotorVars[mtrNum].Vq,gMotorVars[mtrNum].Vq));

	// read Id and Iq vectors in amps
	gMotorVars[mtrNum].Id_A = _IQmpy(gIdq_pu[mtrNum].value[0], _IQ(gUserParams[mtrNum].iqFullScaleCurrent_A));
	gMotorVars[mtrNum].Iq_A = _IQmpy(gIdq_pu[mtrNum].value[1], _IQ(gUserParams[mtrNum].iqFullScaleCurrent_A));

	// calculate vector Is in amps:  (Is_A = sqrt(Id_A^2 + Iq_A^2))
	gMotorVars[mtrNum].Is_A =
			_IQsqrt(
					_IQmpy(gMotorVars[mtrNum].Id_A,gMotorVars[mtrNum].Id_A) + _IQmpy(gMotorVars[mtrNum].Iq_A,gMotorVars[mtrNum].Iq_A));

	// gets the Position Controller status
	gMotorVars[mtrNum].SpinTAC.PosCtlStatus = STPOSCTL_getStatus(st_obj[mtrNum].posCtlHandle);

	// get the inertia setting
	gMotorVars[mtrNum].SpinTAC.InertiaEstimate_Aperkrpm =
			_IQmpy(STPOSCTL_getInertia(st_obj[mtrNum].posCtlHandle),
					_IQ(((float_t)gUserParams[mtrNum].motor_numPolePairs / (0.001 * 60.0 * gUserParams[mtrNum].iqFullScaleFreq_Hz)) * gUserParams[mtrNum].iqFullScaleCurrent_A));

	// get the friction setting
	gMotorVars[mtrNum].SpinTAC.FrictionEstimate_Aperkrpm =
			_IQmpy(STPOSCTL_getFriction(st_obj[mtrNum].posCtlHandle),
					_IQ(((float_t)gUserParams[mtrNum].motor_numPolePairs / (0.001 * 60.0 * gUserParams[mtrNum].iqFullScaleFreq_Hz)) * gUserParams[mtrNum].iqFullScaleCurrent_A));

	// get the Position Controller error
	gMotorVars[mtrNum].SpinTAC.PosCtlErrorID = STPOSCTL_getErrorID(st_obj[mtrNum].posCtlHandle);

	// get the Position Move status
	//gMotorVars[mtrNum].SpinTAC.PosMoveStatus = STPOSMOVE_getStatus(st_obj[mtrNum].posMoveHandle);

	// get the Position Move profile time
	//STPOSMOVE_getProfileTime_tick(st_obj[mtrNum].posMoveHandle, &profile_mticks, &profile_ticks);
	//gMotorVars[mtrNum].SpinTAC.PosMoveTime_mticks = profile_mticks;
	//gMotorVars[mtrNum].SpinTAC.PosMoveTime_ticks = profile_ticks;

	// get the Position Move error
	//gMotorVars[mtrNum].SpinTAC.PosMoveErrorID = STPOSMOVE_getErrorID(st_obj[mtrNum].posMoveHandle);

	// get the Position Converter error
	gMotorVars[mtrNum].SpinTAC.PosConvErrorID = STPOSCONV_getErrorID(st_obj[mtrNum].posConvHandle);

	// get the estimator state
	gMotorVars[mtrNum].EstState = EST_getState(estHandle[mtrNum]);

	return;
} // end of updateGlobalVariables() function

void ST_runPosConv(ST_Handle handle, ENC_Handle encHandle, SLIP_Handle slipHandle, MATH_vec2 *Idq_pu,
		MOTOR_Type_e motorType) {
	ST_Obj *stObj = (ST_Obj *) handle;

	// get the electrical angle from the ENC module
	STPOSCONV_setElecAngle_erev(stObj->posConvHandle, ENC_getElecAngle(encHandle));

	if (motorType == MOTOR_Type_Induction) {
		// The CurrentVector feedback is only needed for ACIM
		// get the vector of the direct/quadrature current input vector values from CTRL
		STPOSCONV_setCurrentVector(stObj->posConvHandle, Idq_pu);
	}

	// run the SpinTAC Position Converter
	STPOSCONV_run(stObj->posConvHandle);

	if (motorType == MOTOR_Type_Induction) {
		// The Slip Velocity is only needed for ACIM
		// update the slip velocity in electrical angle per second, Q24
		SLIP_setSlipVelocity(slipHandle, STPOSCONV_getSlipVelocity(stObj->posConvHandle));
	}
}

/*_iq ST_runPosCtl(ST_Handle handle) {
	_iq iqReference;
	ST_Obj *stObj = (ST_Obj *) handle;

	// provide the updated references to the SpinTAC Position Control
	STPOSCTL_setPositionReference_mrev(stObj->posCtlHandle, STPOSMOVE_getPositionReference_mrev(stObj->posMoveHandle));
	STPOSCTL_setVelocityReference(stObj->posCtlHandle, STPOSMOVE_getVelocityReference(stObj->posMoveHandle));
	STPOSCTL_setAccelerationReference(stObj->posCtlHandle, STPOSMOVE_getAccelerationReference(stObj->posMoveHandle));
	// provide the feedback to the SpinTAC Position Control
	STPOSCTL_setPositionFeedback_mrev(stObj->posCtlHandle, STPOSCONV_getPosition_mrev(stObj->posConvHandle));

	// Run SpinTAC Position Control
	STPOSCTL_run(stObj->posCtlHandle);

	// Provide SpinTAC Position Control Torque Output to the FOC
	iqReference = STPOSCTL_getTorqueReference(stObj->posCtlHandle);

	return iqReference;
}*/

bool isMotorReady(HAL_MtrSelect_e mtrNum) {
	return gMotorVars[mtrNum].Flag_Run_Identify
			&& EST_getState(estHandle[mtrNum]) == EST_State_OnLine
			&& gMotorVars[mtrNum].Flag_enableAlignment == false;
}

bool isMotorActive(HAL_MtrSelect_e mtrNum) {
	return isMotorReady(mtrNum)
			&& STPOSCTL_getStatus(st_obj[mtrNum].posCtlHandle) == ST_CTL_BUSY;
}

_iq runPosCtl(HAL_MtrSelect_e mtrNum) {
	_iq iqReference;
	ST_Obj *stObj = (ST_Obj *) stHandle[mtrNum];
	PositionParams *positionParams = &positionParamsList[mtrNum];

	_iq20 normalizedTransitionPosRef = positionParams->transitionPosRef;

	while (normalizedTransitionPosRef > _IQ20(10.0)) {
		normalizedTransitionPosRef -= _IQ20(20.0);
	}

	while (normalizedTransitionPosRef < _IQ20(-10.0)) {
		normalizedTransitionPosRef += _IQ20(20.0);
	}

	// provide the updated references to the SpinTAC Position Control
	STPOSCTL_setPositionReference_mrev(stObj->posCtlHandle, _IQ20toIQ(normalizedTransitionPosRef));
	STPOSCTL_setVelocityReference(stObj->posCtlHandle, _IQ20mpy(positionParams->speedRef_rps, _IQ20(ST_SPEED_PU_PER_Hz)));
	STPOSCTL_setAccelerationReference(stObj->posCtlHandle, _IQ20mpy(positionParams->currentAcc_rpsps, _IQ20(ST_SPEED_PU_PER_Hz)));
	// provide the feedback to the SpinTAC Position Control
	STPOSCTL_setPositionFeedback_mrev(stObj->posCtlHandle, STPOSCONV_getPosition_mrev(stObj->posConvHandle));

	// Run SpinTAC Position Control
	STPOSCTL_run(stObj->posCtlHandle);

	// Provide SpinTAC Position Control Torque Output to the FOC
	iqReference = STPOSCTL_getTorqueReference(stObj->posCtlHandle);

	return iqReference;
}

void calcTransitionPosRef(HAL_MtrSelect_e mtrNum) {
	ST_Obj *stObj = (ST_Obj *) stHandle[mtrNum];
	PositionParams *params = &positionParamsList[mtrNum];

	params->prevSpeed_rps = params->speedRef_rps;

	_iq posMrev = stObj->pos.conv.Pos_mrev;

	if (!isMotorActive(mtrNum)) {
		params->posRef = _IQ20mpyI32(_IQ20(20.0), stObj->pos.conv.PosROCounts) + _IQtoIQ20(posMrev);
		params->transitionPosRef = params->posRef;
		params->speedRef_rps = _IQ20(0.0);
		params->currentAcc_rpsps = _IQ20(0.0);

		return;
	}

	if (params->maxSpeed_rps == _IQ20(0.0) && params->speedRef_rps == _IQ20(0.0) && _IQabs(posMrev - params->prevPosMrev) > _IQ(0.01)) {
		params->zeroSpeedMoveFailCount++;

		if (params->zeroSpeedMoveFailCount >= params->zeroSpeedMoveFailLimit) {
			// moving, but should not
			gMotorVars[mtrNum].Flag_Run_Identify = false;
			gMotorVars[mtrNum].Flag_enableAlignment = true;

			params->zeroSpeedMoveFailCount = 0;

			params->prevPosMrev = stObj->pos.conv.Pos_mrev;

			return;
		}
	}

	params->prevPosMrev = posMrev;

	if (params->transitionPosRef < params->posRef) {
		params->posDiff = params->posRef - params->transitionPosRef;

		params->requiredDeceleration_rpsps = _IQ20div(_IQ20mpy(params->speedRef_rps, params->speedRef_rps), _IQ20mpy(params->posDiff, _IQ20(2.0)));

		if (params->requiredDeceleration_rpsps > params->dec_rpsps) {
			params->speedRef_rps -= _IQ20mpy(params->requiredDeceleration_rpsps, params->posSampleTime_sec);

			if (params->speedRef_rps < params->minSpeed_rps) {
				params->speedRef_rps = params->minSpeed_rps;
			}
		} else if (params->speedRef_rps < params->maxSpeed_rps) {
			params->speedRef_rps += _IQ20mpy(params->acc_rpsps, params->posSampleTime_sec);

			if (params->speedRef_rps > params->maxSpeed_rps) {
				params->speedRef_rps = params->maxSpeed_rps;
			}
		} else if (params->speedRef_rps > params->maxSpeed_rps) {
			params->speedRef_rps -= _IQ20mpy(params->dec_rpsps, params->posSampleTime_sec);

			if (params->speedRef_rps < params->maxSpeed_rps) {
				params->speedRef_rps = params->maxSpeed_rps;
			}
		}

		params->transitionPosRef += _IQ20mpy(params->speedRef_rps, params->posSampleTime_sec);

		if (params->transitionPosRef > params->posRef) {
			params->transitionPosRef = params->posRef;
			params->speedRef_rps = _IQ20(0.0);
		}

	} else if (params->transitionPosRef > params->posRef) {
		params->posDiff = params->transitionPosRef - params->posRef;

		params->requiredDeceleration_rpsps = _IQ20div(_IQ20mpy(params->speedRef_rps, params->speedRef_rps), _IQ20mpy(params->posDiff, _IQ20(2.0)));

		if (params->requiredDeceleration_rpsps > params->dec_rpsps) {
			params->speedRef_rps += _IQ20mpy(params->requiredDeceleration_rpsps, params->posSampleTime_sec);

			if (params->speedRef_rps > -params->minSpeed_rps) {
				params->speedRef_rps = -params->minSpeed_rps;
			}
		} else if (params->speedRef_rps > -params->maxSpeed_rps) {
			params->speedRef_rps -= _IQ20mpy(params->acc_rpsps, params->posSampleTime_sec);

			if (params->speedRef_rps < -params->maxSpeed_rps) {
				params->speedRef_rps = -params->maxSpeed_rps;
			}
		} else if (params->speedRef_rps < -params->maxSpeed_rps) {
			params->speedRef_rps += _IQ20mpy(params->dec_rpsps, params->posSampleTime_sec);

			if (params->speedRef_rps > -params->maxSpeed_rps) {
				params->speedRef_rps = -params->maxSpeed_rps;
			}
		}

		params->transitionPosRef += _IQ20mpy(params->speedRef_rps, params->posSampleTime_sec);

		if (params->transitionPosRef < params->posRef) {
			params->transitionPosRef = params->posRef;
			params->speedRef_rps = _IQ20(0.0);
		}
	}

	params->currentAcc_rpsps = _IQ20mpy(_IQ20abs(params->speedRef_rps) - _IQ20abs(params->prevSpeed_rps), params->posSampleTime_sec);
}

void serialWrite(char *sendData, int length) {
	int i = 0;

	while (i < length) {
		//SCI_putDataNonBlocking(halHandle->sciBHandle, sendData[i]);
		//i++;

		if (SCI_txReady(halHandle->sciBHandle)) {
			SCI_write(halHandle->sciBHandle, sendData[i]);
			i++;
		}
	}
}

//@} //defgroup
// end of file

