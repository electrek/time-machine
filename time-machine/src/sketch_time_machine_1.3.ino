#define DEBUG
#define TIMER_DURATION_MINUTES 60
#define TIMER_DURATION_SECONDS 0

#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include <wavTrigger.h>

#define CONTROL_IDLE 1
#define CONTROL_TEST 2
#define CONTROL_ENGAGE_BREAKER 3
#define CONTROL_10SEC_COUNTDOWN_INIT 4
#define CONTROL_10SEC_COUNTDOWN 5
#define CONTROL_TIME_WARP_INIT 6
#define CONTROL_TIME_WARP_INIT2 7
#define CONTROL_TIME_WARP 8
#define CONTROL_START_TIMER 9
#define CONTROL_DISENGAGE_BREAKER 10
#define CONTROL_YEAR_INPUT 11
#define CONTROL_COUNTDOWN_INIT 12
#define CONTROL_COUNTDOWN 13
#define CONTROL_RUN 14
#define CONTROL_10SEC_COUNTDOWN_HOME_INIT 15
#define CONTROL_10SEC_COUNTDOWN_HOME 16
#define CONTROL_TIME_WARP_HOME_INIT 17
#define CONTROL_TIME_WARP_HOME_INIT2 18
#define CONTROL_TIME_WARP_HOME 19
#define CONTROL_LOSE 20

#define INPUT_SWITCH_VALUE 1
#define INPUT_SWITCH_DEBOUNCE 2
#define DEBOUNCE_TIME 50

// Set pins for nixie tubes
#define nixie_0_a 12    // was 14 (suspect bad GPIO)
#define nixie_0_b 15
#define nixie_0_c 16
#define nixie_0_d 17
#define nixie_1_a 38
#define nixie_1_b 40
#define nixie_1_c 42
#define nixie_1_d 44
#define nixie_2_a 18
#define nixie_2_b 19
#define nixie_2_c 20
#define nixie_2_d 21
#define nixie_3_a 22
#define nixie_3_b 24
#define nixie_3_c 26
#define nixie_3_d 28
#define nixie_4_a 30
#define nixie_4_b 32
#define nixie_4_c 34
#define nixie_4_d 36
#define nixie_5_a 46
#define nixie_5_b 48
#define nixie_5_c 50                                                                                                                                                                                                                                                                                                          
#define nixie_5_d 52

//anodes
#define TSEC_ONES 8
#define TSEC_TENS 5
#define TMIN_ONES 6
#define TMIN_TENS 7
#define CM_ONES 35
#define CM_TENS 37
#define CY_ONES 9
#define CY_TENS 25
#define CY_HUNS 31
#define CY_THOUS 33
#define TM_ONES 39
#define TM_TENS 41
#define TY_ONES 10
#define TY_TENS 11
#define TY_HUNS 23
#define TY_THOUS 27
#define CD_ONES 2
#define CD_TENS 3
#define TD_ONES 4
#define TD_TENS 29

#define lightvalve0 45
#define lightvalve1 43
#define lightvalve2 53
#define lightvalve3 51
#define lightvalve4 47
#define lightvalve5 49
#define switch0 A7
#define switch1 A6
#define switch2 A8
#define switch3 A9
#define mainbreaker A10
#define relay0 CANTX
#define relay1 CANRX

#define NUM_ADC_CHANNELS 5
#define NUM_SWITCHES 4
#define NUM_LIGHTVALVES 6

#define BOOTUNES A5

//Sound effects clips
#define POWERUP 12
#define UI_BEEP 1
#define COUNTDOWN1 2
#define TIMEWARP 3
#define POWERDOWN 4
#define CLICK1 5
#define CLICK2 6
#define CLICK3 7
#define COUNTDOWN2 8
#define WIN 9
#define LOSE 10
#define LOSE2 11
#define DOORUNLOCK 13
#define WARNING 14
#define AMBIENT 999

#define Serial SerialUSB  //Use SerialUSB for Native port, Serial for Programming port (comment out this define for programming port)

RTC_DS3231 rtc;

wavTrigger wTrig;

// REG_USART3_CR |= US_CR_RXDIS;  // disable RX for serial3



char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
char* controlNames[] = {"", "CONTROL_IDLE", "CONTROL_TEST", "CONTROL_ENGAGE_BREAKER", "CONTROL_10SEC_COUNTDOWN_INIT", "CONTROL_10SEC_COUNTDOWN", "CONTROL_TIME_WARP_INIT", "CONTROL_TIME_WARP_INIT2", "CONTROL_TIME_WARP", "CONTROL_START_TIMER", "CONTROL_DISENGAGE_BREAKER", "CONTROL_YEAR_INPUT", "CONTROL_COUNTDOWN_INIT", "CONTROL_COUNTDOWN", "CONTROL_RUN", "CONTROL_10SEC_COUNTDOWN_HOME_INIT", "CONTROL_10SEC_COUNTDOWN_HOME", "CONTROL_TIME_WARP_HOME_INIT", "CONTROL_TIME_WARP_HOME_INIT2", "CONTROL_TIME_WARP_HOME", "CONTROL_LOSE"};

const byte interruptPin = 13;
volatile byte state = LOW;
volatile boolean oneHzUpdate = false;

