#include <WProgram.h>
#include <APM_ADC.h>
#include <Wire.h>

#include "LanguageExtensions.h"
#include "IMU.h"

#include "SerialComs.h"
#include "GPS.h"
#include "Sensors.h"
#include "motors.h"


IMUHardware::IMUHardware(const unsigned int inputCount) : AnalogInHardwareComponent(inputCount)
{
}

void IMUHardware::initialize()
{
	AnalogInHardwareComponent::initialize();
}	
		
void IMUHardware::process(const unsigned long currentTime)
{
	AnalogInHardwareComponent::process(currentTime);
}

const float* IMUHardware::currentReadings()
{
	return _lastReadings;
}
		
		
#define ZEROLEVELSAMPLECOUNT 50
void IMUHardware::calibrateZero()
{
	int zeroLevelSamples[ZEROLEVELSAMPLECOUNT];
	float dacMvValue = this->referenceVoltage() / (pow(2,this->adcPrecision()) - 1);

	//Calibrate each axis of gyro (Because they can drift over time)
	for (int i = IMUGyroX; i <= IMUGyroZ; i++)
	{
		if (_inputConfigurations[i].inputInUse)
		{
			//Sample the analog reading a bunch of times
			for (int j = 0; j < ZEROLEVELSAMPLECOUNT; j++) 
			{
				zeroLevelSamples[j] = this->readRawValue(_inputConfigurations[i].hardwarePin);
				delay(10);	//Esure that some more readings can be taken.						
			}
			
			//find the mode of the sampled data
			int sampleMode = findMode(zeroLevelSamples, ZEROLEVELSAMPLECOUNT);
			
			//convert the mode value to mV and assign it to the related _inputConfigurations structre for this input
			_inputConfigurations[i].zeroLevel = (float)sampleMode * dacMvValue;
		}
	}
}





OilpanIMU::OilpanIMU() : IMUHardware(6/*number of inputs that this device has*/)
{	
	this->setReferenceVoltage(3300);
	this->setAdcPrecision(12);
}

void OilpanIMU::initialize()
{
	IMUHardware::initialize();
	
	//Setup sensor details
	//X,Y,Z Gyros
	
	//With the settings below gyros give us deg/s
	_inputConfigurations[IMUGyroX].zeroLevel = 1370;			//in mv
	_inputConfigurations[IMUGyroX].sensitivity = 2.2;				//in mV/deg/s
	_inputConfigurations[IMUGyroX].inputInUse = true; 
	_inputConfigurations[IMUGyroX].hardwarePin = 2;	
	_inputConfigurations[IMUGyroX].invert = true;		
				
	_inputConfigurations[IMUGyroY].zeroLevel = 1370;			//in mv
	_inputConfigurations[IMUGyroY].sensitivity = 2.2;				//in mV/deg/s
	_inputConfigurations[IMUGyroY].inputInUse = true; 
	_inputConfigurations[IMUGyroY].hardwarePin = 1;			
	
	_inputConfigurations[IMUGyroZ].zeroLevel = 1370;			//in mv
	_inputConfigurations[IMUGyroZ].sensitivity = 2.2;				//in mV/deg/s
	_inputConfigurations[IMUGyroZ].inputInUse = true; 
	_inputConfigurations[IMUGyroZ].hardwarePin = 0;	
	_inputConfigurations[IMUGyroZ].invert = true;		
	
	
	//X,Y,Z Accels
	//With the settings below accels give us G
	_inputConfigurations[IMUAcclX].zeroLevel = 1650;			//in mv
 	_inputConfigurations[IMUAcclX].sensitivity = 310;				//in mv/G
	_inputConfigurations[IMUAcclX].inputInUse = true; 
	_inputConfigurations[IMUAcclX].hardwarePin = 4;
	_inputConfigurations[IMUAcclX].invert = true;

	_inputConfigurations[IMUAcclY].zeroLevel = 1650;			//in mv
 	_inputConfigurations[IMUAcclY].sensitivity = 310;				//in mv/G
	_inputConfigurations[IMUAcclY].inputInUse = true; 
	_inputConfigurations[IMUAcclY].hardwarePin = 5;			

	_inputConfigurations[IMUAcclZ].zeroLevel = 1650;			//in mv
 	_inputConfigurations[IMUAcclZ].sensitivity = 310;				//in mv/G
	_inputConfigurations[IMUAcclZ].inputInUse = true; 
	_inputConfigurations[IMUAcclZ].hardwarePin = 6;
} 		

const int OilpanIMU::readRawValue(const unsigned int channel)
{			
	return APM_ADC.Ch(channel);
}






SuplimentalYawSource::SuplimentalYawSource() : HardwareComponent()
{
	_lastReadings[0] = _lastReadings[1] = _lastReadings[2] = 0;
}

void SuplimentalYawSource::initialize()
{
	HardwareComponent::initialize();
}	

void SuplimentalYawSource::process(const unsigned long currentTime)
{
	HardwareComponent::process(currentTime);	
}

const int* SuplimentalYawSource::currentReadings()
{
	return _lastReadings;
}





#define HMC5843COMPAS_I2C_ADDRESS 0x1E
void HMC5843Compass::initialize()
{
	SuplimentalYawSource::initialize();

	//Set the compass to continous read mode
	Wire.begin();
	Wire.beginTransmission(HMC5843COMPAS_I2C_ADDRESS);
	Wire.send(0x02);
	Wire.send(0x00);         //Set to continous streaming mode
	Wire.endTransmission();  

	delay(5);                //HMC5843 needs some ms before communication
}

