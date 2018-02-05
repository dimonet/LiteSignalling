/// GSM сигналка c установкой по кнопке
/// с датчиками движения и растяжкой (или с геркониевым датчиком)
/// ВНИМАНИЕ: для корретной работы sms необходимо установить размеры буферов вместо 64 на SERIAL_TX_BUFFER_SIZE 24 и SERIAL_RX_BUFFER_SIZE 170 в файле hardware\arduino\avr\cores\arduino\HardwareSerial.h

#include <EEPROM.h>
#include "DigitalSensor.h"
#include <avr/pgmspace.h>

//#define debug Serial

//// НАСТРОЕЧНЫЕ КОНСТАНТЫ /////


// паузы
#define  delayOnContrTest     7                            // время паузы от нажатие кнопки до установки режима охраны в режиме тестирования
#define  timeAfterPressBtn    3000                         // время выполнения операций после единичного нажатия на кнопку
#define  timeSiren            20000                        // время работы сирены/тревоги в штатном режиме (милисекунды).
#define  timeSirenT           1000                         // время работы сирены/тревоги в тестовом режиме (милисекунды).
#define  timeSmsPIR1          120000                       // время паузы после последнего СМС датчика движения 1 (милисекунды)
#define  timeSkimpySiren      400                          // время короткого срабатывания модуля сирены
#define  timeAllLeds          1200                         // время горение всех светодиодов во время включения устройства (тестирования светодиодов)
#define  timeTestBlinkLed     400                          // время мерцания светодиода при включеном режима тестирования
#define  timeTestBoardLed     3000                         // время мерцания внутреннего светодиода на плате при включеном режима тестирования
#define  timeTrigSensor       1000                         // во избежании ложного срабатывании датчика тревога включается только если датчик срабатывает больше чем указанное время (импл. только для расстяжки)

//// КОНСТАНТЫ ДЛЯ ПИНОВ /////
#define SpecerPin 8
#define OnContrLED 11
#define boardLED 6                              // LED для сигнализации текущего режима с помошью внутреннего светодиода на плате

#define Button 2                                // нога на кнопку
#define SirenGenerator 7                        // нога на сирену

// Спикер
#define sysTone 98                              // системный тон спикера
#define clickTone 98                            // тон спикера при нажатии на кнопку


//Sensores
#define pinSH1      A2                          // нога на растяжку
#define pinPIR1     3                           // нога датчика движения 1

//// КОНСТАНТЫ РЕЖИМОВ РАБОТЫ //// 
#define OutOfContrMod  1                        // снята с охраны
#define OnContrMod     2                        // установлена охрана

//// КОНСТАНТЫ EEPROM ////
#define E_mode           0                      // адресс для сохранения режимов работы 
#define E_inTestMod      1                      // адресс для сохранения режима тестирования

#define E_delaySiren     2                      // адресс для сохранения длины паузы между срабатыванием датяиков и включением сирены (в сикундах)
#define E_delayOnContr   3                      // время паузы от нажатия кнопки до установки режима охраны (в сикундах)

#define E_SirenEnabled   4
#define E_PIR1Siren      5                     
#define E_TensionSiren   6

#define E_IsPIR1Enabled  7                     
#define E_TensionEnabled 8

// Количество нажатий на кнопку для включений режимова
#define E_BtnOnContr     9                     // количество нажатий на кнопку для установки на охрану
#define E_BtnInTestMod   10                     // количество нажатий на кнопку для включение/отключения режима тестирования 
#define E_BtnSkimpySiren 11                     // количество нажатий на кнопку для кратковременного включения сирены
#define E_BtnOutOfContr  12


//// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ////

// константы режимов работы
byte mode = OutOfContrMod;                      // 1 - снята с охраны                                  
                                                // 2 - установлено на охрану                                                
                                                // при добавлении не забываем посмотреть рездел 
                                                
                                               