byte BCDoutputA[] = {nixie_0_a,nixie_1_a,nixie_2_a,nixie_3_a,nixie_4_a,nixie_5_a};
byte BCDoutputB[] = {nixie_0_b,nixie_1_b,nixie_2_b,nixie_3_b,nixie_4_b,nixie_5_b};
byte BCDoutputC[] = {nixie_0_c,nixie_1_c,nixie_2_c,nixie_3_c,nixie_4_c,nixie_5_c};
byte BCDoutputD[] = {nixie_0_d,nixie_1_d,nixie_2_d,nixie_3_d,nixie_4_d,nixie_5_d};

//                         group 0  group 1  group 2    group 3    group 4    group 5                               
byte anodegroups[4][6] = {{TSEC_ONES,     0,       0, TSEC_TENS, TMIN_ONES, TMIN_TENS},   // mux timeslot 0
						{  CY_ONES,       0,       0,   CY_TENS,   CY_HUNS,  CY_THOUS},   // mux timeslot 1
						{  TY_ONES, TY_TENS, TY_HUNS,  TY_THOUS,   CM_ONES,   CM_TENS},   // mux timeslot 2
						{  CD_ONES, CD_TENS, TD_ONES,   TD_TENS,   TM_ONES,   TM_TENS}};  // mux timeslot 3


//(readable) values for all nixies					
int timer_min = 0;
int timer_sec = 0;
int current_year = 0;
int current_month = 0;
int current_day = 0;
int target_year = 0;
int target_month = 0;
int target_day = 0;
int final_sec = 0;   // Final time sec
int final_min = 0;   // Final time min

// Nixie values from base values
byte tsec_tens_val = timer_sec/10;
byte tsec_ones_val = timer_sec % 10;
byte tmin_tens_val = timer_min/10;
byte tmin_ones_val = timer_min % 10;
byte cy_thous_val = current_year/1000;
byte cy_huns_val = (current_year % 1000)/100;
byte cy_tens_val =  (current_year % 100)/10;
byte cy_ones_val = current_year % 10;
byte cm_tens_val = current_month/10;
byte cm_ones_val = current_month % 10;
byte cd_tens_val = current_day/10;
byte cd_ones_val = current_day % 10;
byte ty_thous_val = target_year/1000;
byte ty_huns_val = (target_year % 1000)/100;
byte ty_tens_val = (target_year % 100)/10;
byte ty_ones_val = target_year % 10;
byte td_tens_val = target_day/10;
byte td_ones_val = target_day % 10;
byte tm_tens_val = target_month/10;
byte tm_ones_val = target_month % 10;

byte TS = 0;  //Timeslot
unsigned long ul_PreviousMicros = 0UL;
unsigned long ul_PreviousMillis = 0UL;
unsigned long ul_PreviousMillisFlash = 0UL;
unsigned long ul_MuxIntervalMicros = 3000UL; // 4ms (4000us) mux interval (each timeslot)
unsigned long ul_BlankingIntervalMicros = 300UL;  //blanking time (300us)
unsigned long ul_1secIntervalMillis = 1000UL;  //1 sec interval
unsigned long ul_FlashIntervalMillis = 500UL;  //500 msec interval
unsigned long ul_250msecIntervalMillis = 250UL;  //250 msec interval
unsigned long ul_100msecIntervalMillis = 100UL;  //100 msec interval
unsigned long ul_10msecIntervalMillis = 10UL;  //10 msec interval
boolean updating = true;
boolean blanking = false;

boolean timer_update = false;
boolean fastTime = true; // should default to false
boolean flashing = false;
boolean flash_time_only = false;
boolean flash_state = true;  //  state of flashing, true = Nixies are on (normal), false = off
boolean rand_en = false;
boolean first_time = true;

int ADCval0 = 0;
int ADCval1 = 0;
int valCumulative = 0;
byte valCounter = 0;
int ADCValues[NUM_ADC_CHANNELS];
int ADCValuesOld[NUM_ADC_CHANNELS];
int ADCCounter = 0;
int ADCPins[] = {A0, A1, A2, A3, A4};
byte ADCChannel = 0;
int ADCGuardBand = 25;
int lightvalveSum = 0;
int lightvalveSumTarget = 54;
int knobTargets[] = {90, 556, 370, 840 , 1023};
boolean knobValuesGood = false;

byte switches[] = {switch0, switch1, switch2, switch3};
byte breakerswitch = mainbreaker;
byte lightvalves[] = {lightvalve0, lightvalve1, lightvalve2, lightvalve3, lightvalve4, lightvalve4};
byte doorrelay0 = relay0;
byte doorrelay1 = relay1;
byte switchValues[NUM_SWITCHES];
byte switchValuesOld[NUM_SWITCHES];
byte switchCount[NUM_SWITCHES];
byte SWChannel = 0;
byte lightvalveValues[NUM_LIGHTVALVES];
byte lightvalveValuesOld[NUM_LIGHTVALVES];
byte LVChannel = 0;
byte breakerswitchValue;
unsigned long debounceTime = 10UL;
byte buttonState = 0;
byte control_state = CONTROL_TEST;
byte control_state_old = 0;
byte input_switch_state = INPUT_SWITCH_VALUE;
boolean readADC_en = false;
boolean readSwitches_en = false;
boolean readLightvalves_en = false;
int playtunes = BOOTUNES;
DateTime now;

//nixie data
byte nixiedata[4][6] = {{tsec_ones_val,           0,           0, tsec_tens_val, tmin_ones_val, tmin_tens_val},	  // mux timeslot 0
						{  cy_ones_val,           0,           0,   cy_tens_val,   cy_huns_val,  cy_thous_val},   // mux timeslot 1
						{  ty_ones_val, ty_tens_val, ty_huns_val,  ty_thous_val,   cm_ones_val,   cm_tens_val},   // mux timeslot 2
						{  cd_ones_val, cd_tens_val, td_ones_val,   td_tens_val,   tm_ones_val,   tm_tens_val}};  // mux timeslot 3