void HMC5843Compass::process(const unsigned long currentTime)
{	
	SuplimentalYawSource::process(currentTime);

	Wire.beginTransmission(HMC5843COMPAS_I2C_ADDRESS);
	Wire.send(0x03);        //sends address to read from
	Wire.endTransmission(); //end transmission

	Wire.requestFrom(HMC5843COMPAS_I2C_ADDRESS, 6);    // request 6 bytes from device
	
	while(!Wire.available());  // wait
	
	byte xAxisReadingHigh = Wire.receive();
	byte xAxisReadingLow = Wire.receive();
	int xAxisReading = (((xAxisReadingHigh) << 8) | (xAxisReadingLow));

	byte yAxisReadingHigh = Wire.receive();
	byte yAxisReadingLow = Wire.receive();
	int yAxisReading = (((yAxisReadingHigh) << 8) | (yAxisReadingLow));

	byte zAxisReadingHigh = Wire.receive();
	byte zAxisReadingLow = Wire.receive();
	int zAxisReading = (((zAxisReadingHigh) << 8) | (zAxisReadingLow));

	Wire.endTransmission();

	//The APM and DIYDrones HMC5843 board combination need things swapped around to because of mounting differences
	_lastReadings[0] = -yAxisReading;
	_lastReadings[1] = -xAxisReading;
	_lastReadings[2] = -zAxisReading;
}



	
	
const float IMUFilter::_correctYawReading(const float rollAngle, const float pitchAngle)
{			
	//http://www.ssec.honeywell.com/magnetic/datasheets/lowcost.pdf
	float cos_roll = cos(rollAngle);
	float sin_roll = sin(rollAngle);
	float cos_pitch = cos(pitchAngle);
	float sin_pitch = sin(pitchAngle);
	
	// Tilt compensated Magnetic field X
	float compensatedX = (_suplimentalRaw[0]*cos_pitch) + (_suplimentalRaw[1]*sin_roll*sin_pitch) + (_suplimentalRaw[2]*cos_roll*sin_pitch);
	// Tilt compensated Magnetic field Y
	float compensatedY = (_suplimentalRaw[1]*cos_roll) - (_suplimentalRaw[2]*sin_roll);

	// Compensated Magnetic Heading
	return atan2(-compensatedY,compensatedX);
}

void IMUFilter::_calculateRawAngles(float *rollAngle, float *pitchAngle, float* yawAngle)
{
	float gravityVector = sqrt(sq(_accelRaw[IMUFilterAxisX]) + sq(_accelRaw[IMUFilterAxisY]) + sq(_accelRaw[IMUFilterAxisZ]));
	
	//Pitch is defined as moving AROUND the X axis
	if (pitchAngle)
	{
		*pitchAngle = acos(_accelRaw[IMUFilterAxisX]/gravityVector) - 1.57079633;				
	}
	
	//Roll is defined as moving AROUND the Y axis
	if (rollAngle)
	{
		*rollAngle = -acos(_accelRaw[IMUFilterAxisY]/gravityVector) + 1.57079633;								
	}
	
	//YAW is defined as moving AROUND the Z axis
	if (yawAngle)
	{
		*yawAngle = _correctYawReading(*rollAngle, *pitchAngle);
	}
}


IMUFilter::IMUFilter()
{
	_lastProcessTime = 0;
	_deltaTime = 0;
	
	_firstPass = true;
	
	_suplimentalRaw[IMUFilterAxisX] = _suplimentalRaw[IMUFilterAxisY] = _suplimentalRaw[IMUFilterAxisZ] = 0;
	
	_processedRateInRadians[IMUFilterRoll] = _processedRateInRadians[IMUFilterPitch] = _processedRateInRadians[IMUFilterYaw] = 0;			
	_processedRateInDegrees[IMUFilterRoll] = _processedRateInDegrees[IMUFilterPitch] = _processedRateInDegrees[IMUFilterYaw] = 0;			
	
	_processedAngleInDegrees[IMUFilterRoll] = _processedAngleInDegrees[IMUFilterPitch] = _processedAngleInDegrees[IMUFilterYaw] = 0;
	_processedAngleInRadians[IMUFilterRoll] = _processedAngleInRadians[IMUFilterPitch] = _processedAngleInRadians[IMUFilterYaw] = 0;						
}

void IMUFilter::initialize()
{
	
}

void IMUFilter::filter(const unsigned long currentTime)
{
	_deltaTime = currentTime - _lastProcessTime;
	_lastProcessTime = currentTime;
	
	//Copy over the gyro rates.  If the subclassed filters overwrite then so be it, but this is the default			
	
	//Roll is defined as moving AROUND the Y axis
	
	_processedRateInDegrees[IMUFilterRoll] = _gyroRaw[IMUFilterAxisY];
	
	//Pitch is defined as moving AROUND the X axis
	_processedRateInDegrees[IMUFilterPitch] = _gyroRaw[IMUFilterAxisX];
	
	//YAW is defined as moving AROUND the Z axis
	_processedRateInDegrees[IMUFilterYaw] = _gyroRaw[IMUFilterAxisZ];
	
	//Gyros are in deg/s, so we need to switch to rad/s
	_processedRateInRadians[IMUFilterRoll] = ToRad(_processedRateInDegrees[IMUFilterRoll]);
	_processedRateInRadians[IMUFilterPitch] = ToRad(_processedRateInDegrees[IMUFilterPitch]);
	_processedRateInRadians[IMUFilterYaw] = ToRad(_processedRateInDegrees[IMUFilterYaw]);					
}

