// подсчет сколько прошло милисикунд после последнего срабатывания события (сирена, звонок и т.д.)
unsigned long GetElapsed(unsigned long prEventMillis)
{
  unsigned long tm = millis();
  return (tm >= prEventMillis) ? tm - prEventMillis : 0xFFFFFFFF - prEventMillis + tm + 1;  //возвращаем милисикунды после последнего события
}

void PlayTone(byte tone, unsigned int duration)
{
  if (mode != OnContrMod) digitalWrite(boardLED, LOW);
  for (unsigned long i = 0; i < duration * 1000L; i += tone * 2)
  {
    digitalWrite(SpecerPin, HIGH);
    delayMicroseconds(tone);
    digitalWrite(SpecerPin, LOW);
    delayMicroseconds(tone);
  }
}

void BlinkLEDSpecer(byte pinLED,  unsigned int millisBefore,  unsigned int millisHIGH,  unsigned int millisAfter)     // метод для включения спикера и заданного светодиода на заданное время
{
  digitalWrite(pinLED, LOW);
  delay(millisBefore);
  digitalWrite(pinLED, HIGH);
  PlayTone(sysTone, millisHIGH);
  digitalWrite(pinLED, LOW);
  delay(millisAfter);
}