void setup()
{	
	int i=0;
	int j=0;
	for (i=0;i<=5;i++)
	{
		pinMode(BCDoutputA[i], OUTPUT);
		pinMode(BCDoutputB[i], OUTPUT);
		pinMode(BCDoutputC[i], OUTPUT);
		pinMode(BCDoutputD[i], OUTPUT);
		for (j=0;j<=3;j++)
		{
			pinMode(anodegroups[j][i], OUTPUT);
			digitalWrite(anodegroups[j][i], HIGH);  // Turn off anode voltage to all digits (HIGH to turn off)
		}
	}

	for (i=0;i<NUM_SWITCHES;i++)
	{
		pinMode(switches[i], INPUT_PULLUP);
		switchValues[i] = digitalRead(switches[i]);  // Read in initial switch values
	}
	
	for (i=0;i<NUM_LIGHTVALVES;i++)
	{
		pinMode(lightvalves[i], INPUT);
		lightvalveValues[i] = digitalRead(lightvalves[i]);  // Read in initial switch values
		lightvalveValuesOld[i] = lightvalveValues[i];
	}
		
	pinMode(breakerswitch, INPUT_PULLUP);
	breakerswitchValue = digitalRead(breakerswitch);
	pinMode(doorrelay0, OUTPUT);
	digitalWrite(doorrelay0, HIGH);    // High to de-energize relay
	pinMode(doorrelay1, OUTPUT);
	digitalWrite(doorrelay1, HIGH);    // High to de-energize relay
		
	for (i=0; i<NUM_ADC_CHANNELS; i++)
	{
		pinMode(ADCPins[i], INPUT);
		ADCValues[i] = analogRead(ADCPins[i]);
		ADCValuesOld[i] = ADCValues[i];
	}
	
	pinMode(playtunes, OUTPUT);
	digitalWrite(playtunes, LOW);
	
 // #ifdef DEBUG
     Serial.begin(9600);
     Serial.println("Howdy!");
 // #endif
  
	pinMode(interruptPin, INPUT);
	attachInterrupt(digitalPinToInterrupt(interruptPin), blink, RISING);
	
	if (! rtc.begin())
	{
		Serial.println("Couldn't find RTC");
//		while (1);
	}

	if (rtc.lostPower())
	{
		Serial.println("RTC lost power, lets set the time!");
		// following line sets the RTC to the date & time this sketch was compiled
		rtc.adjust(DateTime(DateTime(2016, 8, 27, 10, 0, 0)));
		// This line sets the RTC with an explicit date & time, for example to set
		// January 21, 2014 at 3am you would call:
		// rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
	}
	
	rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
	now = rtc.now();
	
	wTrig.start();  // WAV Trigger startup at 57600
	wTrig.stopAllTracks();
	wTrig.masterGain(0);   // Reset the master gain to 0dB
	//sfxtest();
	//nixietest();
	ul_PreviousMicros = micros();   //Get the current time in microseconds
	ul_PreviousMillis = millis();   //Get the current time in milliseconds
}

void loop()
{	
	#ifdef DEBUG
		if (control_state != control_state_old)
		{
			Serial.print("control_state = ");
			Serial.println(controlNames[control_state]);
			Serial.print("time = ");
			Serial.println(micros());
			control_state_old = control_state;
		}
	#endif
	control();
	blank();
	refresh();
	if (timer_update==true) {if (fastTime == true) {updateFastTimer();} else {updateTimer();}}
	if (readADC_en==true) {readADC();}
	if (readSwitches_en==true) {readSwitches();}
	if (readLightvalves_en==true) {readLightvalves();}
	if (flashing==true) {flash();}
}