void IMUFilter::setCurrentReadings(const float *currentReadings)
{
	//Readings are expected in the following order (6 floats)
	//XGyro, YGyro, ZGyro, xAccel, yAccel, zAccel
		
	_gyroRaw[IMUFilterAxisX] = currentReadings[0];	//X
	_gyroRaw[IMUFilterAxisY] = currentReadings[1];	//Y
	_gyroRaw[IMUFilterAxisZ] = currentReadings[2];	//Z
		
	_accelRaw[IMUFilterAxisX] = currentReadings[3];	//X
	_accelRaw[IMUFilterAxisY] = currentReadings[4];	//Y
	_accelRaw[IMUFilterAxisZ] = currentReadings[5];	//Z
}

void IMUFilter::setSuplimentalYawReadings (const int *currentReadings)
{
	_suplimentalRaw[IMUFilterAxisX] = currentReadings[0];
	_suplimentalRaw[IMUFilterAxisY] = currentReadings[1];
	_suplimentalRaw[IMUFilterAxisZ] = currentReadings[2];
}

//Accessors for the processed values
const float IMUFilter::rollRateInDegrees()
{
	return _processedRateInDegrees[IMUFilterRoll];
}

const float IMUFilter::pitchRateInDegrees()
{
	return _processedRateInDegrees[IMUFilterPitch];
}

const float IMUFilter::yawRateInDegrees()
{
	return _processedRateInDegrees[IMUFilterYaw];
}


const float IMUFilter::rollRateInRadians()
{
	return _processedRateInRadians[IMUFilterRoll];
}

const float IMUFilter::pitchRateInRadians()
{
	return _processedRateInRadians[IMUFilterPitch];
}

const float IMUFilter::yawRateInRadians()
{
	return _processedRateInRadians[IMUFilterYaw];
}

const float IMUFilter::rollAngleInDegrees()
{		
	return _processedAngleInDegrees[IMUFilterRoll];
}

const float IMUFilter::pitchAngleInDegrees()
{			
	return _processedAngleInDegrees[IMUFilterPitch];
}

const float IMUFilter::yawAngleInDegrees()
{
	return _processedAngleInDegrees[IMUFilterYaw];
}
		

const float IMUFilter::rollAngleInRadians()
{			
	return _processedAngleInRadians[IMUFilterRoll];
}

const float IMUFilter::pitchAngleInRadians()
{		
	return _processedAngleInRadians[IMUFilterPitch];
}

const float IMUFilter::yawAngleInRadians()
{
	return _processedAngleInRadians[IMUFilterYaw];
}	


//Working
//Just a simple Accel only filter.
SimpleAccellOnlyIMUFilter::SimpleAccellOnlyIMUFilter() : IMUFilter()
{
	
}

void SimpleAccellOnlyIMUFilter::initialize()
{
	IMUFilter::initialize();
}

void SimpleAccellOnlyIMUFilter::filter(const unsigned long currentTime)
{
	IMUFilter::filter(currentTime);
	
	//Very simplistic angle determination (just use the raw acels - very noisy, but good to test things without the filter getting in the way)
	this->_calculateRawAngles(&_processedAngleInRadians[IMUFilterRoll], &_processedAngleInRadians[IMUFilterPitch], &_processedAngleInRadians[IMUFilterYaw]);
				
	//Convert to Degrees just incase we need it anywhere (convience sake only)			
	_processedAngleInDegrees[IMUFilterRoll] = ToDeg(_processedAngleInRadians[IMUFilterRoll]);
	_processedAngleInDegrees[IMUFilterPitch] = ToDeg(_processedAngleInRadians[IMUFilterPitch]);
	_processedAngleInDegrees[IMUFilterYaw] = ToDeg(_processedAngleInRadians[IMUFilterYaw]);
	
	if (_firstPass) {_firstPass = false;}
}


//Working
//Based on the code by RoyB at: http://www.rcgroups.com/forums/showpost.php?p=12082524&postcount=1286    
ComplimentryMUFilter::ComplimentryMUFilter() : IMUFilter()
{
    filterTerm0_roll = filterTerm0_pitch = filterTerm0_yaw = 0;
	filterTerm1_roll = filterTerm1_pitch = filterTerm1_yaw = 0;
	filterTerm2_roll = filterTerm2_pitch = filterTerm2_yaw = 0;
	
	previousAngle_roll = previousAngle_pitch = previousAngle_yaw = 0;			 
}

void ComplimentryMUFilter::initialize()
{
    timeConstantCF = 4;

	IMUFilter::initialize();
}

