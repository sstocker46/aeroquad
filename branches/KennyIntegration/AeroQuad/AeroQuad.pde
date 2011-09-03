/*
  AeroQuad v2.5 Beta 1 - July 2011
  www.AeroQuad.com 
  Copyright (c) 2011 Ted Carancho.  All rights reserved.
  An Open Source Arduino based multicopter.
 
  This program is free software: you can redistribute it and/or modify 
  it under the terms of the GNU General Public License as published by 
  the Free Software Foundation, either version 3 of the License, or 
  (at your option) any later version. 

  This program is distributed in the hope that it will be useful, 
  but WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
  GNU General Public License for more details. 

  You should have received a copy of the GNU General Public License 
  along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

/****************************************************************************
   Before flight, select the different user options for your AeroQuad below
   If you need additional assitance go to http://AeroQuad.com/forum
*****************************************************************************/

/****************************************************************************
 ************************* Hardware Configuration ***************************
 ****************************************************************************/
// Select which hardware you wish to use with the AeroQuad Flight Software

//#define AeroQuad_Mini_FFIMUV2  // AeroQuad Mini Shield, Arduino Pro Mini, FFIMUv2
#define AeroQuad_v18           // Arduino 2009 with AeroQuad Shield v1.8 or greater
//#define AeroQuad_Mini          // Arduino Pro Mini with AeroQuad Mini Shield v1.0
//#define AeroQuadMega_v2        // Arduino Mega with AeroQuad Shield v2.x

/****************************************************************************
 *********************** Define Flight Configuration ************************
 ****************************************************************************/
// Use only one of the following definitions
//#define quadPlusConfig
#define quadXConfig
//#define y4Config
//#define hexPlusConfig
//#define hexXConfig
//#define y6Config

// *******************************************************************************************************************************
// Define how many channels that are connected from your R/C receiver
// Please note that the flight software currently only supports 6 channels, additional channels will be supported in the future
// Additionally 8 receiver channels are only available when not using the Arduino Uno
// *******************************************************************************************************************************
#define LASTCHANNEL 6
//#define LASTCHANNEL 8

#include <EEPROM.h>

#include <TwiMaster.h>

#include "AeroQuad.h"
#include "PID.h"
#include "AQMath.h"

// Create objects defined from Configuration Section above

#ifdef AeroQuad_Mini_FFIMUV2
  #include <BMA180.h>
  //#include "BMP085.h"
  #include <ITG3200.h>
  #include <HMC5883.h>
  #include <Motors_PWMtimer_328_Gyro.h>
  //#include <Motors_I2C.h>
  #include <RX_PCINT_328_Gyro.h>
#endif

#ifdef AeroQuad_v18
  #include <BMA180.h>
  #include <ITG3200.h>
  #include <Motors_PWMtimer_328.h>
  //#include <Motors_I2C.h>
  #include <RX_PCINT_328.h>
#endif

#ifdef AeroQuad_Mini
  #include <ADXL345.h>
  #include <ITG3200.h>
  #include <Motors_PWMtimer_328_Gyro.h>
  //#include <Motors_I2C.h>
  #include <RX_PCINT_328_Gyro.h>
#endif

#ifdef AeroQuadMega_v2
  #include <BMA180.h>
  #include <BMP085.h>
  #include <ITG3200.h>
  #include <HMC5843.h>
  #include <Motors_PWMtimer_Mega.h>
  //#include <Motors_I2C.h>
  #include <RX_PCINT_Mega.h>
#endif

// Include this last as it contains objects from above declarations
#include "DataStorage.h"

//////////////////////////////////

float verticalVelocity;
float pressureAltitude;

union {float value[3];
        byte bytes[12];} angle;
        
union {float value[3];
        byte bytes[12];} earthAccel;

//////////////////////////////////

unsigned int isrFrameCounter = 1;
unsigned int thisIsrFrame = 0;

//#if defined (__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  byte timer0countIndex;
//#endif

//////////////////////////////////