void control()
{
	switch(control_state)
    {
    case CONTROL_IDLE:
        // nothing
        break;
	case CONTROL_TEST:
		// Start with a test sequence, cycling through all digits -- low priority
		control_state = CONTROL_ENGAGE_BREAKER;
		break;
	case CONTROL_ENGAGE_BREAKER:
    // Do nothing until the breaker gets engaged
		updateNixieFlashVals(0xF,0xF);
		if (digitalRead(breakerswitch)== LOW)  // LOW = engaged
		{
			delay(200);
			if (digitalRead(breakerswitch)== LOW)
			{
				wTrig.trackPlayPoly(POWERUP);
				delay(2000);
				control_state = CONTROL_10SEC_COUNTDOWN_INIT;
			}
		}
		break;  
	case CONTROL_10SEC_COUNTDOWN_INIT:
		// Set date values to Target: 9/1/1898, Current: now()
		// Countdown from 10.00 (sec) to zero
		
		timer_update = true;
		fastTime= true;
		timer_sec = 0;
		timer_min = 10;
		current_year = now.year();
		current_month = now.month();
		current_day = now.day();
		target_year = 1898;
		target_month = 9;
		target_day = 1;
		updateNixieTimerVals();
		updateNixieVals();
		readADC_en = false;
		readSwitches_en = false;
		readLightvalves_en = false;
		flashing = true;
		rand_en = false;
		ul_FlashIntervalMillis = 500UL;  //500 msec interval
		control_state = CONTROL_10SEC_COUNTDOWN;
		wTrig.trackPlayPoly(COUNTDOWN2);
		break;
	case CONTROL_10SEC_COUNTDOWN:
		// Set date values to random (flashing, 1Hz)
		// Countdown from 10.00 (sec) to zero
		if ((timer_sec == 0) & (timer_min == 0))
		{
			wTrig.stopAllTracks();
			timer_update = false;
			control_state = CONTROL_TIME_WARP_INIT;
		}
		break;
	case CONTROL_TIME_WARP_INIT:
		// Pause, then flash random everything (10 Hz?)
		timer_update = true;
		fastTime= true;
		timer_sec = 0;
		timer_min = 10;
		readADC_en = false;
		readSwitches_en = false;
		readLightvalves_en = false;
		flashing = false;
		rand_en = true;
		ul_FlashIntervalMillis = 50UL;  //50 msec interval
		updateNixieFlashVals(0x0f,0x0f);
		delay(10);
		control_state = CONTROL_TIME_WARP_INIT2;
		break;

  case CONTROL_TIME_WARP_INIT2:
      delay(1000);
	  wTrig.trackPlayPoly(TIMEWARP);
	  delay(1000);
      control_state = CONTROL_TIME_WARP;
      flashing = true;
      oneHzUpdate = false;
  break;
  
	case CONTROL_TIME_WARP:
		// Pause, then flash random everything (10 Hz?)
	if ((timer_sec == 0) & (timer_min == 0) & (oneHzUpdate = true))
		{
			control_state = CONTROL_START_TIMER;
			oneHzUpdate = false;
		}
		break;
	case CONTROL_START_TIMER:
		// Reset timer to 60:00 minutes
		// Set all dates to 00
		// Flash dates (1Hz)
		wTrig.trackPlayPoly(AMBIENT);
		wTrig.trackLoop(AMBIENT, true);   // turn looping on for ambient (repeats indefinitely)
		timer_update = true;
		fastTime= false;
		timer_sec = TIMER_DURATION_SECONDS;
		timer_min = TIMER_DURATION_MINUTES;
		current_year = 0;
		current_month = 0;
		current_day = 0;
		target_year = 0;
		target_month = 0;
		target_day = 0;
		readADC_en = false;
		readSwitches_en = false;
		readLightvalves_en = false;
		flashing = true;
		rand_en = false;
		ul_FlashIntervalMillis = 500UL;  //500 msec interval
		updateNixieTimerVals();
		updateNixieVals();
		int i;
		for (i=0;i<NUM_SWITCHES;i++)
		{
			switchValues[i] = digitalRead(switches[i]);  // Read in initial switch values
			switchValuesOld[i] = switchValues[i];
			switchCount[i] = 0;
		}
		control_state = CONTROL_DISENGAGE_BREAKER;
	break;
	case CONTROL_DISENGAGE_BREAKER:
		// Wait for breaker to be disengaged
		// When that happens, stop flashing
		if (digitalRead(breakerswitch)== HIGH)  // HIGH = disengaged
		{
			//wTrig.trackPlayPoly(CLICK1);
			flashing = true;
			readSwitches_en = true;
			control_state = CONTROL_YEAR_INPUT;
		}
	case CONTROL_YEAR_INPUT:
		// Wait for YEAR (1898) to be input via switches
		// Update YEAR nixies with switch inputs
		int current_year_new;
		current_year_new = 1000*(switchCount[0]%10) + 100*(switchCount[1]%10) + 10*(switchCount[2]%10) + (switchCount[3]%10);
		
		if (current_year != current_year_new)
		{
			current_year = current_year_new;
			updateNixieVals();
			wTrig.trackPlayPoly(UI_BEEP);
			#ifdef DEBUG
				Serial.print("current year = ");
				Serial.println(current_year);
			#endif
			if (current_year == 1898)
			{
				delay(250);
				wTrig.trackPlayPoly(DOORUNLOCK);
				digitalWrite(doorrelay0,LOW);  // Activate relay0, which opens door into library
				digitalWrite(playtunes,HIGH);  // Turn background music on (bootunes)
				control_state = CONTROL_COUNTDOWN_INIT;
			}
		}
		if ((timer_sec == 0) & (timer_min == 0))  //See if timer expires (they really did not do well)
		{
			final_sec = 0;
			final_min = 0;
			control_state = CONTROL_LOSE;			
		}
	break;
		
	case CONTROL_COUNTDOWN_INIT:
		// Wait for either
		// 1) Timer = 0 (game over)
		// 2) Codes are input
		timer_update = true;
		fastTime= false;
		readADC_en = true;
		readSwitches_en = false;
		readLightvalves_en = true;
		flashing = false;
		rand_en = false;
		control_state = CONTROL_COUNTDOWN;
	break;
		
	case CONTROL_COUNTDOWN:
		// Wait for either
		// 1) Timer = 0 (game over)
		// 2) Codes are input
		    #ifdef DEBUG
          SerialUSB.print("ADCValues = {");
        #endif
		for (i=0;i<NUM_ADC_CHANNELS;i++)
		{
        #ifdef DEBUG
          SerialUSB.print(ADCValues[i]);
          SerialUSB.print(" ");
        #endif
			if ((ADCValues[i] > (knobTargets[i] + ADCGuardBand)) | (ADCValues[i] < (knobTargets[i] - ADCGuardBand)))
			{
				knobValuesGood = false;
       #ifdef DEBUG
          SerialUSB.println("}");
          SerialUSB.println("ADCValues no good");
       #endif
        break;
			}
			knobValuesGood = true;
		}
		lightvalveSum = lightvalveValues[0] + lightvalveValues[1]*2 + lightvalveValues[2]*4 + lightvalveValues[3]*8 + lightvalveValues[4]*16 + lightvalveValues[5]*32;
		#ifdef DEBUG
       SerialUSB.print("lightvalueSum = ");
       SerialUSB.println(lightvalveSum);
    #endif
		if ((timer_sec == 0) & (timer_min == 0))  //Time ran out :(
		{
			final_sec = 0;
			final_min = 0;
			control_state = CONTROL_LOSE;
		}
		if ((lightvalveSum == lightvalveSumTarget) & (knobValuesGood == true) & (digitalRead(breakerswitch)== LOW))  // LOW = engaged)
		{
			final_sec = timer_sec;
			final_min = timer_min;
			control_state = CONTROL_10SEC_COUNTDOWN_HOME_INIT;
			wTrig.trackPlayPoly(POWERUP);
			delay(2000);
		}
		break;
		
	case CONTROL_10SEC_COUNTDOWN_HOME_INIT:
		// Set date values to random (flashing, 1Hz)
		// Countdown from 10.00 (sec) to zero
		timer_update = true;
		fastTime= true;
		current_year = 1898;
		current_month = 9;
		current_day = 1;
		target_year = now.year();
		target_month = now.month();
		target_day = now.day();
		timer_sec = 0;
		timer_min = 10;
		readADC_en = false;
		readSwitches_en = false;
		readLightvalves_en = false;
		flashing = true;
		rand_en = false;
		updateNixieTimerVals();
		updateNixieVals();
		ul_FlashIntervalMillis = 500UL;  //500 msec interval
		control_state = CONTROL_10SEC_COUNTDOWN_HOME;
		wTrig.trackPlayPoly(COUNTDOWN2);
		break;		
		
	case CONTROL_10SEC_COUNTDOWN_HOME:
		// Set date values to random (flashing, 1Hz)
		// Countdown from 10.00 (sec) to zero
		if ((timer_sec == 0) & (timer_min == 0))
		{
			timer_update = false;
			control_state = CONTROL_TIME_WARP_HOME_INIT;
		}
		break;
		
	case CONTROL_TIME_WARP_HOME_INIT:
		// Pause, then flash random everything (10 Hz?)
		timer_update = true;
		fastTime= true;
		timer_sec = 0;
		timer_min = 10;
		readADC_en = false;
		readSwitches_en = false;
		readLightvalves_en = false;
		flashing = false;
		rand_en = true;
		ul_FlashIntervalMillis = 50UL;  //50 msec interval
		updateNixieFlashVals(0x0f,0x0f);
		delay(10);
		control_state = CONTROL_TIME_WARP_HOME_INIT2;
		wTrig.stopAllTracks();
		break;

	case CONTROL_TIME_WARP_HOME_INIT2:
		delay(1000);
		control_state = CONTROL_TIME_WARP_HOME;
		delay(1000);
		flashing = true;
		oneHzUpdate = false;
		wTrig.trackPlayPoly(TIMEWARP);
	break;
  
	case CONTROL_TIME_WARP_HOME:
		// Yep, another time warp, though this time it ends up setting current date
		// Open door (doorrelay1)
		// Set current and target dates to (actual)

		if ((timer_sec == 0) & (timer_min == 0))
		{
			
			timer_update = false;
			fastTime= false;
			readADC_en = false;
			readSwitches_en = false;
			readLightvalves_en = false;
			flashing = true;
			flash_time_only = true;
			ul_FlashIntervalMillis = 500UL;  //500 msec interval
			rand_en = false;
			timer_sec = final_sec;
			timer_min = final_min;
			current_year = now.year();
			current_month = now.month();
			current_day = now.day();
			target_year = now.year();
			target_month = now.month();
			target_day = now.day();
			updateNixieTimerVals();
			updateNixieVals();
			control_state = CONTROL_IDLE;
			wTrig.trackPlayPoly(WIN);
			delay(4500);
			wTrig.trackPlayPoly(DOORUNLOCK);
			digitalWrite(doorrelay1,LOW);  //Opens escape door
			digitalWrite(playtunes, LOW);
			delay(2650);
			wTrig.stopAllTracks();
		}
		break;
	case CONTROL_LOSE:
		// Update final time (0,0)
		if ((timer_sec == 0) & (timer_min == 0))
		{
			timer_update = false;
			fastTime= false;
			readADC_en = false;
			readSwitches_en = false;
			readLightvalves_en = false;
			flashing = true;
			flash_time_only = true;
			ul_FlashIntervalMillis = 500UL;  //500 msec interval
			rand_en = false;
			timer_sec = final_sec;
			timer_min = final_min;
			current_year = 1898;
			current_month = 9;
			current_day = 1;
			target_year = 1898;
			target_month = 9;
			target_day = 1;
			updateNixieTimerVals();
			updateNixieVals();
			control_state = CONTROL_IDLE;
			wTrig.trackStop(AMBIENT);
			wTrig.trackPlayPoly(LOSE);
			digitalWrite(playtunes, LOW);
			delay(13000);
			wTrig.stopAllTracks();
		}
		break;
	default:
		control_state = CONTROL_IDLE;
		break;
	}
}

