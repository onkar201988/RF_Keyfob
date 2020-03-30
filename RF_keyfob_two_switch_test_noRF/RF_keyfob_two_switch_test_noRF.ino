#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <LowPower.h>
#include <Vcc.h>
#include <avr/wdt.h>

#define debug                               // comment this to remove serial prints
//----------------------------------------------------------------------------------------------------
const float VccMin   = 1.8;                 // Minimum expected Vcc level, in Volts. (0%)
const float VccMax   = 3.2;                 // Maximum expected Vcc level, in Volts. (100%)
const float VccCorrection = 1.0/1.0;        // Measured Vcc by multimeter divided by reported Vcc

const int sleepDuration = 450;              // Sleep duration to report battery (8 Sec x number of times)(10800 for a day)
volatile int sleepCounter = 0;              // Counter to keep sleep count
const int aliveDuration = 38;               // Duration for alive counter update (8 Sec x number of times)(38 for 5 mins)
volatile int aliveDurationCounter = 0;      // Counter for alive duration

const uint8_t switch_one_pin = 2;           // Switch 1 pin
const uint8_t switch_two_pin = 3;           // switch 2 pin

volatile bool switchOneStatus = false;      // Switch one status global used in ISR
volatile bool switchTwoStatus = false;      // Switch one status global used in ISR

volatile byte switchOneCoutner = 0;         // Switch one counter
volatile byte switchTwoCoutner = 0;         // Switch two counter
volatile byte aliveCoutner = 0;             // alive counter
//----------------------------------------------------------------------------------------------------
Vcc vcc(VccCorrection);                     // create VCC object
//----------------------------------------------------------------------------------------------------
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(switch_one_pin, INPUT);
  digitalWrite(switch_one_pin, HIGH);
  attachInterrupt(digitalPinToInterrupt(switch_one_pin), switchOne_ISR, FALLING);

  pinMode(switch_two_pin, INPUT);
  digitalWrite(switch_two_pin, HIGH);
  attachInterrupt(digitalPinToInterrupt(switch_two_pin), switchTwo_ISR, FALLING);

  #ifdef debug
    Serial.begin(115200);                       //serial port to display received data
    Serial.println("keyfob test started ...");
  #endif

}

//----------------------------------------------------------------------------------------------------
void switchOne_ISR()
{
  if( switchOneStatus == false)
  {
    switchOneStatus = true;
  }
}

//----------------------------------------------------------------------------------------------------
void switchTwo_ISR()
{  
  if( switchTwoStatus == false)
  {
    switchTwoStatus = true;
  }
}

//----------------------------------------------------------------------------------------------------
void loop()
{
  if(switchOneStatus || switchTwoStatus)
  {
    // If any switch is pressed
    noInterrupts();
    
    #ifdef debug
      if(switchOneStatus)
      {
        switchOneCoutner++;
        Serial.println("Switch one ISR triggered, counter = ");
        Serial.println(switchOneCoutner);
      }
      else if(switchTwoStatus)
      {
        switchTwoCoutner++;
        Serial.println("Switch two ISR triggered, counter = ");
        Serial.println(switchTwoCoutner);
      }
      yield();
    #endif
    switchOneStatus = false;
    switchTwoStatus = false;
    interrupts();
  }

  else if(aliveDurationCounter > aliveDuration)
  {
    ++ aliveCoutner;
    Serial.println("Alive counter = ");
    Serial.println(aliveCoutner);
    aliveDurationCounter = 0;
  }
  
  else if(sleepCounter > sleepDuration)
  {
    #ifdef debug
      int batteryStatus = vcc.Read_Perc(VccMin, VccMax);
      Serial.println("Battery =");
      Serial.println(batteryStatus);
      delay(200);
      yield();
    #endif
    
    sleepCounter = 0;
    delay(50);
  }
  
  else
  {
    Serial.println("Going to sleep");
    delay(200);
    yield();
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
  sleepCounter++;
  aliveDurationCounter++;
  //delay(1000);
}
