// 
//    FILE : Temps.h
//    Esta biblioteca mistura as funcoes do DHT11
//    e do sensor DS18B20

#ifndef temps_h
#define temps_h

#include <Arduino.h>
#include "OneWire.h" 

#define DHTLIB_OK               0
#define DHTLIB_ERROR_CHECKSUM   1
#define DHTLIB_ERROR_TIMEOUT    2
#define ERROR_NUM               -255 //menor byte

class Temps{
public:
  int readDHT(int pin){

    // BUFFER TO RECEIVE
    uint8_t bits[5];
    uint8_t cnt = 7;
    uint8_t idx = 0;

    // EMPTY BUFFER
    for (int i=0; i< 5; i++) bits[i] = 0;

    // REQUEST SAMPLE
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delay(18);
    digitalWrite(pin, HIGH);
    delayMicroseconds(40);
    pinMode(pin, INPUT);

    // ACKNOWLEDGE or TIMEOUT
    unsigned int loopCnt = 10000;
    while(digitalRead(pin) == LOW)
      if (loopCnt-- == 0) return DHTLIB_ERROR_TIMEOUT;

    loopCnt = 10000;
    while(digitalRead(pin) == HIGH)
      if (loopCnt-- == 0) return DHTLIB_ERROR_TIMEOUT;

    // READ OUTPUT - 40 BITS => 5 BYTES or TIMEOUT
    for (int i=0; i<40; i++)
    {
      loopCnt = 10000;
      while(digitalRead(pin) == LOW)
        if (loopCnt-- == 0) return DHTLIB_ERROR_TIMEOUT;

      unsigned long t = micros();

      loopCnt = 10000;
      while(digitalRead(pin) == HIGH)
        if (loopCnt-- == 0) return DHTLIB_ERROR_TIMEOUT;

      if ((micros() - t) > 40) bits[idx] |= (1 << cnt);
      if (cnt == 0)   // next byte?
      {
        cnt = 7;    // restart at MSB
        idx++;      // next byte!
      }
      else cnt--;
    }

    // WRITE TO RIGHT VARS as bits[1] and bits[3] are allways zero they are omitted in formulas.
    humidity    = bits[0]; 
    temperature = bits[2]; 

    uint8_t sum = bits[0] + bits[2];  

    if (bits[4] != sum) return DHTLIB_ERROR_CHECKSUM;
    return DHTLIB_OK;
  }

  float readDS(int pin){ //returns the temperature from one DS18S20 in DEG Celsius
    OneWire ds(pin);  // Para Sensor DS
    byte data[12];
    byte addr[8];
    if ( !ds.search(addr)) {
      //no more sensors on chain, reset search
      ds.reset_search();
      return ERROR_NUM;
    }
    if ( OneWire::crc8( addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return ERROR_NUM;
    }
    if ( addr[0] != 0x10 && addr[0] != 0x28) {
      Serial.print("Device is not recognized");
      return ERROR_NUM;
    }
    ds.reset();
    ds.select(addr);
    ds.write(0x44,1); // start conversion, with parasite power on at the end
    byte present = ds.reset();
    ds.select(addr);    
    ds.write(0xBE); // Read Scratchpad
    for (int i = 0; i < 9; i++) // we need 9 bytes
      data[i] = ds.read();
    ds.reset_search();
    byte MSB = data[1];
    byte LSB = data[0];
    float tempRead = ((MSB << 8) | LSB); //using two's compliment
    float TemperatureSum = tempRead / 16;
    return TemperatureSum;
  }

  double dewPoint(double celsius, double humidity){
    double RATIO = 373.15 / (273.15 + celsius);  // RATIO was originally named A0, possibly confusing in Arduino context
    double SUM = -7.90298 * (RATIO - 1);
    SUM += 5.02808 * log10(RATIO);
    SUM += -1.3816e-7 * (pow(10, (11.344 * (1 - 1/RATIO ))) - 1) ;
    SUM += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
    SUM += log10(1013.246);
    double VP = pow(10, SUM - 3) * humidity;
    double T = log(VP/0.61078);   // temp var
    return (241.88 * T) / (17.558 - T);
  }

  double dewPointFast(double celsius, double humidity){
    double a = 17.271;
    double b = 237.7;
    double temp = (a * celsius) / (b + celsius) + log(humidity/100);
    double Td = (b * temp) / (a - temp);
    return Td;
  }

 /* float readDS(int pin);
  double dewPoint(double celsius, double humidity);
  double dewPointFast(double celsius, double humidity);
*/
  int humidity;
  int temperature;
};
extern Temps ttemps;
#endif
//
// END OF FILE
//



