//----------------------------EEPROM---------------------

#include <EEPROM.h>

const int eepromAddress = 0;

//----------------------------define pins----------------------------

const int startButton = D3;
const int stopButton = D4;
const int wallFan = D7;
const int wallCompressor = D6;
const int buildingFan = D5;
const int buildingCompressor = D0;

//------------------------------i2c LCD-------------------------------

#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,16,2);

//---------------------------------HTU21D---------------------------------

#include <Wire.h>
#include "Adafruit_HTU21DF.h"

Adafruit_HTU21DF htu = Adafruit_HTU21DF();

//----------------------------Global Variables----------------------------

//temp related
float temp; 
float rel_hum;
int targetTemp = 78;

//status variables
int fanStatus = 0;
int compressorStatus = 0;
int airConditionerStatus = 0;

//timers
int countdown;
const int countdownStart = 7200; // in seconds
int delayTimer = 300; //in seconds
int fanTime = 0;
int compressorTime = 0;
int compressorDelay = 300; // in seconds
int fanDelay = 300; // in seconds
long totalRunTime;
const int loopTime = 854; //in milliseconds
int writeTimer = 0;
long storedRunTime;
int writeTriggerTime = 400;
int endCountdownWrite = 0;

//----------------------------interupt functions-------------------------

void ICACHE_RAM_ATTR stopState();
void stopState() {
    countdown = 0;
}

void ICACHE_RAM_ATTR startState();
void startState() {
    countdown = countdownStart;
}

//----------------------------LCD Display-------------------------

void lcdDisplay() {
    if(countdown == 7200 && delayTimer == 0){
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("SET: ");
        lcd.setCursor(4,0);
        lcd.print(targetTemp);
        lcd.setCursor(7,0);
        lcd.print("NOW: ");
        lcd.setCursor(11,0);
        lcd.print(temp,1);
        lcd.setCursor(0,1);
        lcd.print("REMAINING: ");
        lcd.setCursor(11,1);
        lcd.print(countdown);
        lcd.setCursor(15,1);
        lcd.print("s");
    }
    if(countdown < 7200 && countdown > 0 && delayTimer == 0){
        lcd.setCursor(11,0);
        lcd.print("     ");
        lcd.setCursor(11,0);
        lcd.print(temp,1);
        lcd.setCursor(11,1);
        lcd.print("    ");
        lcd.setCursor(11,1);
        lcd.print(countdown);
    }
    if(countdown == 0 || delayTimer > 0){
        lcd.setCursor(0,0);
        lcd.print("TEMP:   HUMID:       ");
        lcd.setCursor(5,0);
        lcd.print(temp,0);
        lcd.setCursor(14,0);
        lcd.print(rel_hum,0);
        if(compressorTime == 0 && delayTimer == 0){
            lcd.setCursor(0,1);
            lcd.print("AC OFF   RT:    ");
        }
        if(compressorTime > 0){
            lcd.setCursor(0,1);
            lcd.print("DELAY:   RT:    ");
            lcd.setCursor(6,1);
            lcd.print(compressorTime);
        }
        if(delayTimer > 0){
            lcd.setCursor(0,1);
            lcd.print("DELAY:   RT:    ");
            lcd.setCursor(6,1);
            lcd.print(delayTimer);
        }
        if(totalRunTime/3600 < 16){
            lcd.setCursor(12,1);
            lcd.print(totalRunTime/60);
            lcd.setCursor(15,1);
            lcd.print("M");
        }else{
            lcd.setCursor(12,1);
            lcd.print(totalRunTime/3600);
            lcd.setCursor(15,1);
            lcd.print("H");
        }
        
    }
  
}

//--------------------------------Setup-----------------------------------

void setup() {
    //serial
    Serial.begin(115200);
    Serial.println("Start");
  
    //temp sensor
    if (!htu.begin()) {
        Serial.println("Couldn't find sensor!");
        while (1){
            delay(1000);
            lcd.init();                     
            lcd.backlight();
            lcd.clear();
            lcd.setCursor(2,0);
            lcd.print("TEMP SENSOR");
            lcd.setCursor(3,1);
            lcd.print("NOT FOUND");
        }
    }
    
  
    //buttons and interupts
    pinMode(stopButton, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(stopButton), stopState, CHANGE);
    pinMode(startButton, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(startButton), startState, CHANGE);

    //lcd
    lcd.init();                     
    lcd.backlight();
    lcd.clear();
  
    //Set pin mode
    pinMode(wallFan, OUTPUT);
    pinMode(wallCompressor, OUTPUT);
    pinMode(buildingFan, OUTPUT);
    pinMode(buildingCompressor, OUTPUT);
  
    //turn off all relays
    digitalWrite(wallFan, HIGH);
    digitalWrite(wallCompressor, HIGH);
    digitalWrite(buildingFan, HIGH);
    digitalWrite(buildingCompressor, HIGH);

    //EEPROM
    EEPROM.begin(512);
    EEPROM.get(eepromAddress, storedRunTime);
    if(storedRunTime <= 0){
        EEPROM.put(eepromAddress, 10);
        EEPROM.commit();
        Serial.println("wrote to eeprom");
    }
    totalRunTime = storedRunTime;
    compressorTime = delayTimer; 
}

