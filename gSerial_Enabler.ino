/*
* gSerial Enabler v0.1
* Copyright (C) 2025 @Donutswdad
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
////////////////////
//    OPTIONS    //
//////////////////
*/

uint8_t debugG = 0; // line ~179

uint16_t const offset = 0; // Only needed for multiple gSerial Enablers. Set offset so 2nd, 3rd, etc gSEs don't overlap profiles. (e.g. offset = 8;) 

bool S0 = false;        // (Profile 0) 
                         //  ** Recommended to leave this option "false" if using in tandem with the Scalable Video Switch. AKA Leave JP1 trace un-cut**

////////////////////
                       
// gscart / gcomp adjustment variables for port detection
float const highsamvolt = 3.1; // rise above this voltage for a high sample
byte const apin[3] = {A0,A1,A2}; // defines analog pins used to read bit0, bit1, bit2 respectively
uint8_t const dch = 15; // (duty cycle high) at least this many high samples per "samsize" for a high bit (~75% duty cycle)
uint8_t const dcl = 5; // (duty cycle low) at least this many high samples and less than "dch" per "samsize" indicate all inputs are in-active (~50% duty cycle)
uint8_t const samsize = 20; // total number of ADC samples required to capture at least 1 period
uint8_t const fpdccountmax = 3; // number of periods required when in the 50% duty cycle state before a Profile 0 is triggered.


// gscart / gcomp Global variables
uint8_t fpdcprev = 0; // stores 50% duty cycle detection
uint8_t fpdccount = 0; // number of times a 50% duty cycle period has been detected
uint8_t samcc = 0; // ADC sample cycle counter
uint8_t highcount[3] = {0,0,0}; // number of high samples recorded for bit0, bit1, bit2
uint8_t bitprev[3] = {0,0,0}; // stores previous bit state


void setup(){

    Serial.begin(9600); // set the baud rate for the RT4K Serial Connection
    pinMode(A2,INPUT); // set A2 as bit2 input
    pinMode(A1,INPUT); // set A1 as bit1 input
    pinMode(A0,INPUT); // set A0 as bit0 input
    pinMode(12,INPUT_PULLUP);
    delay(100);
    if(digitalRead(12) == HIGH) S0 = true; // check state of JP1 jumper

} // end of setup

void loop(){

  readGscart(); // reads A0,A1,A2 pins to see which port, if any, are active

} // end of loop()

void readGscart(){

  // https://shmups.system11.org/viewtopic.php?p=1307320#p1307320
  // gscartsw_lite EXT pinout:
  // Pin 1: GND
  // Pin 2: Override
  // Pin 3: N/C
  // Pin 4: +5V
  // Pin 5: IN_BIT0
  // Pin 6: IN_BIT1
  // Pin 7: IN_BIT2
  // Pin 8: N/C
  //
  // Reference for no active input state: https://shmups.system11.org/viewtopic.php?t=50851&start=4976
  //
  // When no input is active, a 50% off/on cycle can be seen for each pin as it cycles through the inputs. Pin5 the fastest, Pin7 the slowest. If we can monitor each
  // pin then we can detect when there is no active input.
  //
  //        Pin7 Pin6 Pin5
  // Port1  0     0     0
  // Port2  0     0     1
  // Port3  0     1     0
  // Port4  0     1     1 
  // Port5  1     0     0
  // Port6  1     0     1
  // Port7  1     1     0
  // Port8  1     1     1

  uint8_t fpdc = 0; // 1 = 50% duty cycle was detected / all ports are in-active
  uint8_t bit[3] = {0,0,0};
  float val[3] = {0,0,0};
  

  for(uint8_t i=0;i<3;i++){ // read in analog pin voltages, read each pin 4x in a row to ensure an accurate read, combats if using too large of a pull-down resistor
    for(uint8_t j=0;j<4;j++){
      val[i] = analogRead(apin[i]);
    }
    if((val[i]/211) >= highsamvolt){ // if voltage is greater than or equal to the voltage defined for a high, increase highcount by 1 for that analog pin
      highcount[i]++;
    }
  }


  if(samcc == samsize){               // when the "samsize" number of samples has been taken, if the voltage was "high" for more than "dch" # of the "samsize" samples, set the bit to 1
    for(uint8_t i=0;i<3;i++){
      if(highcount[i] > dch)          // if the number of "high" samples per "samsize" are greater than "dch" set bit to 1.  
        bit[i] = 1;
      else if(highcount[i] > dcl)     // if the number of "high" samples is greater than "dcl" and less than "dch" (50% high samples) the switch must be cycling inputs, therefor no active input
        fpdc = 1;
    }
  }


  if(((bit[2] != bitprev[2] || bit[1] != bitprev[1] || bit[0] != bitprev[0])) && (samcc == samsize) && !(fpdc)){
    //Detect which scart port is now active and change profile accordingly
    if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 0)){ // 0 0 0
      sendSVS(201);
    } 
    else if((bit[2] == 0) && (bit[1] == 0) && (bit[0] == 1)){ // 0 0 1
      sendSVS(202);
    }
    else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 0)){ // 0 1 0
      sendSVS(203);
    }
    else if((bit[2] == 0) && (bit[1] == 1) && (bit[0] == 1)){ // 0 1 1
      sendSVS(204);
    }
    else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 0)){ // 1 0 0
      sendSVS(205);
    } 
    else if((bit[2] == 1) && (bit[1] == 0) && (bit[0] == 1)){ // 1 0 1
      sendSVS(206);
    }   
    else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 0)){ // 1 1 0
      sendSVS(207);
    } 
    else if((bit[2] == 1) && (bit[1] == 1) && (bit[0] == 1)){ // 1 1 1
      sendSVS(208);
    }
    
    bitprev[0] = bit[0];
    bitprev[1] = bit[1];
    bitprev[2] = bit[2];
    fpdcprev = fpdc;

  }


  if((fpdccount == (fpdccountmax - 1)) && (fpdc != fpdcprev) && (samcc == samsize)){ // if no active port has been detected for fpdccountmax periods
    
    memset(bitprev,0,sizeof(bitprev));
    fpdcprev = fpdc;

    if(S0){ // if trace has been cut, load S0_ SVS profile
      sendSVS(0);
    }

  }


  if(fpdc && (samcc == samsize)){ // if no active port has been detected, loop counter until active port
    if(fpdccount == (fpdccountmax - 1))
      fpdccount = 0;
    else 
      fpdccount++;
  }
  else if(samcc == samsize){
    fpdccount = 0;
  }

  if(debugG){ // debug
    delay(200);
    Serial.print(F("A0 voltage:         "));Serial.println(val[0]/211);
    Serial.print(F("A1 voltage:         "));Serial.println(val[1]/211);
    Serial.print(F("A2 voltage:         "));Serial.println(val[2]/211);
  }

  if(samcc < samsize) // increment counter until "samsize" has been reached then reset counter and "highcount"
    samcc++;
  else{
    samcc = 1;
    memset(highcount,0,sizeof(highcount));
  }

} // end readGscart()

void sendSVS(uint16_t num){
  Serial.print(F("SVS NEW INPUT="));
  Serial.print(num + offset);
  Serial.println(F("\r"));
  delay(1000);
  Serial.print(F("SVS CURRENT INPUT="));
  Serial.print(num + offset);
  Serial.println(F("\r"));
}
