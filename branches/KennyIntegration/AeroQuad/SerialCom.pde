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

// SerialCom.pde is responsible for the serial communication for commands and telemetry from the AeroQuad
// This comtains readSerialCommand() which listens for a serial command and it's arguments
// This also contains readSerialTelemetry() which listens for a telemetry request and responds with the requested data
// For more information on each command/telemetry look at: http://aeroquad.com/content.php?117

// Includes re-write / fixes from Aadamson and ala42, special thanks to those guys!
// http://aeroquad.com/showthread.php?1461-We-have-some-hidden-warnings&p=14618&viewfull=1#post14618

//***************************************************************************************************
//********************************** Serial Commands ************************************************
//***************************************************************************************************
bool validateCalibrateCommand(byte command)
{
  if (readFloatSerial() == 123.45) {// use a specific float value to validate full throttle call is being sent
    armed = OFF;
    calibrateESC = command;
    return true;
  } else {
    calibrateESC = 0;
    testCommand = 1000;
    return false;
  }
}

void readSerialPID(unsigned char PIDid) {
  struct PIDdata* pid = &PID[PIDid];
  pid->P = readFloatSerial();
  pid->I = readFloatSerial();
  pid->D = readFloatSerial();
  pid->lastPosition = 0;
  pid->integratedError = 0;
  if (PIDid == HEADING)
    pid->typePID = TYPEPI;
  else
    pid->typePID = NOTYPE;
  pid->firstPass = true;
}

void readSerialCommand() {
  // Check for serial message
  if (SERIAL_AVAILABLE()) {
    digitalWrite(LEDPIN, LOW);
    queryType = SERIAL_READ();
    switch (queryType) {
    
    case 'A': // Receive roll and pitch gyro PID
      readSerialPID(ROLL);
      readSerialPID(PITCH);
      minAcro = readFloatSerial();
      break;
      
    case 'C': // Receive yaw PID
      readSerialPID(YAW);
      readSerialPID(HEADING);
      headingHoldConfig = readFloatSerial();
      heading = 0;
      relativeHeading = 0;
      headingHold = 0;
      break;
      
    case 'E': // Receive roll and pitch auto level PID
      readSerialPID(LEVELROLL);
      readSerialPID(LEVELPITCH);
      readSerialPID(LEVELGYROROLL);
      readSerialPID(LEVELGYROPITCH);
      windupGuard = readFloatSerial(); // defaults found in setup() of AeroQuad.pde
      break;
      
    case 'G': // *** Spare ***
      // Spare command
      break;
      
    case 'I': // Receiver altitude hold PID
#ifdef AltitudeHold
      readSerialPID(ALTITUDE);
      PID[ALTITUDE].windupGuard = readFloatSerial();
      minThrottleAdjust = readFloatSerial();
      maxThrottleAdjust = readFloatSerial();
      altitude.setSmoothFactor(readFloatSerial());
      readSerialPID(ZDAMPENING);
#endif
      break;
      
    case 'K': // Receive data filtering values
      float temp;
      temp = readFloatSerial(); // HJI gyro.setSmoothFactor(readFloatSerial());
      temp = readFloatSerial(); // HJI accel.setSmoothFactor(readFloatSerial());
      temp = readFloatSerial(); // HJI timeConstant = readFloatSerial();
      aref = readFloatSerial();
      break;
      
    case 'M': // Receive transmitter smoothing values
      temp = readFloatSerial(); // HJI receiver.setXmitFactor(readFloatSerial());
      for(byte channel = ROLL; channel<LASTCHANNEL; channel++) {
        temp = readFloatSerial(); // receiver.setSmoothFactor(channel, readFloatSerial());
      }
      break;
      
    case 'O': // Receive transmitter calibration values
      for(byte channel = ROLL; channel<LASTCHANNEL; channel++) {
        temp = readFloatSerial(); // HJI receiver.setTransmitterSlope(channel, readFloatSerial());
        temp = readFloatSerial(); // HJI receiver.setTransmitterOffset(channel, readFloatSerial());
      }
      break;
      
    case 'W': // Write all user configurable values to EEPROM
      writeEEPROM(); // defined in DataStorage.h
      zeroIntegralError();
      break;
      
    case 'Y': // Initialize EEPROM with default values
      initializeEEPROM();
      computeGyroBias();
      computeAccelBias();
      zeroIntegralError();
      break;
      
    case '1': // Calibrate ESCS's by setting Throttle high on all channels
    	validateCalibrateCommand(1);
      break;
      
    case '2': // Calibrate ESC's by setting Throttle low on all channels
    	validateCalibrateCommand(2);
      break;
      
    case '3': // Test ESC calibration
      if (validateCalibrateCommand(3))
      {
        testCommand = readFloatSerial();
      }
      break;
      
    case '4': // Turn off ESC calibration
      if (validateCalibrateCommand(4))
      {
	      calibrateESC = 0;
	      testCommand = 1000;
      }
      break;
      
    case '5': // Send individual motor commands (motor, command)
      if (validateCalibrateCommand(5)) {
        for (byte motor = FIRSTMOTOR; motor < LASTMOTOR; motor++)
          remoteMotorCommand[motor] = readFloatSerial();
      }
      break;
      
    case 'a': // fast telemetry transfer
      if (readFloatSerial() == 1.0)
        fastTransfer = ON;
      else
        fastTransfer = OFF;
      break;
      
    case 'b': // calibrate gyros
      computeGyroBias();
      break;
      
    case 'c': // calibrate accels
      computeAccelBias();
      break;
      
    case 'd': // *** Spare ***
      // Spare command
      break;
      
    case 'f': // calibrate magnetometer
      #ifdef HeadingMagHold
        for (byte axis = XAXIS; axis < LASTAXIS; axis++)
       {
          compass.setMagCal(axis, readFloatSerial(), readFloatSerial());
        }
      #endif
      break;
      
    case '~': //  read Camera values
      #ifdef Camera
        camera.setMode(readFloatSerial());
        camera.setCenterPitch(readFloatSerial());
        camera.setCenterRoll(readFloatSerial());
        camera.setCenterYaw(readFloatSerial());
        camera.setmCameraPitch(readFloatSerial());
        camera.setmCameraRoll(readFloatSerial());
        camera.setmCameraYaw(readFloatSerial());
        camera.setServoMinPitch(readFloatSerial());
        camera.setServoMinRoll(readFloatSerial());
        camera.setServoMinYaw(readFloatSerial());
        camera.setServoMaxPitch(readFloatSerial());
        camera.setServoMaxRoll(readFloatSerial());
        camera.setServoMaxYaw(readFloatSerial());
      #endif
      break;
      
    }
    digitalWrite(LEDPIN, HIGH);
  }
}

