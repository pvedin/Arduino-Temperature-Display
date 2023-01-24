#include <OneWire.h>
#include <LiquidCrystal.h>
#include <IRremote.h>
#include <math.h>
// Row 1: Temperature from most recent value
// Row 2: Min/max temperatures for the last hour
//
// Functional temperatures:
// Min: 7.0 C
// Max: 32.5 C
// 16-bit:
// Min: -10 C
// Max: 40 C (adjustable)
// Sensors will be read twice as often with 8-bit storage (once every second rather than every other second).


/* LCD Functions (lcd.foo):
 * begin(cols,rows)     -- Specify dimensions of display
 * setCursor(col,row)   -- Position cursor (where subsequent text is written)
 * scrollDisplayLeft()  -- Scroll text and cursor one space to the left
 * scrollDisplayRight() -- See above, but right
 * print(data)          -- Print text (data: char,byte,int,long,string)
 * print(data,BASE)     -- Optional base: BIN, OCT, DEC, HEX
 * clear()              -- Clear display and position cursor in upper-left corner
*/

#define STORAGE_MODE (16) // 8 or 16 bits (affects how temperature values are stored)
#define MAX_TEMPERATURE_8 ((float) 32.5)
#define MIN_TEMPERATURE_8 ((float) 7)
#define MAX_TEMPERATURE_16 ((float) 40)
#define MIN_TEMPERATURE_16 ((float) -10)
#define s ((unsigned long) 1000)
#define minutes (60 * s)
#define minMaxDelay (1 * minutes) // how often the min/max values are calculated (excluding when buttons are pressed)
#define displayTimeout    ( 2 * minutes)
#define displayReInitTime (10 * minutes)
#define pre_cooldown (500)  // PIR
#define post_cooldown (3000) // PIR
#define fluctuation_cooldown (100) // PIR

#if STORAGE_MODE == 8
  #define tempDelay (s)
  #define NUMBER_TYPE unsigned char
  #define MAX_TEMPERATURE_FLT (MAX_TEMPERATURE_8)
  #define MAX_TEMPERATURE_INT ((NUMBER_TYPE)(MAX_TEMPERATURE_8 * 10))
  #define MIN_TEMPERATURE_FLT (MIN_TEMPERATURE_8)
  #define MIN_TEMPERATURE_INT ((NUMBER_TYPE)(MIN_TEMPERATURE_8 * 10))
#else
  #define tempDelay (2.40 * s) // 1500 times/hour
  #define NUMBER_TYPE unsigned int
  #define MAX_TEMPERATURE_FLT (MAX_TEMPERATURE_16)
  #define MAX_TEMPERATURE_INT ((NUMBER_TYPE)(MAX_TEMPERATURE_16 * 10))
  #define MIN_TEMPERATURE_FLT (MIN_TEMPERATURE_16)
  #define MIN_TEMPERATURE_INT ((NUMBER_TYPE)(MIN_TEMPERATURE_16 * 10))
#endif

#define measurementsPerHour (unsigned long)(3600*s / tempDelay)

// https://wiki.keyestudio.com/Ks0023_keyestudio_18B20_Temperature_Sensor
#define analogTemp_Pin (A0)
#define PIR_Pin (15) // Passive Infrared Reciever (Heat/Animal detection)
#define Buzzer_Pin (14)
#define DS18S20_Pin (12)//DS18S20 (digital temp sensor)
#define IR_RCV_Pin (11) 
#define dispMode_Pin (10) // Button used to switch between displaying inside and outside temperatures
#define dispToggle_Pin (9) // Button used to turn on/off the display
#define dispToggleOut_Pin (8) // Used to control the power to the LCD backlight
#define lcd_a_Pin dispToggleOut_Pin // synonym
#define lcd_rs_Pin (2)
#define lcd_enable_Pin (3)
#define lcd_d4_Pin (4)
#define lcd_d5_Pin (5)
#define lcd_d6_Pin (6)
#define lcd_d7_Pin (7)