bool interrupt = false;                         // разрешить/запретить обработку прырывания (по умолчанию запретить, что б небыло ложного срабатывания при старте устройства)
bool inTestMod = false;                         // режим тестирования датчиков (не срабатывает сирена и не отправляются СМС)
bool isSiren = false;                           // режим сирены

bool SirEnabled = false;                        // включена/выключена сирена глобально
bool TensionSir = false;                        // включена/выключена сирена для растяжки
bool PIR1Sir = false;                           // включена/выключена сирену для датчика движения 1

bool reqSirena = false;                         // уст. в true когда сработал датчик и необходимо включить сирену
bool isRun = true;                              // флаг для управления выполнения блока кода в loop только при старте устройства


unsigned long prSiren = 0;                      // время включения сирены (милисекунды)
unsigned long prLastPressBtn = 0;               // время последнего нажатие на кнопку (милисекунды)
unsigned long prTestBlinkLed = 0;               // время мерцания светодиода при включеном режима тестирования (милисекунды)
unsigned long prReqSirena = 0;                  // время последнего обнаружения, что необходимо включать сирену

byte countPressBtn = 0;                         // счетчик нажатий на кнопку


// Датчики
DigitalSensor SenTension(pinSH1);
DigitalSensor SenPIR1(pinPIR1);

void(* RebootFunc) (void) = 0;  
// объявляем функцию Reboot

void setup() 
{
  delay(1000);                                // чтобы нечего не повисало при включении
 // debug.begin(19200);
  pinMode(SpecerPin, OUTPUT);
  pinMode(OnContrLED, OUTPUT);
  pinMode(boardLED, OUTPUT);                  // LED для сигнализации текущего режима с помошью внутреннего светодиода на плате
  pinMode(pinSH1, INPUT_PULLUP);              // нога на растяжку
  pinMode(pinPIR1, INPUT);                    // нога датчика движения 1
  pinMode(Button, INPUT_PULLUP);              // кнопка для установки режима охраны
  pinMode(SirenGenerator, OUTPUT);            // нога на сирену

  StopSiren();                                // при включении устройства сирена должна быть по умолчанию выключена
   
  // блок сброса очистки EEPROM (сброс всех настроек)
  if (digitalRead(Button) == LOW)
  { 
    byte count = 0;
    while (count < 100)
    {
      if (digitalRead(Button) == HIGH) break;
      count++;
      delay(100);
    }
    if (count == 100)
    {
        PlayTone(sysTone, 1000);               
        for (int i = 0 ; i < EEPROM.length() ; i++) 
          EEPROM.write(i, 0);                        // стираем все данные с EEPROM
        // установка дефолтных параметров
        EEPROM.write(E_mode, OutOfContrMod);         // устанавливаем по умолчанию режим не на охране
        EEPROM.write(E_inTestMod, 0);                // режим тестирования по умолчанию выключен
        EEPROM.write(E_delaySiren, 0);               // пауза между сработкой датчиков и включением сирены отключена (0 секунд) 
        EEPROM.write(E_delayOnContr, 25);            // пауза от нажатия кнопки до установки режима охраны (25 сек)
        EEPROM.write(E_IsPIR1Enabled, 1);            
        EEPROM.write(E_TensionEnabled, 0);
        EEPROM.write(E_BtnOnContr, 1);
        EEPROM.write(E_BtnInTestMod, 2);
        EEPROM.write(E_BtnSkimpySiren, 3);        
        EEPROM.write(E_BtnOutOfContr, 0);
        EEPROM.write(E_SirenEnabled, 1);              // сирена по умолчанию включена
        EEPROM.write(E_PIR1Siren, 1);                 // сирена при срабатывании датчика движения 1 по умолчанию включена
        EEPROM.write(E_TensionSiren, 1);              // сирена при срабатывании растяжки по умолчанию включена             
        RebootFunc();                                 // перезагружаем устройство
    }
  }  
   
  // блок тестирования спикера и всех светодиодов
  PlayTone(sysTone, 100);                          
  delay(500);
  digitalWrite(OnContrLED, HIGH);
  digitalWrite(boardLED, HIGH);
  delay(timeAllLeds);
  digitalWrite(OnContrLED, LOW);
  digitalWrite(boardLED, LOW);

  
  attachInterrupt(0, ClickButton, FALLING);             // привязываем 0-е прерывание к функции ClickButton(). 
  interrupt = true;                                     // разрешаем обработку прырывания  

  inTestMod = EEPROM.read(E_inTestMod);                 // читаем тестовый режим из еепром
  SirEnabled = EEPROM.read(E_SirenEnabled);             // читаем включена или выключена сирена глобально
  TensionSir = EEPROM.read(E_TensionSiren);             // читаем включена или выключена сирена для растяжки
  PIR1Sir = EEPROM.read(E_PIR1Siren);                   // читаем включена или выключена сирена для датчика движения 1
 
  // чтение конфигураций с EEPROM
  if (EEPROM.read(E_mode) == OnContrMod) Set_OnContrMod(true);                              // читаем режим из еепром      
    else Set_OutOfContrMod();  
}