void readLightvalves()
{
	lightvalveValuesOld[LVChannel] = lightvalveValues[LVChannel];
	lightvalveValues[LVChannel] = digitalRead(lightvalves[LVChannel]);
	if (lightvalveValues[LVChannel] != lightvalveValuesOld[LVChannel])
	{
			wTrig.trackPlayPoly(CLICK2);
	}
		// #ifdef DEBUG
			// Serial.print("Lightvalve ");
			// Serial.print(LVChannel);
			// Serial.print(": value = ");
			// Serial.println(lightvalveValues[LVChannel]);
		// #endif
	LVChannel = (LVChannel + 1) % NUM_LIGHTVALVES;
	
}

void readSwitches()
{
	static unsigned long ts;
	
	switch(input_switch_state)
	{
	case INPUT_SWITCH_VALUE:
		switchValues[SWChannel] = digitalRead(switches[SWChannel]);
		if (switchValues[SWChannel] != switchValuesOld[SWChannel])   //check to see if switch was turned
		{
			ts = millis();
			input_switch_state = INPUT_SWITCH_DEBOUNCE;
		}
		else
		{
			SWChannel = (SWChannel + 1) % NUM_SWITCHES;
		}
	break;
	
	case INPUT_SWITCH_DEBOUNCE:
		switchValues[SWChannel] = digitalRead(switches[SWChannel]);
		if (switchValues[SWChannel] == switchValuesOld[SWChannel])   //check to see if switch value changed back
		{
			input_switch_state = INPUT_SWITCH_VALUE;
		}
		else if (millis() > ts + DEBOUNCE_TIME)
		{
			switchCount[SWChannel] = (switchCount[SWChannel] + 1) % 10;                                // increment switchCount after debounce time
			switchValuesOld[SWChannel] = switchValues[SWChannel];
			#ifdef DEBUG
				Serial.print("switchCount");
				Serial.print(SWChannel);
				Serial.print(" = ");
				Serial.println(switchCount[SWChannel]);
			#endif
			SWChannel = (SWChannel + 1) % NUM_SWITCHES;
		}
	break;
	}
}