/* Special characters: 
 * http://www.martyncurrey.com/wp-content/uploads/2017/03/LCDs_12_CharSet_01.jpg
 * left table
*/
#define degreeSym (String((char)0b11011111))
#define kk_na (String((char)0b11000101))
#define kk_ka (String((char)0b10110110))
#define kk_so (String((char)0b10111111))
#define kk_to (String((char)0b11000100))

// IR Signals
#define IR_CH_M (0xBA45Ff00) // CH-
#define IR_CH   (0xB946FF00) // CH
#define IR_CH_P (0xB847FF00)  // CH+

#define IR_REV (0xBB44FF00) // |<<
#define IR_FF (0xBF40FF00) // >>|
#define IR_PP (0xBC43FF00) // >||          
#define IR_M (0xF807FF00) // -
#define IR_P (0xEA15FF00) // +
#define IR_EQ (0xF609FF00) // EQ
#define IR_0 (0xE916FF00) // 0
#define IR_100P (0xE619FF00) // 100+
#define IR_200P (0xF20DFF00) // 200+

#define IR_1 (0xF30CFF00) // 1
#define IR_2 (0xE718FF00) // 2
#define IR_3 (0xA15EFF00) // 3

#define IR_4 (0xF708FF00) // 4 
#define IR_5 (0xE31CFF00) // 5
#define IR_6 (0xA55EFF00) // 6

#define IR_7 (0xBD42FF00) // 7
#define IR_8 (0xAD52FF00) // 8
#define IR_9 (0xB54AFF00) // 9 

// Fan remote IR signals
#define IR_FAN_POWER (0xFC03FC03)
#define IR_FAN_TIME (0xF40BF40B)
#define IR_FAN_ROTATION (0xEC13EC13)

String currTemp = "??.?" + degreeSym + "C";
String minTemp = "??.?" + degreeSym + "C";
String maxTemp = "??.?" + degreeSym + "C";
String dispMode = "Outside"; // Will initialise as "Inside"
bool dispToggle = false; // ON
bool dispToggleChange = false; 
bool dispModeChange = false;
bool dispStatReset = false;
bool dispForceUpdate = true;
bool first = true;
bool turnOnDisplay = true;
bool turnOffDisplay = false;

unsigned long time = millis();
unsigned long lastButtonPress = 0;


unsigned long measurements = 0;
#if STORAGE_MODE == 8
  byte inTemps[measurementsPerHour];
  byte outTemps[measurementsPerHour];
#else
  int inTemps[measurementsPerHour];
  int outTemps[measurementsPerHour];
#endif

unsigned int tempIndex = 0;




// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(lcd_rs_Pin, lcd_enable_Pin, lcd_d4_Pin, lcd_d5_Pin, lcd_d6_Pin, lcd_d7_Pin);
OneWire ds(DS18S20_Pin);  // on digital pin 2


void setup(void) {
    Serial.begin(9600);
    lcd.begin(16, 2);  // set up the LCD's number of columns and rows:
    pinMode(dispMode_Pin, INPUT_PULLUP); //Set button port
    pinMode(dispToggle_Pin, INPUT_PULLUP); //Set button port
    pinMode(dispToggleOut_Pin, OUTPUT);
    pinMode(Buzzer_Pin, OUTPUT);
    pinMode(PIR_Pin, INPUT);
    IrReceiver.begin(IR_RCV_Pin,ENABLE_LED_FEEDBACK); //Initialization infrared receiver
}
void loop(void) {
    static byte validTemps = 0b11; // 0b01: inside; 0b10: outside
    time = millis();
    readDispTogglePin();
    readDispModePin();
    readIRRcv();
    checkAvoidance(); // activate buzzer if motion detected
    validTemps = getTemps(validTemps);
    dispForceUpdate = dispModeChange || dispToggleChange || dispStatReset || first;
    processTemps();
    updateDisplay(validTemps);
}