// ************************************************************
// ********************** Setup AeroQuad **********************
// ************************************************************
void setup() {
  SERIAL_BEGIN(BAUD);
  Serial.println();

  TwiMaster_init(false);         // Internal Pull Ups disabled
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, LOW);

  #if defined(AeroQuad_v18) || defined(AeroQuadMega_v2) || defined(AeroQuad_Mini)
    pinMode(LED2PIN, OUTPUT);
    digitalWrite(LED2PIN, LOW);
    pinMode(LED3PIN, OUTPUT);
    digitalWrite(LED3PIN, LOW);
  #endif
  
  // Read user values from EEPROM
  readEEPROM(); // defined in DataStorage.h
  
  // Configure motors
  initializeMotors();

  // Setup receiver pins for pin change interrupts
  initializeReceiver();
       
  // Initialize sensors
  initializeGyro();
  initializeAccel();
  
  #if defined(HMC5843) | defined(HMC5883)
    initializeCompass();
  #endif
  
  #if defined(BMP085)
    initializePressure();
  #endif 

  zeroIntegralError();

  initializeAHRS();

  // Integral Limit for attitude mode
  // This overrides default set in readEEPROM()
  // Set for 1/2 max attitude command (+/-0.75 radians)
  // Rate integral not used for now
  PID[LEVELROLL].windupGuard = 0.375;
  PID[LEVELPITCH].windupGuard = 0.375;

  // Optional Sensors
  #ifdef AltitudeHold
    altitude.initialize();
  #endif
  
  // Battery Monitor
  #ifdef BattMonitor
    batteryMonitor.initialize();
  #endif
  
  #if defined(BinaryWrite) || defined(BinaryWritePID)
    #ifdef OpenlogBinaryWrite
      binaryPort = &Serial1;
      binaryPort->begin(115200);
      delay(1000);
    #else
     binaryPort = &Serial;
    #endif 
  #endif
  
  previousTime = micros();
  digitalWrite(LEDPIN, HIGH);
  safetyCheck = 0;

  setupFourthOrder();
  
  ////////////////////////////////////////////////////////////////////////////////
  
//  #if defined (__AVR_ATmega328P__)
//    EICRA = 0x03;  // Set INT0 interrupt request on rising edge
//    EIMSK = 0x01;  // Enable External Interrupt 0
//  #endif
//  
//  #if defined (__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    TCCR0A = 0x00;                    // Normal port operation, OC0A, OC0B disconnected
  
    timer0countIndex = 0;
    OCR0A = TCNT0 + TIMER0_COUNT0;
    
    TIMSK0 |= (1<<OCIE0A);            // Enable Timer/Counter0 Output Compare A Match Interrupt
//  #endif
  
  //////////////////////////////////////////////////////////////////////////////// 
}

void loop () {
  if (((isrFrameCounter % BACKGROUND_COUNT) == 0) && (isrFrameCounter != thisIsrFrame))
  {
//    long t1 = micros();
    
    thisIsrFrame = isrFrameCounter;
    
    cli();
    for (byte i = 0; i < 3; i++)
    {
      accelSummedSamples[i] = accelSum[i];
      accelSum[i] = 0;
      gyroSummedSamples[i] = gyroSum[i];
      gyroSum[i] = 0;
    }
    #if defined(HMC5843) | defined(HMC5883)
      if (newMagData == 1)
      {
        for (byte i = 0; i < 3; i++) rawMagTemporary[i] = rawMag.value[i];
      }
    #endif
    #if defined(BMP085)
      if ((thisIsrFrame % ALTITUDE_COUNT) == 0 )
      {
        uncompensatedPressureSummedSamples = uncompensatedPressureSum;
        uncompensatedPressureSum = 0;
      } 
    #endif      
    sei();
    
    accel.value[XAXIS] = (float(accelSummedSamples[XAXIS])/SUM_COUNT) * accelScaleFactor[XAXIS] + runTimeAccelBias[XAXIS];
    accel.value[YAXIS] = (float(accelSummedSamples[YAXIS])/SUM_COUNT) * accelScaleFactor[YAXIS] + runTimeAccelBias[YAXIS];
    accel.value[ZAXIS] = (float(accelSummedSamples[ZAXIS])/SUM_COUNT) * accelScaleFactor[ZAXIS] + runTimeAccelBias[ZAXIS]; 
      
    gyro.value[ROLL]  = (float(gyroSummedSamples[ROLL])/SUM_COUNT - runTimeGyroBias[ROLL])   * gyroScaleFactor;
    gyro.value[PITCH] = (runTimeGyroBias[PITCH] - float(gyroSummedSamples[PITCH])/SUM_COUNT) * gyroScaleFactor;
    gyro.value[YAW]   = (runTimeGyroBias[YAW]   - float(gyroSummedSamples[YAW])/SUM_COUNT)   * gyroScaleFactor;
    
    filteredAccel.value[XAXIS] = computeFourthOrder(accel.value[XAXIS], &fourthOrder[AX_FILTER]);
    filteredAccel.value[YAXIS] = computeFourthOrder(accel.value[YAXIS], &fourthOrder[AY_FILTER]);
    filteredAccel.value[ZAXIS] = computeFourthOrder(accel.value[ZAXIS], &fourthOrder[AZ_FILTER]);
    
    #if defined(HMC5843) | defined(HMC5883)
      if (newMagData == 1)
      {
        mag.value[XAXIS] = float(rawMagTemporary[XAXIS]) + magBias[XAXIS];
        mag.value[YAXIS] = float(rawMagTemporary[YAXIS]) + magBias[YAXIS];
        mag.value[ZAXIS] = float(rawMagTemporary[ZAXIS]) + magBias[ZAXIS];
        newMagData = 0;
      }
    #endif
    
    #if defined(BMP085)
      if ((thisIsrFrame % ALTITUDE_COUNT) == 0)
      {
        if (thisIsrFrame == ALTITUDE_COUNT)
        {
          uncompensatedPressureAverage = uncompensatedPressureSummedSamples / (SUM_COUNT-1);
          calculateTemperature();
        }
        else
        {
          uncompensatedPressureAverage = uncompensatedPressureSummedSamples / SUM_COUNT;
        } 
        pressureAltitude = calculatePressure();
      }
    #endif      
      
    if (thisIsrFrame % RECEIVER_COUNT == 0)
    {
      readPilotCommands();
    }
      
    AHRSupdate();
    
    flightControl();
    
    if (thisIsrFrame % SERIAL_COM_COUNT == 0)
    {
      readSerialCommand();
      sendSerialTelemetry();
    }

//    Serial.println(micros()-t1);
  }
}