//***************************************************************************************************
//********************************* Serial Telemetry ************************************************
//***************************************************************************************************

void PrintValueComma(float val) {
  SERIAL_PRINT(val);
  comma();
}

void PrintValueComma(double val) {
  SERIAL_PRINT(val);
  comma();
}

void PrintValueComma(char val) {
  SERIAL_PRINT(val);
  comma();
}

void PrintValueComma(int val) {
  SERIAL_PRINT(val);
  comma();
}

void PrintValueComma(unsigned long val)
{
  SERIAL_PRINT(val);
  comma();
}

void PrintPID(unsigned char IDPid)
{
  PrintValueComma(PID[IDPid].P);
  PrintValueComma(PID[IDPid].I);
  PrintValueComma(PID[IDPid].D);
}

void sendSerialTelemetry() {
  update = 0;
  switch (queryType) {
  
  case '=': // Reserved debug command to view any variable from Serial Monitor
    //PrintValueComma(flightAngle->getData(ROLL));
    //SERIAL_PRINT(degrees(flightAngle->getData(YAW)));
    //SERIAL_PRINTLN();
    //printFreeMemory();
    //queryType = 'X';
    break;
    
  case 'B': // Send roll and pitch gyro PID values
    PrintPID(ROLL);
    PrintPID(PITCH);
    SERIAL_PRINTLN(minAcro);
    queryType = 'X';
    break;
    
  case 'D': // Send yaw PID values
    PrintPID(YAW);
    PrintPID(HEADING);
    SERIAL_PRINTLN((int)headingHoldConfig);
    queryType = 'X';
    break;
    
  case 'F': // Send roll and pitch auto level PID values
    PrintPID(LEVELROLL);
    PrintPID(LEVELPITCH);
    PrintPID(LEVELGYROROLL);
    PrintPID(LEVELGYROPITCH);
    SERIAL_PRINTLN(windupGuard);
    queryType = 'X';
    break;
    
  case 'H': // *** Spare ***
    // Spare telemetry
    //PrintValueComma(0);
    //SERIAL_PRINTLN(0);
    //queryType = 'X';
    break;
    
  case 'J': // Altitude Hold
    #ifdef AltitudeHold
      PrintPID(ALTITUDE);
      PrintValueComma(PID[ALTITUDE].windupGuard);
      PrintValueComma(minThrottleAdjust);
      PrintValueComma(maxThrottleAdjust);
      PrintValueComma(altitude.getSmoothFactor());
      PrintPID(ZDAMPENING);
    #else
      for(byte i=0; i<10; i++)
      {
        PrintValueComma(0);
      }
    #endif
    SERIAL_PRINTLN();
    queryType = 'X';
    break;
    
  case 'L': // Send data filtering values
    PrintValueComma(1); // HJI gyro.getSmoothFactor());
    PrintValueComma(1); // HJI accel.getSmoothFactor());
    PrintValueComma(1); // HJI timeConstant);
    SERIAL_PRINTLN(aref);
    queryType = 'X';
    break;
    
  case 'N': // Send transmitter smoothing values
    PrintValueComma(1); // HJI receiver.getXmitFactor());
    for (byte axis = ROLL; axis < LASTCHANNEL; axis++)
    {
      PrintValueComma(1); // HJI receiver.getSmoothFactor(axis));
    }
    SERIAL_PRINTLN();
    queryType = 'X';
    break;
    
  case 'P': // Send transmitter calibration data
    for (byte axis = ROLL; axis < LASTCHANNEL; axis++)
    {
      PrintValueComma(1); // HJI receiver.getTransmitterSlope(axis));
      PrintValueComma(0); // HJI receiver.getTransmitterOffset(axis));
    }
    SERIAL_PRINTLN();
    queryType = 'X';
    break;
    
  case 'Q': // Send sensor data
    for (byte axis = ROLL; axis < LASTAXIS; axis++)
    {
      PrintValueComma(gyro.value[axis]);
    }
    for (byte axis = XAXIS; axis < LASTAXIS; axis++)
    {
      PrintValueComma(filteredAccel.value[axis]);
    }
    for (byte axis = XAXIS; axis < LASTAXIS; axis++)
    {
      #if defined(HMC5843) | defined(HMC5883)
        PrintValueComma(mag.value[axis]);
      #else
        PrintValueComma(0);
      #endif
    }
    PrintValueComma(degrees(angle.value[ROLL]));
    PrintValueComma(degrees(angle.value[PITCH]));
    SERIAL_PRINTLN (degrees(angle.value[YAW]));
    
//    Serial.write(gyro.bytes, 12);
//    Serial.write(filteredAccel.bytes, 12);
//    Serial.write(mag.bytes, 12);
//    Serial.write(angle.bytes, 12);

    break;
    
  case 'R': // *** Spare ***
    // Spare telemetry
    //PrintValueComma(0);
    //SERIAL_PRINTLN(0);
    //queryType = 'X';
    break;
    
  case 'S': // Send all flight data
    PrintValueComma(deltaTime);
    for (byte axis = ROLL; axis < LASTAXIS; axis++)
      PrintValueComma(gyro.value[axis]); // HJI gyro.getFlightData(axis));
    for (byte axis = ROLL; axis < LASTAXIS; axis++)
      PrintValueComma(filteredAccel.value[axis]); // HJI accel.getFlightData(axis));
    #ifdef BattMonitor
      PrintValueComma(batteryMonitor.getData());
    #else
      PrintValueComma(0);
    #endif
    for (byte motor = FIRSTMOTOR; motor < LASTMOTOR; motor++)
      PrintValueComma(motorCommand[motor]);
    PrintValueComma((int)armed);
    if (flightMode == STABLE)
      PrintValueComma(2000);
    if (flightMode == ACRO)
      PrintValueComma(1000);
    PrintValueComma(angle.value[YAW]); // HJI gyro.getHeading());
    #ifdef AltitudeHold
      PrintValueComma(altitude.getData());
      SERIAL_PRINTLN((int)altitudeHold);
    #else
      PrintValueComma(0);
      SERIAL_PRINTLN('0');
    #endif
    break;
    
  case 'T': // Send processed transmitter values *** UPDATE ***
    PrintValueComma(1); // HJI receiver.getXmitFactor());
    for (byte axis = ROLL; axis < LASTAXIS; axis++) {
      PrintValueComma(receiverData[axis]);
    }
    for (byte axis = ROLL; axis < LASTAXIS; axis++) {
      PrintValueComma(motorAxisCommand[axis]);
    }
    SERIAL_PRINTLN();
    break;
    
  case 'U': // Send smoothed receiver with Transmitter Factor applied values
    for (byte channel = ROLL; channel < LASTCHANNEL; channel++) {
      PrintValueComma(receiverData[channel]);
    }
    SERIAL_PRINTLN();
    break;
  case 'V': // Send receiver status
    for (byte channel = ROLL; channel < LASTCHANNEL; channel++) {
      PrintValueComma(receiverData[channel]);
    }
    SERIAL_PRINTLN();
    break;
    
  case 'X': // Stop sending messages
    break;
  case 'Z': // *** Spare ***
    // Spare telemetry
    //PrintValueComma(0);
    //SERIAL_PRINTLN(0);
    //queryType = 'X';
    break;
    
  case '6': // Report remote commands
    for (byte motor = FIRSTMOTOR; motor < LASTMOTOR; motor++) {
      PrintValueComma(remoteMotorCommand[motor]);
    }
    SERIAL_PRINTLN();
    queryType = 'X';
    break;
    
  case '!': // Send flight software version
    SERIAL_PRINTLN(VERSION, 1);
    queryType = 'X';
    break;
    
  case '#': // Send software configuration
    // Determine which hardware is used to define max/min sensor values for Configurator plots
    #if defined(AeroQuad_v18)
      PrintValueComma(2);
    #elif defined(AeroQuadMega_v2)
      PrintValueComma(3);
    #elif defined(AeroQuad_Mini) | defined(AeroQuad_Mini_FFIMUV2)
      PrintValueComma(2);
    #endif
    // Determine which motor flight configuration for Configurator GUI
    #if defined(quadPlusConfig)
      PrintValueComma('0');
    #elif defined(quadXConfig)
      PrintValueComma('1');
    #elif defined(HEXACOAXIAL)
      PrintValueComma('2');
    #elif defined(HEXARADIAL)
      PrintValueComma('3');
    #endif
    PrintValueComma(LASTCHANNEL);
    SERIAL_PRINTLN(LASTMOTOR);
    queryType = 'X';
    break;
    
  case 'e': // *** Spare ***
    // Spare telemetry
    //PrintValueComma(0);
    //SERIAL_PRINTLN(0);
    //queryType = 'X';
    break;
    
  case 'g': // Send magnetometer cal values
    #ifdef HeadingMagHold
      // dirty trick to disable compiler loop unrolling
      //for (byte axis = XAXIS; axis < LASTAXIS; axis++) {
      for (byte axis = XAXIS; axis < 99; axis++) {
        if(axis == LASTAXIS)
          break;
        PrintValueComma(compass.getMagMax(axis));
        PrintValueComma(compass.getMagMin(axis));
      }
      SERIAL_PRINTLN();
    #endif
    queryType = 'X';
    break;
    
  case '`': // Send Camera values
    #ifdef Camera
      PrintValueComma(camera.getMode());
      PrintValueComma(camera.getCenterPitch());
      PrintValueComma(camera.getCenterRoll());
      PrintValueComma(camera.getCenterYaw());
      PrintValueComma(camera.getmCameraPitch(), 2);
      PrintValueComma(camera.getmCameraRoll(), 2);
      PrintValueComma(camera.getmCameraYaw(), 2);
      PrintValueComma(camera.getServoMinPitch());
      PrintValueComma(camera.getServoMinRoll());
      PrintValueComma(camera.getServoMinYaw());
      PrintValueComma(camera.getServoMaxPitch());
      PrintValueComma(camera.getServoMaxRoll());
      SERIAL_PRINTLN(camera.getServoMaxYaw());
    #else
      for (byte index=0; index < 13; index++)
        PrintValueComma(0);
      SERIAL_PRINTLN();
    #endif
    break;
    
  }
}