void readADC()
{
	ADCValuesOld[ADCChannel] = ADCValues[ADCChannel];
	ADCValues[ADCChannel] = analogRead(ADCPins[ADCChannel]);
	if (((ADCValues[ADCChannel] - ADCValuesOld[ADCChannel]) > 15) | ((ADCValuesOld[ADCChannel] - ADCValues[ADCChannel]) > 15))
	{
	//	wTrig.trackPlayPoly(CLICK3);
	}
		
	// #ifdef DEBUG
		// Serial.print("ADC Channel ");
		// Serial.print(ADCChannel);
		// Serial.print(": value = ");
		// Serial.println(ADCValues[ADCChannel]);
	// #endif
	
	ADCChannel = (ADCChannel + 1) % NUM_ADC_CHANNELS;	
	
}

void flash()
{
	int i;
	unsigned long ul_CurrentMillisFlash = millis();
	if ((flashing == true) & (ul_CurrentMillisFlash - ul_PreviousMillisFlash > ul_FlashIntervalMillis)) //if true, time to toggle flash
	{
		// #ifdef DEBUG
			// Serial.print("Flashing. Time (millis) = ");
			// Serial.println(millis());
			// Serial.print("flash_state = ");
			// Serial.println(flash_state);
		// #endif
		flash_state = !flash_state;
		if (rand_en == true)
		{
			updateNixieRandVals(1,1);
		}
		else if (flash_time_only == true)
		{
			updateNixieFlashVals((flash_state*0xF), 0x0);
		}
		else
		{
			updateNixieFlashVals(0x0, (flash_state*0xF));
		}
		ul_PreviousMillisFlash = ul_CurrentMillisFlash;  //get the time (in milliseconds) for the next timeslot
	}
}

void updateFastTimer()
{
	int i;
	unsigned long ul_CurrentMillis = millis();
	if ((timer_update == true) & (ul_CurrentMillis - ul_PreviousMillis > ul_10msecIntervalMillis)) //if true, time to update timer
	{
		// #ifdef DEBUG
			// Serial.print("Updating timer.  Time (millis) =  ");
			// Serial.println(millis());
			// Serial.print("timer_min = ");
			// Serial.println(timer_min);
			// Serial.print("timer_sec = ");
			// Serial.println(timer_sec);
		// #endif
		//note that during this time, timer_sec has been re-defined at hundredths of seconds
		//and timer_min is now seconds
		if (timer_sec==0)
		{
			timer_sec = 99;
			timer_min--;
		}
		else
		{
			timer_sec--;
		}
		// if ((timer_min == 0) & (timer_sec == 0))
		// {
			// timer_update = false;    //stop at 00:00.00
		// }
		updateNixieTimerVals();
		ul_PreviousMillis = ul_CurrentMillis;  //get the time (in milliseconds) for the next timeslot
	}
}


void updateTimer()
{
	int i;
	unsigned long ul_CurrentMillis = millis();
	//if ((timer_update == true) & (ul_CurrentMillis - ul_PreviousMillis > ul_1secIntervalMillis)) //if true, time to update timer
	if ((timer_update == true) & (oneHzUpdate == true))
	{
		oneHzUpdate = false;
		// #ifdef DEBUG
			// Serial.print("Updating timer.  Time (millis) =  ");
			// Serial.println(millis());
			// Serial.print("timer_min = ");
			// Serial.println(timer_min);
			// Serial.print("timer_sec = ");
			// Serial.println(timer_sec);
		// #endif
		if (timer_sec==0)
		{
			timer_sec = 59;
			timer_min--;
		}
		else
		{
			timer_sec--;
		}
		if ((timer_min == 1) & (timer_sec == 0))  //For the last minute, go to ss.00 format
		{
			fastTime = true;
			wTrig.trackPlayPoly(WARNING);
			wTrig.trackLoop(WARNING, true);  // plays track WARNING continuously (loop enabled)
			timer_min = 60;
		}
		updateNixieTimerVals();
		ul_PreviousMillis = ul_CurrentMillis;  //get the time (in milliseconds) for the next timeslot
	}
}