// adapted from https://www.instructables.com/id/Arduino-37-in-1-Sensors-Kit-Explained/, step 18
NUMBER_TYPE Thermistor(int RawADC) {
  double Temp;
  Temp = log(10000.0*((1024.0/RawADC-1))); 
  Temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * Temp * Temp ))* Temp );
  Temp = Temp - 273.15;            // Convert Kelvin to Celcius
   //Temp = (Temp * 9.0)/ 5.0 + 32.0; // Convert Celcius to Fahrenheit
   return (NUMBER_TYPE) (Temp * 10);
}

void readDispTogglePin() {
  static bool startRead = false;
  static bool lockInput = false;
  static unsigned long lastRead = 0;
  readBtn(dispToggle_Pin, &startRead, &lockInput, &lastRead, &dispToggleChange);
  if (dispToggleChange || (lastButtonPress + displayTimeout < time && !dispToggle)) { 
    // button press or display timeout while display on
    toggleDisp();
  }
}

void toggleDisp() {
  if (!dispToggle) {
      turnOffDisplay = true;
    } else {
      turnOnDisplay = true;
    }
  lastButtonPress = time;
  dispToggleChange = false;
  dispToggle ^= true;
  dispModeChange = true; // refresh text
}

void readDispModePin() {
  static bool startRead = false;
  static bool lockInput = false;
  static unsigned long lastRead = 0;
  readBtn(dispMode_Pin, &startRead, &lockInput, &lastRead, &dispModeChange);
  if (dispModeChange) {
    toggleDispMode();
  }
}

void toggleDispMode() {
    dispModeChange = true;
    dispMode = (dispMode == "Inside") ? "Outside" : "Inside";
    lastButtonPress = time;
}

void readIRRcv(){
  if (IrReceiver.decode()){//&& (lastButtonPress + displayTimeout < time && !dispToggle)) {
    switch(IrReceiver.decodedIRData.decodedRawData){
      case IR_CH_M:
      case IR_CH:
      case IR_CH_P:
      case IR_REV:
      case IR_FF:
      case IR_PP:        
      case IR_M:
      case IR_P:
      case IR_FAN_POWER:
        Serial.println("IR: toggling display!");
        toggleDisp();
        break;
      case IR_EQ:
      case IR_FAN_TIME:
        Serial.println("IR: resetting temperature statistics!");
        resetStats();
        break;
      case IR_0:
      case IR_100P:
      case IR_200P:
      case IR_1:
      case IR_2:
      case IR_3:
      case IR_4:
      case IR_5:
      case IR_6:
      case IR_7:
      case IR_8:
      case IR_9:
      case IR_FAN_ROTATION:
        Serial.println("IR: toggling display mode!");
        toggleDispMode();
        break;
      default:
        Serial.print("Unknown IR value: ");
        Serial.println(IrReceiver.decodedIRData.decodedRawData, HEX);
        break;
    }        
    IrReceiver.resume(); // Receiving the next value
  }
}

void resetStats() {
  if (measurements){
    measurements = 0;
    inTemps[0] = inTemps[tempIndex];
    outTemps[0] = outTemps[tempIndex];
    tempIndex = 0;
    dispStatReset = true;
    lastButtonPress = time;
  }
}

void checkAvoidance(){
  static unsigned long oldTime = time;
  static bool started = false;
  static bool played = false;
  static bool fluctuation = false;
  unsigned char sigOut = digitalRead(PIR_Pin);
  if (started && sigOut == HIGH) {
    started = false;
    fluctuation = true;
    oldTime = time;
    Serial.println("PIR Fluctuation!");
    return;
  }
  if (oldTime + pre_cooldown > time) {return;}
  else if (fluctuation && oldTime + fluctuation_cooldown > time) {return;}

  fluctuation = false;
  
  //Serial.print(sigOut);
  if (!started && sigOut == LOW) {
    started = true;
    oldTime = time;
    Serial.println("Motion detected!");
    return;
  }
  
  if (started && !played) {
    played = true;
    tone(Buzzer_Pin,1000,post_cooldown);
    Serial.println("Playing tone!");
  }
  
  if (started && oldTime + pre_cooldown +  post_cooldown < time) {
    started = false;
    played = false;
    Serial.println("Buzzer cooldown reset");
  }
  //*/
}