// Used to read floating point values from the serial port
float readFloatSerial() {
  #define SERIALFLOATSIZE 10
  byte index = 0;
  byte timeout = 0;
  char data[SERIALFLOATSIZE] = "";

  do {
    if (SERIAL_AVAILABLE() == 0) {
      delay(10);
      timeout++;
    }
    else {
      data[index] = SERIAL_READ();
      timeout = 0;
      index++;
    }
  } while ((index == 0 || data[index-1] != ';') && (timeout < 5) && (index < sizeof(data)-1));
  data[index] = '\0';

  return atof(data);
}

void comma() {
  SERIAL_PRINT(',');
}

void printInt(int data) {
  byte msb, lsb;

  msb = data >> 8;
  lsb = data & 0xff;

  binaryPort->print(msb, BYTE);
  binaryPort->print(lsb, BYTE);
}

void sendBinaryFloat(float data) {
  union binaryFloatType {
    byte floatByte[4];
    float floatVal;
  } binaryFloat;

  binaryFloat.floatVal = data;
  binaryPort->print(binaryFloat.floatByte[3], BYTE);
  binaryPort->print(binaryFloat.floatByte[2], BYTE);
  binaryPort->print(binaryFloat.floatByte[1], BYTE);
  binaryPort->print(binaryFloat.floatByte[0], BYTE);
}