void updateNixieTimerVals()   //Here we keep the timer value variables updated (e.g. tsec_ones_val, etc)
{
	tsec_tens_val = timer_sec/10;
	tsec_ones_val = timer_sec % 10;
	tmin_tens_val = timer_min/10;
	tmin_ones_val = timer_min % 10;
	nixiedata[0][0] = tsec_ones_val;
	nixiedata[0][3] = tsec_tens_val;
	nixiedata[0][4] = tmin_ones_val;
	nixiedata[0][5] = tmin_tens_val;
}

void updateNixieVals()    //Keep the date value variables updated
{
	cy_thous_val = current_year/1000;
	cy_huns_val = (current_year % 1000)/100;
	cy_tens_val =  (current_year % 100)/10;
	cy_ones_val = current_year % 10;
	cm_tens_val = current_month/10;
	cm_ones_val = current_month % 10;
	cd_tens_val = current_day/10;
	cd_ones_val = current_day % 10;
	ty_thous_val = target_year/1000;
	ty_huns_val = (target_year % 1000)/100;
	ty_tens_val = (target_year % 100)/10;
	ty_ones_val = target_year % 10;
	td_tens_val = target_day/10;
	td_ones_val = target_day % 10;
	tm_tens_val = target_month/10;
	tm_ones_val = target_month % 10;

	nixiedata[1][0] = cy_ones_val;
	nixiedata[1][3] = cy_tens_val;
	nixiedata[1][4] = cy_huns_val; 
	nixiedata[1][5] = cy_thous_val;  
	nixiedata[2][0] = ty_ones_val;  
	nixiedata[2][1] = ty_tens_val;   
	nixiedata[2][2] = ty_huns_val;   
	nixiedata[2][3] = ty_thous_val;
	nixiedata[2][4] = cm_ones_val; 
	nixiedata[2][5] = cm_tens_val;   
	nixiedata[3][0] = cd_ones_val;   
	nixiedata[3][1] = cd_tens_val;
	nixiedata[3][2] = td_ones_val; 
	nixiedata[3][3] = td_tens_val; 
	nixiedata[3][4] = tm_ones_val;
	nixiedata[3][5] = tm_tens_val;  
} 

void updateNixieFlashVals(byte timerFlashMask, byte dateFlashMask)    //simply mute (with 0xF (decimal 16)) when flashing is enabled and in off portion of cycle
{
	nixiedata[0][0] = tsec_ones_val | timerFlashMask;
	nixiedata[0][3] = tsec_tens_val | timerFlashMask;
	nixiedata[0][4] = tmin_ones_val | timerFlashMask;
	nixiedata[0][5] = tmin_tens_val | timerFlashMask;
	nixiedata[1][0] = cy_ones_val | dateFlashMask;
	nixiedata[1][3] = cy_tens_val | dateFlashMask;
	nixiedata[1][4] = cy_huns_val | dateFlashMask; 
	nixiedata[1][5] = cy_thous_val | dateFlashMask;  
	nixiedata[2][0] = ty_ones_val | dateFlashMask;  
	nixiedata[2][1] = ty_tens_val | dateFlashMask;   
	nixiedata[2][2] = ty_huns_val | dateFlashMask;   
	nixiedata[2][3] = ty_thous_val | dateFlashMask; 
	nixiedata[2][4] = cm_ones_val | dateFlashMask;   
	nixiedata[2][5] = cm_tens_val | dateFlashMask;   
	nixiedata[3][0] = cd_ones_val | dateFlashMask;   
	nixiedata[3][1] = cd_tens_val | dateFlashMask; 
	nixiedata[3][2] = td_ones_val | dateFlashMask; 
	nixiedata[3][3] = td_tens_val | dateFlashMask;   
	nixiedata[3][4] = tm_ones_val | dateFlashMask; 
	nixiedata[3][5] = tm_tens_val | dateFlashMask;   
} 

void updateNixieRandVals(byte timerFlashMask, byte dateFlashMask)    //simply mute (with 0xF (decimal 16)) when flashing is enabled and in off portion of cycle
{
	nixiedata[0][0] = tsec_ones_val ^ (timerFlashMask * random(9));
	nixiedata[0][3] = tsec_tens_val ^ (timerFlashMask * random(5));
	nixiedata[0][4] = tmin_ones_val ^ (timerFlashMask * random(9));
	nixiedata[0][5] = tmin_tens_val ^ (timerFlashMask * random(5));
	nixiedata[1][0] = cy_ones_val ^ (dateFlashMask * random(9));
	nixiedata[1][3] = cy_tens_val ^ (dateFlashMask * random(9));
	nixiedata[1][4] = cy_huns_val ^ (dateFlashMask * random(9));
	nixiedata[1][5] = cy_thous_val ^ (dateFlashMask * random(9));
	nixiedata[2][0] = ty_ones_val ^ (dateFlashMask * random(9));
	nixiedata[2][1] = ty_tens_val ^ (dateFlashMask * random(9));
	nixiedata[2][2] = ty_huns_val ^ (dateFlashMask * random(9));
	nixiedata[2][3] = ty_thous_val ^ (dateFlashMask * random(9));
	nixiedata[2][4] = cm_ones_val ^ (dateFlashMask * random(9));
	nixiedata[2][5] = cm_tens_val ^ (dateFlashMask * random(9));
	nixiedata[3][0] = cd_ones_val ^ (dateFlashMask * random(9));
	nixiedata[3][1] = cd_tens_val ^ (dateFlashMask * random(9));
	nixiedata[3][2] = td_ones_val ^ (dateFlashMask * random(9));
	nixiedata[3][3] = td_tens_val ^ (dateFlashMask * random(9));
	nixiedata[3][4] = tm_ones_val ^ (dateFlashMask * random(9));
	nixiedata[3][5] = tm_tens_val ^ (dateFlashMask * random(9));
} 