void loop() 
{   
  if (inTestMod)                                                                            // если включен режим тестирования
  {
    if (GetElapsed(prTestBlinkLed) > timeTestBlinkLed)   
    {
      digitalWrite(boardLED, digitalRead(boardLED) == LOW);                               // то мигаем внутренним светодиодом на плате
      prTestBlinkLed = millis();
    }
  } 
  
  if (mode == OutOfContrMod) 
  {
    if (countPressBtn != 0)
    {     
      if (GetElapsed(prLastPressBtn) > timeAfterPressBtn)
      {       
        // установка на охрану countBtnOnContrMod
        if (countPressBtn == EEPROM.read(E_BtnOnContr))              // если кнопку нажали заданное количество для включение/отключения режима тестирования
        {
          countPressBtn = 0;  
          Set_OnContrMod(true);       
        }
        else
        // включение/отключения режима тестирования
        if (countPressBtn == EEPROM.read(E_BtnInTestMod))            // если кнопку нажали заданное количество для включение/отключения режима тестирования
        {
          countPressBtn = 0;  
          PlayTone(sysTone, 250);                                                             // сигнализируем об этом спикером  
          InTestMod(!inTestMod);
        }                                                                               
        else
        // кратковременное включение сирены (для тестирования модуля сирены)
        if (countPressBtn == EEPROM.read(E_BtnSkimpySiren))                      
        {
          countPressBtn = 0;  
          PlayTone(sysTone, 250);                                    
          SkimpySiren();
        }        
        else
        {
          PlayTone(sysTone, 250);   
          countPressBtn = 0;  
        }                            
      }                   
    }    
  }                                                                                           // end OutOfContrMod  
  else
  ////// IN CONTROL MODE ///////  
  if (mode == OnContrMod)                                                                     // если в режиме охраны
  {    
    if (countPressBtn != 0)
    {     
      if (GetElapsed(prLastPressBtn) > timeAfterPressBtn)
      {       
        // установка на охрану countBtnOnContrMod
        if (mode == OutOfContrMod && countPressBtn == EEPROM.read(E_BtnOnContr))              // если кнопку нажали заданное количество для включение/отключения режима тестирования
        {
          countPressBtn = 0;  
          Set_OnContrMod(true);       
        }
        else
        // включение/отключения режима тестирования
        if (mode == OutOfContrMod && countPressBtn == EEPROM.read(E_BtnInTestMod))            // если кнопку нажали заданное количество для включение/отключения режима тестирования
        {
          countPressBtn = 0;  
          PlayTone(sysTone, 250);                                                             // сигнализируем об этом спикером  
          InTestMod(!inTestMod);
        }                                                                               
        else
        // кратковременное включение сирены (для тестирования модуля сирены)
        if (mode == OutOfContrMod && countPressBtn == EEPROM.read(E_BtnSkimpySiren))                      
        {
          countPressBtn = 0;  
          PlayTone(sysTone, 250);                                    
          SkimpySiren();
        }        
        
        // выключение режима контроля (если настроена для этого кнопка)
        else 
        if (mode == OnContrMod && countPressBtn == EEPROM.read(E_BtnOutOfContr))      
        {
          delay(200);                                                                         // пайза, что б не сливались звуковые сигналы нажатия кнопки и установки режима
          countPressBtn = 0;          
          Set_OutOfContrMod();             
        }
        else
        {
          PlayTone(sysTone, 250);   
          countPressBtn = 0;  
        }                            
     }
     else
      // снятие кнопкой с охраны (работает только в тестовом режиме, когда не блокируются прерывания)
      if (mode == OnContrMod && inTestMod)                                                    // в тестовом режиме можно сниамть кнопкой с охраны
      {
        delay(200);                                                                           // пайза, что б не сливались звуковые сигналы нажатия кнопки и установки режима
        countPressBtn = 0;   
        Set_OutOfContrMod();       
      }                  
    }
    
    if (isSiren && !inTestMod)
    {
      if (GetElapsed(prSiren) > timeSiren)                                                 // если включена сирена и сирена работает больше установленного времени то выключаем ее
        StopSiren();
    } 
      
    if (EEPROM.read(E_TensionEnabled) && !SenTension.IsTrig && SenTension.CheckSensor())    // проверяем растяжку только если она не срабатывала ранее (что б смс и звонки совершались единоразово)
    {
      if (SenTension.PrTrigTime == 0) SenTension.PrTrigTime = millis();                     // если это первое срабатывание то запоминаем когда сработал датчик
      if (GetElapsed(SenTension.PrTrigTime) > timeTrigSensor)                               // реагируем на сработку датчика только если он срабатывает больше заданного времени (во избежании ложных срабатываний)
      {      
        if (!inTestMod && TensionSir) 
        {
          reqSirena = true;             
          if (prReqSirena == 1) prReqSirena = millis();                         
        }
        SenTension.PrTrigTime = millis();                                                   // запоминаем когда сработал датчик для отображения статуса датчика
        SenTension.IsTrig = true;                  
      }    
    }
    if (SenTension.PrTrigTime != 0 && !SenTension.IsTrig && !SenTension.CheckSensor())    // проверяем если были ложные срабатывания расстяжки то сбрасываем счетчик времени
      SenTension.PrTrigTime = 0;
    
    
    if (EEPROM.read(E_IsPIR1Enabled) && SenPIR1.CheckSensor())
    {            
      if (!inTestMod && PIR1Sir) 
      {
        reqSirena = true;
        if (prReqSirena == 1) prReqSirena = millis();
      }
      SenPIR1.PrTrigTime = millis();                                                       // запоминаем когда сработал датчик для отображения статуса датчика           
    }
   
    
    if (reqSirena 
      && (GetElapsed(prReqSirena)/1000 >= EEPROM.read(E_delaySiren) || prReqSirena == 0))      
    {       
      reqSirena = false;            
      if (!isSiren)
      {
        StartSiren();
        prReqSirena = 0;                                                                   // устанавливаем в 0 для отключения паузы между следующим срабатыванием датчиков и включением сирены
      }
      else
        prSiren = millis();      
    }         
  }                                                                                      // end OnContrMod                                                                                        
}