//#if defined (__AVR_ATmega328P__)
//  ////////////////////////////////////////////////////////////////////////////////
//  // Interrupt Service Routine - INT0
//  ////////////////////////////////////////////////////////////////////////////////
//  ISR(INT0_vect, ISR_NOBLOCK)
//  {  
//    readAccel();
//    
//    readGyro();
//    
//    #if defined(HMC5843) | defined(HMC5883)
//      if ((isrFrameCounter % COMPASS_COUNT) == 0) {
//        readCompass();
//        newMagData = 1;
//      }
//    #endif
//    
//    #if defined(BMP085)
//      if (((isrFrameCounter + 1) % PRESSURE_COUNT) == 0)
//      {
//        if (isrFrameCounter == (PRESSURE_COUNT-1))  readTemperatureRequestPressure();
//        else if (isrFrameCounter == (ISR_FRAME_COUNT-1)) readPressureRequestTemperature();
//        else readPressureRequestPressure();
//      }
//    #endif
//    
//    isrFrameCounter++;
//    if (isrFrameCounter > ISR_FRAME_COUNT) isrFrameCounter = 1;
//    
//    #if defined(I2C_ESC)
//      // If using I2C ESCs, check for updated motor commands and
//      //   write them out if present.
//      if (sendMotorCommands == 1)
//      {
//        for (byte motor = FIRSTMOTOR; motor < LASTMOTOR; motor++)
//        {
//          twiMaster.start(MOTORBASE + motor | I2C_WRITE);
//          twiMaster.write(motorCommandI2C[motor]);
//        }
//        sendMotorCommands == 0;
//      }
//    #endif
//  }
//#endif

//#if defined (__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  ////////////////////////////////////////////////////////////////////////////////
  // Interrupt Service Routine - TIMER0_COMPA
  //   Due to the limitations of the 8 bit counter, this ISR fires twice
  //   for every sensor read.  Nothing of value is executed on the first fire,
  //   the counter is simply reloaded.  On the second fire, the sensors are 
  //   processed.  For the cost of this little overhead, we don't have to use any 
  //   of the other timing resources to generate the interrupt.  
  ////////////////////////////////////////////////////////////////////////////////
  ISR(TIMER0_COMPA_vect, ISR_NOBLOCK)
  {   
    // Check which count has expired.  If count 1, just reload timer and return.
    // If count 2, reload timer and execute senor reads as required.  Then return.
    if (timer0countIndex == 0)        // If 1st count complete
    {
      OCR0A = TCNT0 + TIMER0_COUNT1;  // Load 2nd count
      timer0countIndex = 1;           // Indicate 2nd count active
      return;                         // And return from ISR
    }
    else {                            // Else 2nd count complete
      OCR0A = TCNT0 + TIMER0_COUNT0;  // Load 1st count
      timer0countIndex = 0;           // Indicate 1st count active
                                      // And execute sensor reads
      readAccel();
      
      readGyro();
      
      #if defined(HMC5843) | defined(HMC5883)
        if ((isrFrameCounter % COMPASS_COUNT) == 0) {
          readCompass();
          newMagData = 1;
        }
      #endif
      
      #if defined(BMP085)
        if (((isrFrameCounter + 1) % PRESSURE_COUNT) == 0) {
          if (isrFrameCounter == (PRESSURE_COUNT-1))  readTemperatureRequestPressure();
          else if (isrFrameCounter == (ISR_FRAME_COUNT-1)) readPressureRequestTemperature();
          else readPressureRequestPressure();
        }
      #endif
      
      isrFrameCounter++;
      if (isrFrameCounter > ISR_FRAME_COUNT) isrFrameCounter = 1;
      
      #if defined(I2C_ESC)
        // If using I2C ESCs, check for updated motor commands and
        //   write them out if present.
        if (sendMotorCommands == 1)
        {
          for (byte motor = FIRSTMOTOR; motor < LASTMOTOR; motor++)
          {
            twiMaster.start(MOTORBASE + motor | I2C_WRITE);
            twiMaster.write(motorCommandI2C[motor]);
          }
          sendMotorCommands == 0;
        }
      #endif
    }
  }
//#endif