void ComplimentryMUFilter::filter(const unsigned long currentTime)
{			
	IMUFilter::filter(currentTime);
	
	float dt = _deltaTime / 1000.0;			//delta time in Seconds
	
	//Only calculate the roll and pitch.  We'll oragnize YAW down further after filtering the data				
	float newPitchAngle, newRollAngle, newYawAngle;
	this->_calculateRawAngles(&newRollAngle, &newPitchAngle, NULL);
										
	if (_firstPass)
	{
		previousAngle_roll = newRollAngle;
		filterTerm2_roll = _processedRateInRadians[IMUFilterRoll];
		
		previousAngle_pitch = newPitchAngle;
		filterTerm2_pitch = _processedRateInRadians[IMUFilterPitch];	
		
		//previousAngle_yaw = newYawAngle;
		//filterTerm2_yaw = _processedRateInRadians[IMUFilterYaw];								
		
		_firstPass = false;
	}
	else
	{		
		float sqTimeConstant = sq(timeConstantCF *  timeConstantCF);
		float doubleTimeConstant = 2 * timeConstantCF;
		
		filterTerm0_roll = (newRollAngle - previousAngle_roll) * sqTimeConstant;
		filterTerm2_roll += (filterTerm0_roll * dt);
		filterTerm1_roll = filterTerm2_roll + (newRollAngle - previousAngle_roll) * doubleTimeConstant + _processedRateInRadians[IMUFilterRoll];
		previousAngle_roll = (filterTerm1_roll * dt) + previousAngle_roll;
		
		filterTerm0_pitch = (newPitchAngle - previousAngle_pitch) * sqTimeConstant;
		filterTerm2_pitch += filterTerm0_pitch * dt;
		filterTerm1_pitch = filterTerm2_pitch + (newPitchAngle - previousAngle_pitch) * doubleTimeConstant + _processedRateInRadians[IMUFilterPitch];
		previousAngle_pitch = (filterTerm1_pitch * dt) + previousAngle_pitch;	
		
		newYawAngle = this->_correctYawReading(previousAngle_roll, previousAngle_pitch);
		
		/*filterTerm0_yaw = (newYawAngle - previousAngle_yaw) * sqTimeConstant;
		filterTerm2_yaw += filterTerm0_yaw * dt;
		filterTerm1_yaw = filterTerm2_yaw + (newYawAngle - previousAngle_yaw) * doubleTimeConstant + _processedRateInRadians[IMUFilterYaw];
		previousAngle_yaw = (filterTerm1_yaw * dt) + previousAngle_yaw;*/			
	}						
	
	_processedAngleInRadians[IMUFilterRoll] = previousAngle_roll;
	_processedAngleInRadians[IMUFilterPitch] = previousAngle_pitch;
	_processedAngleInRadians[IMUFilterYaw] = newYawAngle; //previousAngle_yaw; <-> filtered YAW "bounces"
	
	_processedAngleInDegrees[IMUFilterRoll] = ToDeg(_processedAngleInRadians[IMUFilterRoll]);
	_processedAngleInDegrees[IMUFilterPitch] = ToDeg(_processedAngleInRadians[IMUFilterPitch]);
	_processedAngleInDegrees[IMUFilterYaw] = ToDeg(_processedAngleInRadians[IMUFilterYaw]);
}



//Not working yet.
//Just returns 0's
void DCMIMUFilter::_matrixUpdate()
{
	
	_gyroVector[0] = ToRad(_gyroRaw[IMUFilterAxisX]);
	_gyroVector[1] = ToRad(_gyroRaw[IMUFilterAxisY]);
	_gyroVector[2] = ToRad(_gyroRaw[IMUFilterAxisZ]);

	_accelVector[0] = _accelRaw[IMUFilterAxisX];
	_accelVector[1] = _accelRaw[IMUFilterAxisY];
	_accelVector[2] = _accelRaw[IMUFilterAxisZ];

	// Low pass filter on accelerometer data (to filter vibrations)
	//Accel_Vector[0]=Accel_Vector[0]*0.5 + (float)read_adc(3)*0.5; // acc x
	//Accel_Vector[1]=Accel_Vector[1]*0.5 + (float)read_adc(4)*0.5; // acc y
	//Accel_Vector[2]=Accel_Vector[2]*0.5 + (float)read_adc(5)*0.5; // acc z

	Vector_Add(&_omega[0], &_gyroVector[0], &_omegaI[0]);//adding integrator
	Vector_Add(&_omegaVector[0], &_omega[0], &_omegaP[0]);//adding proportional

	//Accel_adjust();//adjusting centrifugal acceleration. // Not used for quadcopter

	_updateMatrix[0][0] = 0;
	_updateMatrix[0][1] = -_dt * _omegaVector[2];//-z
	_updateMatrix[0][2] = _dt * _omegaVector[1];//y
	_updateMatrix[1][0] = _dt * _omegaVector[2];//z
	_updateMatrix[1][1] = 0;
	_updateMatrix[1][2] = -_dt * _omegaVector[0];//-x
	_updateMatrix[2][0] = -_dt * _omegaVector[1];//-y
	_updateMatrix[2][1] = _dt * _omegaVector[0];//x
	_updateMatrix[2][2] = 0;

	Matrix_Multiply(_DCMMatrix,_updateMatrix,_temporaryMatrix); //a*b=c

	for(int x = 0; x<3; x++)  //Matrix Addition (update)
	{
		for(int y = 0; y<3; y++)
		{
			_DCMMatrix[x][y] += _temporaryMatrix[x][y];
		} 
	}
}

void DCMIMUFilter::_normalize()
{
	float error = 0;
	float temporary[3][3];
	float renorm = 0;

	error= -Vector_Dot_Product(&_DCMMatrix[0][0],&_DCMMatrix[1][0])*.5; //eq.19

	Vector_Scale(&temporary[0][0], &_DCMMatrix[1][0], error); //eq.19
	Vector_Scale(&temporary[1][0], &_DCMMatrix[0][0], error); //eq.19

	Vector_Add(&temporary[0][0], &temporary[0][0], &_DCMMatrix[0][0]);//eq.19
	Vector_Add(&temporary[1][0], &temporary[1][0], &_DCMMatrix[1][0]);//eq.19

	Vector_Cross_Product(&temporary[2][0],&temporary[0][0],&temporary[1][0]); // c= a x b //eq.20

	renorm= .5 *(3 - Vector_Dot_Product(&temporary[0][0],&temporary[0][0])); //eq.21
	Vector_Scale(&_DCMMatrix[0][0], &temporary[0][0], renorm);

	renorm= .5 *(3 - Vector_Dot_Product(&temporary[1][0],&temporary[1][0])); //eq.21
	Vector_Scale(&_DCMMatrix[1][0], &temporary[1][0], renorm);

	renorm= .5 *(3 - Vector_Dot_Product(&temporary[2][0],&temporary[2][0])); //eq.21
	Vector_Scale(&_DCMMatrix[2][0], &temporary[2][0], renorm);	
}

