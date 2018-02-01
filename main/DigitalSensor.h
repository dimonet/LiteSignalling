#include "Arduino.h"

class DigitalSensor
{
  public: 
    DigitalSensor(byte pinSensor);
    bool CheckSensor();           // проверяет состояние датчика и если он сработал то возвращает true
    void ResetSensor();           // сброс всех свойств датчика в значения по умолянию
    bool IsTrig;                  // указывает, что датчик сработал
    unsigned long PrTrigTime;     // время последнего срабатывания датчика
    unsigned long PrAlarmTime;    // время последнего оповещение о срабатывании датчика (СМС, тел. звонок)
        
  private:
    byte _pinSensor;              // пинг датчика который опрашивается
};
