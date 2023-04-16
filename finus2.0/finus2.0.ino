//generalized wave freq detection with 38.5kHz sampling rate and interrupts
//by Amanda Ghassaei
//https://www.instructables.com/id/Arduino-Frequency-Detection/
//Sept 2012

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
*/

#include <LiquidCrystal.h>
#include <string.h>
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

float noteFreqs[6] = {77.78, 103.83, 138.59, 185.00, 233.08, 311.13 };
char strings[6] = {'E', 'A', 'D', 'G', 'B', 'H'};

//clipping indicator variables
boolean clipping = 0;
int counter = 0;
int sum = 0;
int avg = 0;
//data storage variables
byte newData = 0;
byte prevData = 0;
unsigned int time = 0;//keeps time and sends vales to store in timer[] occasionally
int timer[10];//sstorage for timing of events
int slope[10];//storage for slope of events
unsigned int totalTimer;//used to calculate period
unsigned int period;//storage for period of wave
byte index = 0;//current storage index
float frequency;//storage for frequency calculations
int maxSlope = 0;//used to calculate max slope as trigger point
int newSlope;//storage for incoming slope data

//variables for decided whether you have a match
byte noMatch = 0;//counts how many non-matches you've received to reset variables if it's been too long
byte slopeTol = 3;//slope tolerance- adjust this if you need
int timerTol = 10;//timer tolerance- adjust this if you need

//variables for amp detection
unsigned int ampTimer = 0;
byte maxAmp = 0;
byte checkMaxAmp;
byte ampThreshold = 30;//raise if you have a very noisy signal

float getCents(float freq, float targetFreq) {
  return 1200 * (log(freq / targetFreq) / log(2));
}

void getNote(float freq) {
  int left = 0, right = 5;

  while (left <= right) {
    int mid = (left + right) / 2;
    if (noteFreqs[mid] == freq) {
      Serial.println(strings[mid]);
    } else if (noteFreqs[mid] < freq) {
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }
  if (abs(noteFreqs[left] - freq) > abs(noteFreqs[right] - freq)) {
    float targetFreq = noteFreqs[right];
    Serial.print(strings[right]);
    targetFreq - freq < 0 ? Serial.println(", +" + String(getCents(freq, targetFreq)) + " cents") : Serial.println(", " + String(getCents(freq, targetFreq)) + " cents");

    // PRINT TO LCD
    lcd.setCursor(7, 0);
    lcd.print(strings[right]);
    lcd.setCursor(0, 1);
    targetFreq - freq < 0 ? lcd.print("+" + String(getCents(freq, targetFreq)) + " cents") : lcd.print(String(getCents(freq, targetFreq)) + " cents");
  } else {
    float targetFreq = noteFreqs[left];
    Serial.print(strings[left]);
    targetFreq - freq < 0 ? Serial.println(", +" + String(getCents(freq, targetFreq)) + " cents") : Serial.println(", " + String(getCents(freq, targetFreq)) + " cents");

    // PRINT TO LCD
    lcd.setCursor(7, 0);
    lcd.print(strings[left]);
    lcd.setCursor(0, 1);
    targetFreq - freq < 0 ? lcd.print("+" + String(getCents(freq, targetFreq)) + " cents") : lcd.print(String(getCents(freq, targetFreq)) + " cents");
  }
}

void setup(){
  
  Serial.begin(9600);
  lcd.begin(16, 2);
  lcd.clear();
  // lcd.print("hello");
  // pinMode(13,OUTPUT);//led indicator pin
  // pinMode(12,OUTPUT);//output pin
  
  cli();//diable interrupts
  
  //set up continuous sampling of analog pin 0 at 38.5kHz
 
  //clear ADCSRA and ADCSRB registers
  ADCSRA = 0;
  ADCSRB = 0;
  
  // Serial.println(REFS0);
  // analogReference(EXTERNAL);
  // ADMUX |= (1 << REFS0); //set reference voltage
  ADMUX |= (1 << ADLAR); //left align the ADC value- so we can read highest 8 bits from ADCH register only
  // Serial.println(ADMUX);
  
  ADCSRA |= (1 << ADPS2) | (1 << ADPS0); //set ADC clock with 32 prescaler- 16mHz/32=500kHz
  ADCSRA |= (1 << ADATE); //enabble auto trigger
  ADCSRA |= (1 << ADIE); //enable interrupts when measurement complete
  ADCSRA |= (1 << ADEN); //enable ADC
  ADCSRA |= (1 << ADSC); //start ADC measurements
  
  sei();//enable interrupts
}

ISR(ADC_vect) {//when new ADC value ready
  
  PORTB &= B11101111;//set pin 12 low
  prevData = newData;//store previous value
  newData = ADCH;//get value from A0
  if (prevData < 127 && newData >=127){//if increasing and crossing midpoint
    newSlope = newData - prevData;//calculate slope
    if (abs(newSlope-maxSlope)<slopeTol){//if slopes are ==
      //record new data and reset time
      slope[index] = newSlope;
      timer[index] = time;
      time = 0;
      if (index == 0){//new max slope just reset
        PORTB |= B00010000;//set pin 12 high
        noMatch = 0;
        index++;//increment index
      }
      else if (abs(timer[0]-timer[index])<timerTol && abs(slope[0]-newSlope)<slopeTol){//if timer duration and slopes match
        //sum timer values
        totalTimer = 0;
        for (byte i=0;i<index;i++){
          totalTimer+=timer[i];
        }
        period = totalTimer;//set period
        //reset new zero index values to compare with
        timer[0] = timer[index];
        slope[0] = slope[index];
        index = 1;//set index to 1
        PORTB |= B00010000;//set pin 12 high
        noMatch = 0;
      }
      else{//crossing midpoint but not match
        index++;//increment index
        if (index > 9){
          reset();
        }
      }
    }
    else if (newSlope>maxSlope){//if new slope is much larger than max slope
      maxSlope = newSlope;
      time = 0;//reset clock
      noMatch = 0;
      index = 0;//reset index
    }
    else{//slope not steep enough
      noMatch++;//increment no match counter
      if (noMatch>9){
        reset();
      }
    }
  }
    
  if (newData == 0 || newData == 1023){//if clipping
    PORTB |= B00100000;//set pin 13 high- turn on clipping indicator led
    clipping = 1;//currently clipping
  }
  
  time++;//increment timer at rate of 38.5kHz
  
  ampTimer++;//increment amplitude timer
  if (abs(127-ADCH)>maxAmp){
    maxAmp = abs(127-ADCH);
  }
  if (ampTimer==1000){
    ampTimer = 0;
    checkMaxAmp = maxAmp;
    maxAmp = 0;
  }
  
}

void reset(){//clea out some variables
  index = 0;//reset index
  noMatch = 0;//reset match couner
  maxSlope = 0;//reset slope
}


void checkClipping(){//manage clipping indicator LED
  if (clipping){//if currently clipping
    PORTB &= B11011111;//turn off clipping indicator led
    clipping = 0;
  }
}


void loop(){
  
  // checkClipping();
  
  if (checkMaxAmp>ampThreshold){
    frequency = 38462/float(period);//calculate frequency timer rate/period
    // Serial.print(frequency);
    // Serial.println(" HZ");      
    sum = sum + abs(frequency);
    counter++;
    if (counter == 10000) {
      avg = sum / 10000;
      Serial.print(avg);
      Serial.println(" hz");      
      // getNote(avg);
      sum = 0;
      counter = 0;
    }
    // lcd.clear();
    
    //print results
    // Serial.print(frequency);
    // Serial.println(" hz");
  }
  //do other stuff here
}