void readBtn(int pin, bool* startRead, bool* lockInput, unsigned long* lastRead, bool* outputSignal) {
  if (*lastRead + 25 < time) { // read every 25ms
    *lastRead = time;
    if (!(*startRead) && digitalRead(pin)==LOW) { 
      *startRead = true;
    } else if (*lockInput && digitalRead(pin)==HIGH) {
      *lockInput = false;
    } else if (*startRead) {
      *startRead = false;
      if (digitalRead(pin)==LOW && !(*lockInput)) {
        *lockInput = true; // allow button to be held
        *outputSignal = true;
        Serial.println("Button press!!");
      }
    }
  }
}


byte getTemps(byte validTemps) {
    //returns the temperature from one DS18S20 in DEG Celsius
    static unsigned long oldTime = 0;

    if (time - oldTime <= tempDelay) {
      return validTemps;
    }

    validTemps |= 0x3; // assume valid
    tempIndex = (tempIndex + 1) % measurementsPerHour;
    measurements++;

    float TemperatureSum = readDigitalTemperature();
    if (first) {
      TemperatureSum = readDigitalTemperature(); // returns value that is approx 7 deg colder first time for some reason 
    }

    // add result to array
    unsigned int outTempRes = TemperatureSum * 10; // e.g. 34.5 C => 345
    Serial.println("Out: " + (String)(int)outTempRes);
    NUMBER_TYPE outTempPrevious = outTemps[(tempIndex-1) % measurementsPerHour];
    if (outTempRes <= MIN_TEMPERATURE_INT || outTempRes > MAX_TEMPERATURE_INT) {
      outTemps[tempIndex] = outTempPrevious;
      validTemps &= 0b01;
      Serial.println("Invalid out temp; assuming previous temp unchanged");
    } else { // Ensure the range [MIN,MAX] can be represented in the chosen list (esp. for 8-bit storage)
      outTemps[tempIndex] = (NUMBER_TYPE) (outTempRes - MIN_TEMPERATURE_INT); 
    }

    NUMBER_TYPE inTempRes = Thermistor(analogRead(analogTemp_Pin));
    NUMBER_TYPE inTempPrevious = inTemps[(tempIndex-1) % measurementsPerHour];
    if (inTempRes <= MIN_TEMPERATURE_INT || inTempRes > MAX_TEMPERATURE_INT) {
      inTemps[tempIndex] = inTempPrevious;
      validTemps &= 0b10;
      Serial.println("Invalid in temp; assuming previous temp unchanged");
    } else {
      inTemps[tempIndex] = inTempRes - MIN_TEMPERATURE_INT;
    }
    oldTime = millis();
    return validTemps;
}

float readDigitalTemperature() {
    byte data[12];
    byte addr[8];
 
    if ( !ds.search(addr)) {
        //no more sensors on chain, reset search
        ds.reset_search();
    }
 
    if ( OneWire::crc8( addr, 7) != addr[7]) {
        Serial.println("CRC is not valid!");
    }
 
    if ( addr[0] != 0x10 && addr[0] != 0x28) {
        Serial.print("Device is not recognized");
    }
 
    ds.reset();
    ds.select(addr);
    ds.write(0x44,1); // start conversion, with parasite power on at the end
 
    byte present = ds.reset();
    ds.select(addr);    
    ds.write(0xBE); // Read Scratchpad
 
    for (int i = 0; i < 9; i++) { // we need 9 bytes
        data[i] = ds.read();
    }
    ds.reset_search();
   
    byte MSB = data[1];
    byte LSB = data[0];

    float tempRead = ((MSB << 8) | LSB); //using two's compliment
    float TemperatureSum = tempRead / 16;
    return TemperatureSum;
}