void nixietest()
{	
	int i=0;
	int j=0;
	int k=0;

	for (j=0;j<=3;j++)
	{
		for (i=0;i<=5;i++)
		{		
			if (anodegroups[j][i] != 0)
			{
				digitalWrite(anodegroups[j][i],LOW);	// Turn on anode voltage to all digits in mux timeslot j (LOW to turn on)
			}
		}		
		for (k=0; k<=9; k++)
		{
			for (i = 0; i <=5; i++)
			{
				digitalWrite(BCDoutputA[i], bitRead(k, 0));
				digitalWrite(BCDoutputB[i], bitRead(k, 1));
				digitalWrite(BCDoutputC[i], bitRead(k, 2));
				digitalWrite(BCDoutputD[i], bitRead(k, 3));
			}
		delay(250);
		}
		for (i=0;i<=5;i++)
		{	
			if (anodegroups[j][i] != 0)
			{	
				digitalWrite(anodegroups[j][i],HIGH);	// Turn off anode voltage to all digits in previous group (LOW to turn on)
			}
		}
		
	}
}

void blank()
{
	int i;
	unsigned long ul_CurrentMicros = micros();
	if ((updating == true) & (ul_CurrentMicros - ul_PreviousMicros > ul_MuxIntervalMicros)) //if true, the mux interval has ended
	{	
	// #ifdef DEBUG
		// Serial.print("Blanking.  Time = ");
		// Serial.println(micros());
	// #endif
		//First turn off the anodes that were on
		for (i=0;i<=5;i++)
			{		
				if (anodegroups[TS][i] != 0)   // Probably can remove this check (what happens if you attempt to write to a nonexistant pin?)
				{
					digitalWrite(anodegroups[TS][i],HIGH);	// Turn off anode voltage to all digits in mux timeslot TS (HIGH to turn on)
				}
			}
		//Advance the timeslot
		TS = (TS + 1) % 4;			
		//Next, write to the Nixie data lines (ABCD)
		for (i = 0; i <=5; i++)
			{
				digitalWrite(BCDoutputA[i], bitRead(nixiedata[TS][i], 0));
				digitalWrite(BCDoutputB[i], bitRead(nixiedata[TS][i], 1));
				digitalWrite(BCDoutputC[i], bitRead(nixiedata[TS][i], 2));
				digitalWrite(BCDoutputD[i], bitRead(nixiedata[TS][i], 3));
			}
		updating = false;
		blanking = true;
		ul_PreviousMicros = ul_CurrentMicros;  //get the time (in microseconds) for the next timeslot
	}
}

void refresh()
{
	int i;
	unsigned long ul_CurrentMicros = micros();
	if ((blanking == true) & (ul_CurrentMicros - ul_PreviousMicros > ul_BlankingIntervalMicros)) //if true, blanking interval is over
	{
	// #ifdef DEBUG
		// Serial.print("Refreshing.  Time = ");
		// Serial.println(micros());
	// #endif
		//Turn on the anodes for the Nixies for this timeslot (TS)
		for (i=0;i<=5;i++)
			{		
				if (anodegroups[TS][i] != 0)   // Probably can remove this check (what happens if you attempt to write to a nonexistant pin?)
				{
					digitalWrite(anodegroups[TS][i],LOW);	// Turn on anode voltage to all digits in mux timeslot j (LOW to turn on)
				}
			}		
		updating = true;
		blanking = false;
		ul_PreviousMicros = ul_CurrentMicros;  //get the time (in microseconds) for the next timeslot
	}
	
}

void blink()
{
	//state = !state;
	//if (state) {oneHzUpdate = true;}
	// #ifdef DEBUG
		// Serial.print("state = ");
		// Serial.println(state);
	// #endif
	oneHzUpdate = true;
}

// Scans keypad and returns which number is pressed
int switchscan(byte* switchlist, byte* switchValues, int length)
{
    int i;
	for (i = 0; i < length; i++)
	{
        if (digitalRead(switchlist[i])!=switchValues[i])
        {
            return i;
            break;
        }
	}
  return -1;
}

void aggregate(byte* x, byte num)
{
	int i;
	for (i=0;i<=sizeof(x);i++)
	{
		x[i] = bitRead(num,i);

	}
	
}

void sfxtest()
{
	int j;
	wTrig.trackPlayPoly(999);    // track 999 is ambient
	wTrig.trackLoop(999, true);  // plays track 999 continuously (loop enabled)
	delay(5000);
	for (j=0;j<=12;j++)
	{
		wTrig.trackPlayPoly(j);
		#ifdef DEBUG
			Serial.print("track00");
			Serial.println(j);
		#endif
		delay(5000);
		wTrig.trackStop(j);
		delay(1000);
	}
	wTrig.stopAllTracks();
}
