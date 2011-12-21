/*
  AeroQuad v3.0 - May 2011
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

#ifndef _AEROQUAD_GYROSCOPE_ITG3200_9DOF_H_
#define _AEROQUAD_GYROSCOPE_ITG3200_9DOF_H_

#include <Gyroscope.h>
#include <SensorsStatus.h>

#define ITG3200_ADDRESS					0x68

int gyroAddress = ITG3200_ADDRESS;
  
void initializeGyro() {

  if (readWhoI2C(gyroAddress) != gyroAddress+1) {
	sensorsState |= GYRO_BIT_STATE;
  }
	
  gyroScaleFactor = radians(1.0 / 14.375);    //  ITG3200 14.375 LSBs per �/sec
  updateRegisterI2C(gyroAddress, 0x3E, 0x80); // send a reset to the device
  updateRegisterI2C(gyroAddress, 0x16, 0x1D); // 10Hz low pass filter
  updateRegisterI2C(gyroAddress, 0x3E, 0x01); // use internal oscillator 
}
    
void measureGyro() {
  sendByteI2C(gyroAddress, 0x1D);
  Wire.requestFrom(gyroAddress, 6);
    
  // The following 3 lines read the gyro and assign it's data to gyroADC
  // in the correct order and phase to suit the standard shield installation
  // orientation.  See TBD for details.  If your shield is not installed in this
  // orientation, this is where you make the changes.
  int gyroADC[3];
  gyroADC[YAXIS] = ((Wire.read() << 8) | Wire.read()) - gyroZero[YAXIS];
  gyroADC[XAXIS] = ((Wire.read() << 8) | Wire.read()) - gyroZero[XAXIS];
  gyroADC[ZAXIS] = gyroZero[ZAXIS] - ((Wire.read() << 8) | Wire.read());

  for (byte axis = 0; axis <= ZAXIS; axis++) {
    gyroRate[axis] = filterSmooth(gyroADC[axis] * gyroScaleFactor, gyroRate[axis], gyroSmoothFactor);
  }
 
  // Measure gyro heading
  long int currentTime = micros();
  if (gyroRate[ZAXIS] > radians(1.0) || gyroRate[ZAXIS] < radians(-1.0)) {
    gyroHeading += gyroRate[ZAXIS] * ((currentTime - gyroLastMesuredTime) / 1000000.0);
  }
  gyroLastMesuredTime = currentTime;
}

void measureGyroSum() {
  sendByteI2C(gyroAddress, 0x1D);
  Wire.requestFrom(gyroAddress, 6);
  
  gyroSample[YAXIS] = ((Wire.read() << 8) | Wire.read());
  gyroSample[XAXIS] = ((Wire.read() << 8) | Wire.read());
  gyroSample[ZAXIS] = ((Wire.read() << 8) | Wire.read());

  gyroSampleCount++;
}

void evaluateGyroRate() {
  int gyroADC[3];
  gyroADC[XAXIS] = (gyroSample[XAXIS] / gyroSampleCount)  - gyroZero[XAXIS];
  gyroADC[YAXIS] = (gyroSample[YAXIS] / gyroSampleCount)  - gyroZero[YAXIS];
  gyroADC[ZAXIS] = gyroZero[ZAXIS] -   (gyroSample[ZAXIS]   / gyroSampleCount);
  gyroSample[0] = 0.0;
  gyroSample[1] = 0.0;
  gyroSample[2] = 0.0;
  gyroSampleCount = 0;

  for (byte axis = 0; axis <= ZAXIS; axis++) {
    gyroRate[axis] = filterSmooth(gyroADC[axis] * gyroScaleFactor, gyroRate[axis], gyroSmoothFactor);
  }
 
  // Measure gyro heading
  long int currentTime = micros();
  if (gyroRate[ZAXIS] > radians(1.0) || gyroRate[ZAXIS] < radians(-1.0)) {
    gyroHeading += gyroRate[ZAXIS] * ((currentTime - gyroLastMesuredTime) / 1000000.0);
  }
  gyroLastMesuredTime = currentTime;
}



void calibrateGyro() {
  int findZero[FINDZERO];
  for (byte calAxis = XAXIS; calAxis <= ZAXIS; calAxis++) {
    for (int i=0; i<FINDZERO; i++) {
      sendByteI2C(gyroAddress, (calAxis * 2) + 0x1D);
      findZero[i] = readWordI2C(gyroAddress);
      delay(10);
    }
    if (calAxis == XAXIS) {
      gyroZero[YAXIS] = findMedianInt(findZero, FINDZERO);
	  Serial.print("Y = ");Serial.print(gyroZero[YAXIS]);Serial.print(",");
	}
    else if (calAxis == YAXIS) {
      gyroZero[XAXIS] = findMedianInt(findZero, FINDZERO);
	  Serial.print("X = ");Serial.print(gyroZero[YAXIS]);Serial.print(",");
	}
    else {
      gyroZero[ZAXIS] = findMedianInt(findZero, FINDZERO);
	  Serial.print("Z = ");Serial.print(gyroZero[YAXIS]);Serial.print(",");
	}
  }
}

#endif