String processTemp(NUMBER_TYPE temp) { // float => String
    String tmpStr = (String) (((unsigned int)temp) + MIN_TEMPERATURE_INT);
    if (tmpStr.length() == 1) {tmpStr = "0" + tmpStr;} // + 0.x
    if (tmpStr.length() == 2) {tmpStr = "0" + tmpStr;} // +0x.x
    int lastI = tmpStr.length()-1;
    String out = String(tmpStr.substring(0,lastI) + "." + tmpStr.substring(lastI));
    out.concat(degreeSym);
    out.concat("C");
    return out;
}

void processTemps() { // Prepare temperature strings
    static unsigned long oldTimeCurr = 0;
    static unsigned long oldTimeMinMax = 0;
    if (first) {
      return; // no temperatures to process
    }
    if (time - oldTimeCurr > tempDelay || dispForceUpdate) {
        currTemp = dispMode == "Inside" ? processTemp(inTemps[tempIndex]) : processTemp(outTemps[tempIndex]);
        oldTimeCurr = millis();
    }
    if (time - oldTimeMinMax > minMaxDelay || dispForceUpdate) {  // O(n) since unsorted array
        unsigned int minMaxI[2] = {0,0}; // 0=min, 1=max
        for (unsigned int i = 1; i < min(measurements, measurementsPerHour); i++) {
            int tempTemp = dispMode == "Inside" ? inTemps[i] : outTemps[i];
            if (tempTemp < (dispMode == "Inside" ? inTemps[minMaxI[0]] : outTemps[minMaxI[0]])) {
                minMaxI[0] = i;
            }
            if (tempTemp > (dispMode == "Inside" ? inTemps[minMaxI[1]] : outTemps[minMaxI[1]])) {
                minMaxI[1] = i;
            }
        }

        minTemp = dispMode == "Inside" ? processTemp(inTemps[minMaxI[0]]) : processTemp(outTemps[minMaxI[0]]);
        maxTemp = dispMode == "Inside" ? processTemp(inTemps[minMaxI[1]]) : processTemp(outTemps[minMaxI[1]]);
        oldTimeMinMax = millis();
        dispStatReset = false;
    }
}

void updateDisplay(byte validTemps) {
    static unsigned long oldTimeCurr = 0;
    static unsigned long oldTimeMinMax = 0;
    static unsigned long displayOffTime = 0;
    static String line1 = "123456789ABCDEF";
    static String line2 = "FEDCBA987654321";
    if (dispForceUpdate) {Serial.println("force update!");}
    if (time - oldTimeMinMax > minMaxDelay || dispForceUpdate) {
        line2 = minTemp;
        if (dispMode == "Inside") {
          line2.concat(" " + kk_na + kk_ka + " "); // "Inside" == "ナカ" in Japanese
        } else if (dispMode == "Outside") {
          line2.concat(" " + kk_so + kk_to + " "); // "Outside" == "ソト" in Japanese
        } else {
          line2.concat(" ?? ");
        }
        line2.concat(maxTemp);
        oldTimeMinMax = millis();
        dispModeChange = false;
    }
    if (time - oldTimeCurr > tempDelay || dispForceUpdate) {
        if (first) {first = false;}
        String line1;

        bool validTemp = dispMode == "Inside" ? validTemps & 0x1 : validTemps & 0x2;
        String ind = validTemp ? " " : "!";
        line1 = "Min " + ind;
        line1.concat(currTemp);
        line1.concat(ind + " Max");

        lcd.clear();  //Clears the LCD screen and positions the cursor in the upper-left corner
        lcd.print(line1);
        lcd.setCursor(0,1);
        lcd.print(line2);
        oldTimeCurr = millis();
    }

    if (turnOffDisplay) {
      digitalWrite(dispToggleOut_Pin,LOW);
      lcd.noDisplay();
      turnOffDisplay = false;
      displayOffTime = time;
    }
    if (turnOnDisplay) {
      digitalWrite(dispToggleOut_Pin,HIGH);
      lcd.display();
      turnOnDisplay = false;
      if (displayOffTime + displayReInitTime < time) { // need to reinitialise
        lcd.begin(16, 2);
      }
    }
    
}
