#include "DigitalSensor.h"

DigitalSensor::DigitalSensor(byte pinSensor)
{  
  _pinSensor = pinSensor;
  ResetSensor();   
}

bool DigitalSensor::CheckSensor()
{
  if (digitalRead(_pinSensor) == HIGH) return true;
    else return false; 
}

void DigitalSensor::ResetSensor()
{
  IsTrig = false; 
  PrTrigTime = 0; 
  PrAlarmTime = 0;  
}
;