//// ------------------------------- Functions --------------------------------- ////
void ClickButton()
{ 
  if (interrupt)
  {
    static unsigned long millis_prev;
    if(millis()-300 > millis_prev) 
    {          
      PlayTone(clickTone, 40);    
      countPressBtn++;
      prLastPressBtn = millis();         
    }       
    millis_prev = millis();           
  }      
}

bool Set_OutOfContrMod()                                // метод для снятие с охраны
{ 
  mode = OutOfContrMod;                                 // снимаем с охраны
  interrupt = true;                                     // разрешаем обрабатывать прерывания
  digitalWrite(OnContrLED, LOW); 
  digitalWrite(boardLED, LOW);
  PlayTone(sysTone, 500);  
  StopSiren();                                          // выключаем сирену                         
  reqSirena = false; 
    
  SenPIR1.ResetSensor();
  SenTension.ResetSensor();
  
  EEPROM.write(E_mode, mode);                           // пишим режим в еепром, что б при следующем включении устройства, оно оставалось в данном режиме  
  return true;
}

bool Set_OnContrMod(bool IsWaiting)                     // метод для установки на охрану
{ 
  if (IsWaiting == true)                                // если включен режим ожидание перед установкой охраны, выдерживаем заданную паузу, что б успеть покинуть помещение
  {   
    digitalWrite(boardLED, LOW);                        // во время ожидание перед установкой на охрану, что б не горел внутренний светодиод, выключаем его (он моргает в режиме тестирования)
    byte timeWait = 0;
    if (inTestMod) timeWait = delayOnContrTest;         // если включен режим тестирования, устанавливаем для удобства тестирования меньшую паузу
    else timeWait = EEPROM.read(E_delayOnContr);        // если режим тестирования выклюяен, используем обычную паузу
    for(byte i = 0; i < timeWait; i++)   
    {               
      if (countPressBtn > 0)                            // если пользователь нажал на кнопку то установка на охрану прерывается
      {
        countPressBtn = 0;
        delay(200);                                     // пайза, что б не сливались звуковые сигналы нажатия кнопки и установки режима
        Set_OutOfContrMod();
        return false;
      }        
      if (i < (timeWait * 0.7))                         // первых 70% паузы моргаем медленным темпом
        BlinkLEDSpecer(OnContrLED, 0, 500, 500);              
      else                                              // последних 30% паузы ускоряем темп
      {
        BlinkLEDSpecer(OnContrLED, 0, 250, 250); 
        BlinkLEDSpecer(OnContrLED, 0, 250, 250);              
      }         
    }
  }
   
  // установка переменных в дефолтное состояние  
  SenPIR1.ResetSensor();
  SenTension.ResetSensor();
  
  //установка на охрану     
  mode = OnContrMod;                                    // ставим на охрану                                                    
  digitalWrite(boardLED, HIGH);
  digitalWrite(OnContrLED, HIGH);
  PlayTone(sysTone, 500);  
  EEPROM.write(E_mode, mode);                           // пишим режим в еепром, что б при следующем включении устройства, оно оставалось в данном режиме
  delay (2500);                                         // дополнительная пауза так как датчик держит лог. единицу 2,5
  prReqSirena = 1;                                      // устанавливаем в 1 для активации паузы между срабатыванием датчиков и включением сирены
  return true;
}

void StartSiren()
{  
  if (SirEnabled)                                       // и сирена не отключена в конфигурации
    digitalWrite(SirenGenerator, HIGH);                 // включаем сирену через через транзистор или релье  
  isSiren = true; 
  prSiren = millis(); 
}

void StopSiren()
{
  digitalWrite(SirenGenerator, LOW);                    // выключаем сирену через транзистор или релье
  isSiren = false;   
}


void SkimpySiren()                                                                        // метод для кратковременного включения сирены (для теститования сирены)
{
  digitalWrite(SirenGenerator, HIGH);                                                     // включаем сирену                                 
  delay(timeSkimpySiren);                                                                 // кратковременный период на который включается сирена
  digitalWrite(SirenGenerator, LOW);                                                      // выключаем сирену через релье  
}

void InTestMod(bool state)
{
  inTestMod = state;                                                                      // включаем/выключаем режим тестирование датчиков          
  EEPROM.write(E_inTestMod, inTestMod);                                                   // пишим режим тестирование датчиков в еепром    
}