void DCMIMUFilter::_driftCorrection()
{
	//Compensation the Roll, Pitch and Yaw drift. 
	float errorCourse;
	static float Scaled_Omega_P[3];
	static float Scaled_Omega_I[3];			

	//*****Roll and Pitch***************
	Vector_Cross_Product(&_errorRollPitch[0],&_accelVector[0],&_DCMMatrix[2][0]); //adjust the ground of reference
	Vector_Scale(&_omegaP[0],&_errorRollPitch[0],_kpRollPitch*_accelWeight);

	Vector_Scale(&Scaled_Omega_I[0],&_errorRollPitch[0],_kpRollPitch*_accelWeight);
	Vector_Add(_omegaI,_omegaI,Scaled_Omega_I);

	//*****YAW***************
	// We make the gyro YAW drift correction based on compass magnetic heading 
	errorCourse = (_DCMMatrix[0][0]*_suplimentalRaw[IMUFilterAxisY]) - (_DCMMatrix[1][0]*_suplimentalRaw[IMUFilterAxisX]);  //Calculating YAW error
	Vector_Scale(_errorYaw,&_DCMMatrix[2][0],errorCourse); //Applys the yaw correction to the XYZ rotation of the aircraft, depeding the position.

	Vector_Scale(&Scaled_Omega_P[0],&_errorYaw[0],_kpYaw);
	Vector_Add(_omegaP,_omegaP,Scaled_Omega_P);//Adding  Proportional.

	Vector_Scale(&Scaled_Omega_I[0],&_errorYaw[0],_kiYaw);
	Vector_Add(_omegaI,_omegaI,Scaled_Omega_I);//adding integrator to the Omega_I*/			
}

void DCMIMUFilter::_eulerAngles()
{
	// Euler angles from DCM matrix
	_processedAngleInRadians[IMUFilterRoll] = atan2(_DCMMatrix[2][1],_DCMMatrix[2][2]);
	_processedAngleInRadians[IMUFilterPitch] = asin(-_DCMMatrix[2][0]);
	//yaw = atan2(DCM_Matrix[1][0],DCM_Matrix[0][0]);
}

DCMIMUFilter::DCMIMUFilter() : IMUFilter()
{
	
}

void DCMIMUFilter::initialize()
{
	IMUFilter::initialize();
	
	_kpRollPitch = 0.002;
	_kiRollPitch = 0.0000005;
	_kpYaw = 1.5;
	_kiYaw = 0.00005;
	
	_accelWeight = 1.0;

	//Initialize all the matrix
	{
		_DCMMatrix[0][0] = 1;
		_DCMMatrix[0][1] = 0;
		_DCMMatrix[0][2] = 0;
	
		_DCMMatrix[1][0] = 0;
		_DCMMatrix[1][1] = 1;
		_DCMMatrix[1][2] = 0;
	
		_DCMMatrix[2][0] = 0;
		_DCMMatrix[2][1] = 0;
		_DCMMatrix[2][2] = 1;


		_updateMatrix[0][0] = 0;
		_updateMatrix[0][1] = 1;
		_updateMatrix[0][2] = 2;
	
		_updateMatrix[1][0] = 3;
		_updateMatrix[1][1] = 4;
		_updateMatrix[1][2] = 5;
	
		_updateMatrix[2][0] = 6;
		_updateMatrix[2][1] = 7;
		_updateMatrix[2][2] = 8;
	

		_temporaryMatrix[0][0] = 0;
		_temporaryMatrix[0][1] = 0;
		_temporaryMatrix[0][2] = 0;
	
		_temporaryMatrix[1][0] = 0;
		_temporaryMatrix[1][1] = 0;
		_temporaryMatrix[1][2] = 0;
	
		_temporaryMatrix[2][0] = 0;
		_temporaryMatrix[2][1] = 0;
		_temporaryMatrix[2][2] = 0;


		_omegaVector[0] = 0;
		_omegaVector[1] = 0;
		_omegaVector[2] = 0;
	

		_omegaP[0] = 0;
		_omegaP[1] = 0;
		_omegaP[2] = 0;
	

		_omegaI[0] = 0;
		_omegaI[1] = 0;
		_omegaI[2] = 0;
	

		_omega[0] = 0;
		_omega[1] = 0;
		_omega[2] = 0;
	

		_errorRollPitch[0] = 0;
		_errorRollPitch[1] = 0;
		_errorRollPitch[2] = 0;
	

		_errorYaw[0] = 0;
		_errorYaw[1] = 0;
		_errorYaw[2] = 0;


		_accelVector[0] = 0;
		_accelVector[1] = 0;
		_accelVector[2] = 0;


		_gyroVector[0] = 0;
		_gyroVector[1] = 0;
		_gyroVector[2] = 0;
	}
}

void DCMIMUFilter::filter(const unsigned long currentTime)
{
	IMUFilter::filter(currentTime);
	
	//get the delta time in seconds
	_dt = _deltaTime / 1000.0f;
	
	_matrixUpdate(); 
	_normalize();
	_driftCorrection();
	_eulerAngles();
	
	if (_firstPass) {_firstPass = false;}
}


//Working 
//Based on code at: http://gluonpilot.com
	
// Initializing the struct
void KalmanIMUFilter::_init(struct Gyro1DKalman *filterdata, float Q_angle, float Q_gyro, float R_angle)
{
	memset (filterdata, 0, sizeof (struct Gyro1DKalman));
	filterdata->Q_angle = Q_angle;
	filterdata->Q_gyro  = Q_gyro;
	filterdata->R_angle = R_angle;
}