//---------------------------------Main Loop--------------------------------

void loop() {
    delay(loopTime);
    
    //---------Read Temp-------

    temp = (((htu.readTemperature()*9)/5)+32);
    rel_hum = htu.readHumidity();

    //--------Display Status on LCD-----------
  
    lcdDisplay();
  
    //decrement fan time
    if (fanTime > 0){
        fanTime--;
    }
      
    //decrement compressor time
    if (compressorTime > 0){
        compressorTime--;
    }

    //decrement delay time
    if (delayTimer > 0){
        delayTimer--;
       return;
    }

    //decrement countdown
    if(countdown > 0 && delayTimer == 0){
        countdown--;
    }

    //compare temperature
    if (targetTemp < temp){
        if (countdown > 0 && delayTimer == 0){
            airConditionerStatus = 1;
        }
    }else{
        airConditionerStatus = 0;
    }

    //turn on the air conditioner
    if (1 == airConditionerStatus && countdown > 0 && 0 == compressorTime && compressorStatus == 0){
        digitalWrite(wallFan, LOW);
        digitalWrite(wallCompressor, LOW);
        digitalWrite(buildingFan, LOW);
        digitalWrite(buildingCompressor, LOW);
        compressorTime = (compressorDelay);
        fanStatus = 1;
        compressorStatus = 1;
        Serial.println("AC ON");
    }

    //turn off the compressor and start the timer
    if (0 == compressorTime && 1 == compressorStatus && 0 == airConditionerStatus){
        digitalWrite(wallCompressor, HIGH);
        digitalWrite(buildingCompressor, HIGH);
        compressorStatus = 0;
        compressorTime = (compressorDelay);
        fanTime = (fanDelay);
        Serial.println("Compressor OFF");
    }
      
    //turn off the fan
    if (0 == compressorStatus && 0 == fanTime && 1 == fanStatus && 0 == airConditionerStatus){
        digitalWrite(wallFan, HIGH);
        digitalWrite(buildingFan, HIGH);
        fanStatus = 0; 
        Serial.println("Fan OFF");
        if(endCountdownWrite == 1){
            writeTimer = 0;
            endCountdownWrite == 0;
            EEPROM.put(eepromAddress, totalRunTime);
            EEPROM.commit();
        }
    }
      
    //start the shutdown process when the 2 hours is over
    if (0 == countdown){
        airConditionerStatus = 0;
        endCountdownWrite = 1;
        Serial.println("Out of Time. Please Insert Coin to Continue");
    }
       
    //add runtime to EEPROM
    if(fanStatus == 1){
        writeTimer++;
        totalRunTime++;
        if(writeTimer == writeTriggerTime){
            writeTimer = 0;
            EEPROM.put(eepromAddress, totalRunTime);
            EEPROM.commit();
        }    
    }

    EEPROM.get(eepromAddress, storedRunTime);
    //display all variables
    Serial.print("Temp: "); Serial.print(temp); Serial.println("F");
    Serial.print("Humidity: "); Serial.print(rel_hum); Serial.println("%");
    Serial.print("Countdown: ");Serial.print(countdown);Serial.println("s");
    Serial.print("AC Status: ");Serial.println(airConditionerStatus);
    Serial.print("Compressor Status: ");Serial.println(compressorStatus);
    Serial.print("Compressor Time: ");Serial.print(compressorTime);Serial.println("s");
    Serial.print("Fan Status: ");Serial.println(fanStatus);
    Serial.print("Fan Time: ");Serial.print(fanTime);Serial.println("s");
    Serial.print("Total Run Time: ");Serial.print(totalRunTime);Serial.print("s  or ");Serial.print(totalRunTime/3600);Serial.println("h");
    Serial.print("Write Timer: ");Serial.print(writeTimer);Serial.println("s");
    Serial.print("EEPROM Value: ");Serial.println(storedRunTime);
    Serial.println("-----------------------------------------------");
}
