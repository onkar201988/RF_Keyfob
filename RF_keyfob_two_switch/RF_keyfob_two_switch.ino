#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <LowPower.h>
#include <Vcc.h>
#include <avr/wdt.h>

//#define debug                               // comment this to remove serial prints
//----------------------------------------------------------------------------------------------------
const uint64_t pipeAddress = 0xB00B1E50C3LL;// Create pipe address for the network, "LL" is for LongLong type
const uint8_t rfChannel = 89;               // Set channel frequency default (chan 84 is 2.484GHz to 2.489GHz)
const uint8_t retryDelay = 7;               // this is based on 250us increments, 0 is 250us so 7 is 2 ms
const uint8_t numRetries = 5;               // number of retries that will be attempted

const uint8_t CE_pin = 8;                   // This pin is used to set the nRF24 to standby (0) or active mode (1)
const uint8_t CSN_pin = 9;                  // This pin is used for SPI comm chip select

const float VccMin   = 1.8;                 // Minimum expected Vcc level, in Volts. (0%)
const float VccMax   = 3.2;                 // Maximum expected Vcc level, in Volts. (100%)
const float VccCorrection = 1.0/1.0;        // Measured Vcc by multimeter divided by reported Vcc

const int sleepDuration = 10800;            // Sleep duration to report battery (8 Sec x number of times)(10800 for a day)
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
const int SENSORTYPE  = 0;
const int FOBNUMBER   = 1;
const int ALIVE       = 2;
const int BATTERY     = 3;
const int SWITCHONE   = 4;
const int SWITCHTWO   = 5;
const int RESERVED    = 6;

const int PAYLOAD_LENGTH = 7;

char send_payload[PAYLOAD_LENGTH];

Vcc vcc(VccCorrection);                     // create VCC object
RF24 rfRadio(CE_pin, CSN_pin);              // Declare object from nRF24 library (Create your wireless SPI)
//----------------------------------------------------------------------------------------------------
void setup() {

  send_payload[SENSORTYPE]  = 'K';             // Sensor type [D:Door, T:Temerature, K:Keyfob, S:RF Switch etc]
  send_payload[FOBNUMBER]   = '1';             // Sensor number[1: Onkar's FOB, 2:Poorvi's FOB, 3:One button SW, etc]
  send_payload[ALIVE]       = (char) 0;        // Alive counter, 0 - 255 (Count will be increased and data will be sent)
  send_payload[BATTERY]     = (char) 100;      // Battery status, 0-100%
  send_payload[SWITCHONE]   = (char) 0;        // Key press count, 0 - 255 (Count will be increased and data will be sent)
  send_payload[SWITCHTWO]   = (char) 0;        // Key press count, 0 - 255 (count will overflow after 255 -> 0)
  send_payload[RESERVED]    = (char) 0;        // Reserved
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(switch_one_pin, INPUT);
  digitalWrite(switch_one_pin, HIGH);
  attachInterrupt(digitalPinToInterrupt(switch_one_pin), switchOne_ISR, FALLING);

  pinMode(switch_two_pin, INPUT);
  digitalWrite(switch_two_pin, HIGH);
  attachInterrupt(digitalPinToInterrupt(switch_two_pin), switchTwo_ISR, FALLING);

  #ifdef debug
    Serial.begin(9600);                       //serial port to display received data
    Serial.println("Keyfob is online...");
  #endif

  send_payload[BATTERY] = (char) vcc.Read_Perc(VccMin, VccMax);
  sendData();                                   // send battery data to server
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
void powerUpNRF()
{
  rfRadio.begin();                              //Start the nRF24 module
  rfRadio.setPALevel(RF24_PA_LOW);              //Set low power for RF24
  rfRadio.setChannel(rfChannel);                //set communication frequency channel
  rfRadio.setRetries(retryDelay,numRetries);    //if a transmit fails to reach receiver (no ack packet) then this sets retry attempts and delay between retries   
  rfRadio.openWritingPipe(pipeAddress);         //open writing or transmit pipe
  rfRadio.stopListening();                      //go into transmit mode

  delay(100);
}

//----------------------------------------------------------------------------------------------------
void powerDownNRF()
{
  rfRadio.powerDown();
  delay(50);
}
//----------------------------------------------------------------------------------------------------
void sendData()
{
  powerUpNRF();
  
  if (!rfRadio.write(send_payload, PAYLOAD_LENGTH))
  #ifdef debug
    {  //send data and remember it will retry if it fails
      Serial.println("Sending failed, check network");
    }
    else
    {
      Serial.println("Sending successful, data sent");
    }
  #else
    {
    }
  #endif

  powerDownNRF();
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
        Serial.println("Switch one ISR triggered");
        delay(200);
        yield();
      }
      else if(switchTwoStatus)
      {
        Serial.println("Switch two ISR triggered");
        delay(200);
        yield();
      }
      yield();
    #endif
    
    if(switchOneStatus)
    {
      send_payload[SWITCHONE]  = (char) ++switchOneCoutner;
      switchOneStatus = false;
    }

    if(switchTwoStatus)
    {
      send_payload[SWITCHTWO]  = (char) ++switchTwoCoutner;
      switchTwoStatus = false;
    }
    sendData();

    interrupts();
  }

  else if(aliveDurationCounter > aliveDuration)
  {
    #ifdef debug
      Serial.println("Sending Alive status...");
      delay(200);
      yield();
    #endif
    send_payload[ALIVE]  = (char) ++aliveCoutner;
    aliveDurationCounter = 0;
    sendData();
  }
  
  else if(sleepCounter > sleepDuration)
  {
    #ifdef debug
      Serial.println("Sending battery status...");
      delay(200);
      yield();
    #endif
    delay(200);
    send_payload[BATTERY] = (char) vcc.Read_Perc(VccMin, VccMax);
    sendData();
    sleepCounter = 0;
    delay(50);
  }
  
  else
  {
    Serial.println("Going to sleep...");
    delay(200);
    yield();
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
  sleepCounter++;
  aliveDurationCounter++;
}