// Kalman predict
/*
 * The predict function. Updates 2 variables:
 * our model-state x and the 2x2 matrix P
 *     
 * x = [ angle, bias ]' 
 * 
 *   = F x + B u
 *
 *   = [ 1 -dt, 0 1 ] [ angle, bias ] + [ dt, 0 ] [ dotAngle 0 ]
 *
 *   => angle = angle + dt (dotAngle - bias)
 *      bias  = bias
 *
 *
 * P = F P transpose(F) + Q
 *
 *   = [ 1 -dt, 0 1 ] * P * [ 1 0, -dt 1 ] + Q
 *
 *  P(0,0) = P(0,0) - dt * ( P(1,0) + P(0,1) ) + dt² * P(1,1) + Q(0,0)
 *  P(0,1) = P(0,1) - dt * P(1,1) + Q(0,1)
 *  P(1,0) = P(1,0) - dt * P(1,1) + Q(1,0)
 *  P(1,1) = P(1,1) + Q(1,1)
 *
 *
 */
void KalmanIMUFilter::_predict(struct Gyro1DKalman *filterdata, const float dotAngle, const float dt)
{
	filterdata->x_angle += dt * (dotAngle - filterdata->x_bias);

	filterdata->P_00 +=  - dt * (filterdata->P_10 + filterdata->P_01) + filterdata->Q_angle * dt;
	filterdata->P_01 +=  - dt * filterdata->P_11;
	filterdata->P_10 +=  - dt * filterdata->P_11;
	filterdata->P_11 +=  + filterdata->Q_gyro * dt;
}

// Kalman update
/*
 *  The update function updates our model using 
 *  the information from a 2nd measurement.
 *  Input angle_m is the angle measured by the accelerometer.
 *
 *  y = z - H x
 *
 *  S = H P transpose(H) + R
 *    = [ 1 0 ] P [ 1, 0 ] + R
 *    = P(0,0) + R
 * 
 *  K = P transpose(H) S^-1
 *    = [ P(0,0), P(1,0) ] / S
 *
 *  x = x + K y
 *
 *  P = (I - K H) P
 *
 *    = ( [ 1 0,    [ K(0),
 *          0 1 ] -   K(1) ] * [ 1 0 ] ) P
 *
 *    = [ P(0,0)-P(0,0)*K(0)  P(0,1)-P(0,1)*K(0),
 *        P(1,0)-P(0,0)*K(1)  P(1,1)-P(0,1)*K(1) ]
 */
float KalmanIMUFilter::_update(struct Gyro1DKalman *filterdata, const float angle_m)
{
	const float y = angle_m - filterdata->x_angle;

	const float S = filterdata->P_00 + filterdata->R_angle;
	const float K_0 = filterdata->P_00 / S;
	const float K_1 = filterdata->P_10 / S;

	filterdata->x_angle +=  K_0 * y;
	filterdata->x_bias  +=  K_1 * y;

	filterdata->P_00 -= K_0 * filterdata->P_00;
	filterdata->P_01 -= K_0 * filterdata->P_01;
	filterdata->P_10 -= K_1 * filterdata->P_00;
	filterdata->P_11 -= K_1 * filterdata->P_01;

	return filterdata->x_angle;
}


KalmanIMUFilter::KalmanIMUFilter() : IMUFilter()
{
	
}

void KalmanIMUFilter::initialize()
{
	IMUFilter::initialize();
	
	_init(&_rollFilterData, 0.001, 0.003, 0.03);
	_init(&_pitchFilterData, 0.001, 0.003, 0.03);		
}

void KalmanIMUFilter::filter(const unsigned long currentTime)
{
	IMUFilter::filter(currentTime);

	float dt = _deltaTime / 1000.0f;  //delta time in seconds
	
	float rawRollAngle, rawPitchAngle, rawYawAngle;
	this->_calculateRawAngles(&rawRollAngle, &rawPitchAngle, NULL);	

	if (_firstPass)
	{
		//make sure to initialize things the first time through.
		_rollFilterData.x_angle = rawRollAngle;
		_pitchFilterData.x_angle = rawPitchAngle;			
		
		_firstPass = false;
	}
	else
	{
		//Filter Roll
		_predict(&_rollFilterData, _processedRateInRadians[IMUFilterRoll], dt);
	 	_update(&_rollFilterData, rawRollAngle);
	
		//Filter Pitch
		_predict(&_pitchFilterData, _processedRateInRadians[IMUFilterPitch], dt);
		_update(&_pitchFilterData, rawPitchAngle);									
	}				
	
	_processedAngleInRadians[IMUFilterRoll] = _rollFilterData.x_angle;
	_processedAngleInRadians[IMUFilterPitch] = _pitchFilterData.x_angle;				
	_processedAngleInRadians[IMUFilterYaw] = this->_correctYawReading(_rollFilterData.x_angle, _pitchFilterData.x_angle);
	
	_processedAngleInDegrees[IMUFilterRoll] = ToDeg(_processedAngleInRadians[IMUFilterAxisX]);
	_processedAngleInDegrees[IMUFilterPitch] = ToDeg(_processedAngleInRadians[IMUFilterAxisY]);
	_processedAngleInDegrees[IMUFilterYaw] = ToDeg(_processedAngleInRadians[IMUFilterAxisZ]);				
}	