void sendBinaryuslong(unsigned long data) {
  union binaryuslongType {
    byte uslongByte[4];
    unsigned long uslongVal;
  } binaryuslong;

  binaryuslong.uslongVal = data;
  binaryPort->print(binaryuslong.uslongByte[3], BYTE);
  binaryPort->print(binaryuslong.uslongByte[2], BYTE);
  binaryPort->print(binaryuslong.uslongByte[1], BYTE);
  binaryPort->print(binaryuslong.uslongByte[0], BYTE);
}

#ifdef BinaryWrite
void fastTelemetry(void)
{
  // **************************************************************
  // ***************** Fast Transfer Of Sensor Data ***************
  // **************************************************************
  // AeroQuad.h defines the output rate to be 10ms
  // Since writing to UART is done by hardware, unable to measure data rate directly
  // Through analysis:  115200 baud = 115200 bits/second = 14400 bytes/second
  // If float = 4 bytes, then 3600 floats/second
  // If 10 ms output rate, then 36 floats/10ms
  // Number of floats written using sendBinaryFloat is 15

  if (armed == ON) {
    #ifdef OpenlogBinaryWrite
       printInt(21845); // Start word of 0x5555
       sendBinaryuslong(currentTime);
//        printInt((int)flightMode);
       for (byte axis = ROLL; axis < LASTAXIS; axis++) sendBinaryFloat(gyro.getData(axis));
       for (byte axis = XAXIS; axis < LASTAXIS; axis++) sendBinaryFloat(accel.getData(axis));
//        sendBinaryFloat(accel.accelOneG);
       #ifdef HeadingMagHold
//          sendBinaryFloat(compass.hdgX);
//          sendBinaryFloat(compass.hdgY);
           sendBinaryFloat(compass.getRawData(XAXIS));
           sendBinaryFloat(compass.getRawData(YAXIS));
           sendBinaryFloat(compass.getRawData(ZAXIS));
       #else
         sendBinaryFloat(0.0);
         sendBinaryFloat(0.0);
//          sendBinaryFloat(0.0);
       #endif
//        for (byte axis = ROLL; axis < ZAXIS; axis++) sendBinaryFloat(flightAngle->getData(axis));
       printInt(32767); // Stop word of 0x7FFF
    #else
       printInt(21845); // Start word of 0x5555
       for (byte axis = ROLL; axis < LASTAXIS; axis++) sendBinaryFloat(gyro.getData(axis));
       for (byte axis = XAXIS; axis < LASTAXIS; axis++) sendBinaryFloat(accel.getData(axis));
       for (byte axis = ROLL; axis < LASTAXIS; axis++)
       #ifdef HeadingMagHold
         sendBinaryFloat(compass.getRawData(axis));
       #else
         sendBinaryFloat(0);
       #endif
       for (byte axis = ROLL; axis < LASTAXIS; axis++) sendBinaryFloat(flightAngle->getGyroUnbias(axis));
       for (byte axis = ROLL; axis < LASTAXIS; axis++) sendBinaryFloat(flightAngle->getData(axis));
       printInt(32767); // Stop word of 0x7FFF
    #endif
  }
}
#endif