//Not working yet
//Based on the code at: http://code.google.com/p/gluonpilot/source/browse/trunk/Firmware/rtos_pilot/ahrs_simple_quaternion.c
/*class QuaternionIMUFilter : public IMUFilter
{
	private:
		static double pitch_rad, roll_rad;
		static double pitch_acc, roll_acc;
		static double q[4];
		static double w; 		// speed along z axis
		
		static double p_bias;	// in rad/sec
		static double q_bias;

		void quaternionNormalize(double *q)
		{
		        double norm = sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
		
		        q[0] /= norm;   
		        q[1] /= norm;
		        q[2] /= norm;
		        q[3] /= norm;
		}

		void quaternionFromAttitude (const double roll, const double pitch, const double yaw, double* q)
		{
		        double cos_roll_2 = cosf(roll/2.0);
		        double sin_roll_2 = sinf(roll/2.0);
		        double cos_pitch_2 = cosf(pitch/2.0);
		        double sin_pitch_2 = sinf(pitch/2.0);
		        double cos_yaw_2 = cosf(yaw/2.0);
		        double sin_yaw_2 = sinf(yaw/2.0);
		
		        q[0] = cos_roll_2 * cos_pitch_2 * cos_yaw_2 + sin_roll_2 * sin_pitch_2 * sin_yaw_2;
		        q[1] = sin_roll_2 * cos_pitch_2 * cos_yaw_2 - cos_roll_2 * sin_pitch_2 * sin_yaw_2;
		        q[2] = cos_roll_2 * sin_pitch_2 * cos_yaw_2 + sin_roll_2 * cos_pitch_2 * sin_yaw_2;
		        q[3] = cos_roll_2 * cos_pitch_2 * sin_yaw_2 - sin_roll_2 * sin_pitch_2 * cos_yaw_2; // WAS cos_roll_2 * cos_pitch_2 * sin_yaw_2 - sin_roll_2 * sin_pitch_2 * sin_yaw_2
		
		        quaternion_normalize(q);
		}
		
		
		double quaternionToRoll (const double* q)
		{
		        double r;
		        errno = 0;
		        r = atan2( 2.0 * ( q[2]*q[3] + q[0]*q[1] ) ,
		                     (1.0 - 2.0 * (q[1]*q[1] + q[2]*q[2])) );  
		        if (errno)
		                return  0.0;
		        else
		                return r;
		}       
		
		double quaternionToPitch(const double* q)
		{
		        double r;
		        errno = 0;
		        r = asinf( -2.0 * (q[1]*q[3] - q[0]*q[2]) );    
		        if (errno)
		                return  0.0;
		        else
		                return r;
		}
		
		
		double quaternionToYaw(const double* q)
		{
		        double r;
		        errno = 0;
		        r = atan2( 2.0 * ( q[0]*q[3] + q[1]*q[2] ) ,
		                  (1.0 - 2.0 * (q[2]*q[2] + q[3]*q[3])) );  
		        if (errno)
		                return  0.0;
		        else
		                return r;
		}       


	public:
		QuaternionIMUFilter() : IMUFilter()
		{
			pitch_rad = 0.0, roll_rad = 0.0;
			pitch_acc = 0.0, roll_acc = 0.0;
			
			p_bias = 0.0;  
			q_bias = 0.0;
		}
		
		void initialize()
		{
			IMUFilter::initialize();
			
			quaternionFromAttitude(0.0, 0.0, 0.0, q);                	
		}
		
		void filter(const unsigned long currentTime)
		{
			IMUFilter::filter(currentTime);
			
			float sensorData_p = _processedRateInRadians[IMUFilterRoll] + p_bias;
			float sensorData_q = _processedRateInRadians[IMUFilterPitch] + q_bias;
			
			quaternionUpdateWithRates(sensorData_p, sensorData_q, _processedRateInRadians[IMUFilterYaw], q, _deltaTime);

			float roll_rad = quaternionToRoll(q);
        	float pitch_rad = quaternionToPitch(q);
        	float yaw_rad = quaternionToYaw(q);

			
			if (_firstPass) {_firstPass = false;}
		}
};*/

			
IMU::IMU() : SubSystem()
{
	_imuHardware = NULL;
	_imuFilter = NULL;
	_suplimentalYawSource = NULL;
	
	_altitudeRateOfChange = 0;
	_lastAltitudeTime = 0;
}

void IMU::initialize(const unsigned int frequency, const unsigned int offset) 
{ 	
	SubSystem::initialize(frequency, offset);

	if (_imuHardware)
	{
		_imuHardware->initialize();
	}
	
	if (_suplimentalYawSource)
	{
		_suplimentalYawSource->initialize();
	}
	
	if (_imuFilter)
	{
		_imuFilter->initialize();
	}
	
	serialcoms.shell()->registerKeyword("calibrateIMU", "calibrateIMU", _calibrateIMU);
	serialcoms.shell()->registerKeyword("monitorIMU", "monitorIMU", _monitorIMU, true);	
}

void IMU::setHardwareType(const HardwareType hardwareType)
{
	switch (hardwareType)
	{	
		case ArduPilotOilPan:
		{
			_imuHardware = new OilpanIMU();
			break;
		}
		default:
		{
			serialcoms.println("ERROR: Unknown IMU Hardware type selected.");					
			break;
		}
	}
}

void IMU::setSuplimentalYawSource(const SuplimentalYawSourceType yawSourceType)
{
	switch (yawSourceType)
	{
		case HMC5843:
		{
			_suplimentalYawSource = new HMC5843Compass();
			break;
		}
		
		default:
		{
			serialcoms.println("ERROR: Unknown Suplimental Yaw Source type selected.");						
			break;
		}
	}
}

void IMU::setFilterType(const FilterType filterType)
{
	switch (filterType)
	{
		case SimpleAccellOnly:
		{
			_imuFilter = new SimpleAccellOnlyIMUFilter();
			break;
		}
		
		case Complimentry:
		{
			_imuFilter = new ComplimentryMUFilter();
			break;
		}								
		
		case DCM:
		{
			_imuFilter = new DCMIMUFilter();
			break;
		}
		
		case Kalman:
		{
			_imuFilter = new KalmanIMUFilter();
			break;
		}
		
		/*case Quaternion:
		{
			_imuFilter = new QuaternionIMUFilter();
			break;
		}*/
						
		default:
		{
			serialcoms.println("ERROR: Unknown IMU Filter type selected.");					
			break;
		}
	}
}

void IMU::process(const unsigned long currentTime)
{
	if (this->_canProcess(currentTime))
	{
		SubSystem::recordProcessingStartTime();
		
		if (_imuHardware)
		{
			_imuHardware->process(currentTime);
			
			if (_suplimentalYawSource)
			{
				_suplimentalYawSource->process(currentTime);
			}

			if (_imuFilter)
			{
				_imuFilter->setCurrentReadings(_imuHardware->currentReadings());
				if (_suplimentalYawSource)
				{
					_imuFilter->setSuplimentalYawReadings(_suplimentalYawSource->currentReadings());
				}
				_imuFilter->filter(currentTime);
				
				//Process the altitude so that we can find out its rate of change (in m/s)
				float currentAltitude = this->altitudeInMeters();
				float altitudeChange = currentAltitude - _lastAltitude;
				
				_altitudeRateOfChange = altitudeChange / (float)(currentTime - _lastAltitudeTime);
				
				_lastAltitude = currentAltitude;
				_lastAltitudeTime = currentTime;
			}
								
			/*int rollTransmitterCommand = receiver.channelValue(ReceiverHardware::Channel1);
			int pitchTransmitterCommand = receiver.channelValue(ReceiverHardware::Channel2);
			int throttleTransmitterCommand = receiver.channelValue(ReceiverHardware::Channel3);
			int yawTransmitterCommand = receiver.channelValue(ReceiverHardware::Channel4);
			
			//DEBUGSERIALPRINT(throttleTransmitterCommand);
			//DEBUGSERIALPRINT(",");
			//DEBUGSERIALPRINT(yawTransmitterCommand);
			//DEBUGSERIALPRINTLN("");
			
			//Check for some basic "Command" stick positions
			if ((throttleTransmitterCommand < 1100 && yawTransmitterCommand < 1100) && (rollTransmitterCommand > 1900 && pitchTransmitterCommand < 1100))
			{
				this->calibrateZero();
			}*/
			
			SubSystem::recordProcessingEndTime();
		}
	}
}

void IMU::calibrateZero()
{
	if (_imuHardware)
	{
		_imuHardware->calibrateZero();
	}	
}
		
//Accessors for the processed values
const float IMU::rollRateInDegrees()
{
	if (_imuFilter)
	{
		return _imuFilter->rollRateInDegrees();
	}
	
	return 0;
}

const float IMU::pitchRateInDegrees()
{
	if (_imuFilter)
	{
		return _imuFilter->pitchRateInDegrees();
	}
	
	return 0;	
}

const float IMU::yawRateInDegrees()
{
	if (_imuFilter)
	{
		return _imuFilter->yawRateInDegrees();
	}
	
	return 0;
}

const float IMU::rollRateInRadians()
{
	if (_imuFilter)
	{
		return _imuFilter->rollRateInRadians();
	}
	
	return 0;
}

const float IMU::pitchRateInRadians()
{
	if (_imuFilter)
	{
		return _imuFilter->pitchRateInRadians();
	}
	
	return 0;	
}

const float IMU::yawRateInRadians()
{
	if (_imuFilter)
	{
		return _imuFilter->yawRateInRadians();
	}
	
	return 0;
}




const float IMU::rollAngleInDegrees()
{
	if (_imuFilter)
	{
		return _imuFilter->rollAngleInDegrees();
	}
	
	return 0;
}

const float IMU::pitchAngleInDegrees()
{
	if (_imuFilter)
	{
		return _imuFilter->pitchAngleInDegrees();
	}
	
	return 0;
}

const float IMU::yawAngleInDegrees()
{
	if (_imuFilter)
	{
		return _imuFilter->yawAngleInDegrees();
	}
	
	return 0;
}

const float IMU::rollAngleInRadians()
{
	if (_imuFilter)
	{
		return _imuFilter->rollAngleInRadians();
	}
	
	return 0;
}

const float IMU::pitchAngleInRadians()
{
	if (_imuFilter)
	{
		return _imuFilter->pitchAngleInRadians();
	}
	
	return 0;
}

const float IMU::yawAngleInRadians()
{
	if (_imuFilter)
	{
		return _imuFilter->yawAngleInRadians();
	}
	
	return 0;
}

const float IMU::altitudeInMeters()
{
	return ((gps.altitude()/100.0f) + sensors.altitudeUsingPressure()) / 2;
}

const float IMU::altitudeRateInMeters()
{
	return _altitudeRateOfChange;
}

IMU imu;

const ArduinoShellCallback::callbackReturn _calibrateIMU(ArduinoShell &shell, const int argc, const char* argv[])
{
	//Ensure that we don't have armed motors since this is disruptive.
	if (motors.armed())
	{
		shell << "ERROR: Unable to calibrate the IMU while motors are armed." << endl;		
	}
	else
	{
		shell << "Calibrating..." << endl;		
		imu.calibrateZero();	
		shell << "Calibration complete" << endl;
	}
	
	return ArduinoShellCallback::Success;
}
const ArduinoShellCallback::callbackReturn _monitorIMU(ArduinoShell &shell, const int argc, const char* argv[])
{
	shell << imu.rollRateInDegrees() << ","
			<< imu.pitchRateInDegrees() << ","
			<< imu.yawRateInDegrees() << ","
			<< imu.altitudeRateInMeters() << ","		
			<< imu.rollAngleInDegrees() << ","
			<< imu.pitchAngleInDegrees() << ","
			<< imu.yawAngleInDegrees() << ","		
			<< imu.altitudeInMeters() << endl;
		
	return ArduinoShellCallback::Success;
}
