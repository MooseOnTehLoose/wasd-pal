#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "LedControl.h"
#include <SoftwareSerial.h>
#include "lcdgfx.h"
#include <stdlib.h>
#include <avr/wdt.h>


//Bluetooth Module Configuration
#define BUFSIZE                        128  
#define BLUEFRUIT_SPI_CS               8
#define BLUEFRUIT_SPI_IRQ              7
#define BLUEFRUIT_SPI_RST              4   
//Battery Capacity Reading
#define VBATPIN A9

//https://learn.adafruit.com/introducing-adafruit-ble-bluetooth-low-energy-friend/ble-services#hid-keyboard-codes-2435765
//
//KEY                                  //KEY VALUES
const int values[6][6] = {             //const String keyvalues[6][6] = {                                          
  {0x3A,0x3B,0x3C,0x3D,0x3E,0x3F},     //{"F1","F2","F3","F4","F5","F6"},                                        
  {0x29,0x1e,0x1f,0x20,0x21,0x22},     //{"ESC","1","2","3","4","5"},                                        
  {0x2B,0x14,0x1a,0x08,0x15,0x17},     //{"TAB","Q","W","E","R","T"},                                        
  {0x39,0x04,0x16,0x07,0x09,0x0A},     //{"CAPS","A","S","D","F","G"},                                        
  {0xE1,0x1D,0x1B,0x06,0x19,0x05},     //{"SHIFT","Z","X","C","V","B"},                                        
  {0xE0,0xE3,0xE2,0x2C,0x00,0x00}      //{"CTRL","WIN","ALT","SPACE","MOD1","MOD2"}                                       
};                                     //};    
    
//MOD2                                 ////MOD2 VALUES      
const int mod2values[6][6] = {         //const String mod2keyvalues[6][6] = {                                    
  {0x40,0x41,0x42,0x43,0x44,0x45},     //{"F7","F8","F9","F10","F11","F12"},                                        
  {0x4C,0x23,0x24,0x25,0x26,0x27},     //{"DEL","6","7","8","9","0"},                                        
  {0x2A,0x1C,0x18,0x0C,0x12,0x13},     //{"BKSPC","Y","U","I","O","P"},                                        
  {0x28,0x0B,0x0D,0x0E,0x0F,0x33},     //{"ENTER","H","J","K","L",";"},                                        
  {0xE1,0x11,0x10,0x36,0x37,0x38},     //{"SHIFT","N","M",",",".","/"},                                        
  {0xE0,0xE3,0xE2,0x2C,0x00,0x00}      //{"CTRL","WIN","ALT","SPACE","MOD1","MOD2"}                                       
};                                     //}; 
       
//MOD1                                 ////MOD1 VALUES     
const int mod1values[6][6] = {         //const String mod1keyvalues[6][6] = {                                    
  {0x3A,0x3B,0x3C,0x3D,0x3E,0x3F},     //{"F1","F2","F3","F4","F5","F6"},                                        
  {0x29,0x1e,0x1f,0x20,0x21,0x22},     //{"ESC","1","2","3","4","5"},                                        
  {0x2B,0x14,0x52,0x08,0x15,0x17},     //{"TAB","BRIGHTDOWN","UP","BRIGHTUP","R","T"},                                        
  {0x39,0x50,0x51,0x4F,0x09,0x0A},     //{"CAPS","LEFT","DOWN","RIGHT","F","G"},                                        
  {0xE1,0x80,0x81,0x06,0x19,0x05},     //{"SHIFT","VOLDOWN","VOLUP","C","V","B"},                                        
  {0xE0,0xE3,0xE2,0x2C,0x00,0x00}      //{"CTRL","WIN","ALT","SPACE","MOD1","MOD2"}                                       
};                                     //};        
                                             
//Keyboard Setup
char previous[52];
byte rows[] = {11,12,13,23,22,21};
const int rowCount = 6;
byte cols[] = {0,1,5,6,9,10};
const int colCount = 6;
byte keys[6][6];

//Non Blocking Timer
unsigned long previousMillis = 0;
unsigned long previousMillis1 = 0;
unsigned long previousMillis2 = 0;
const long intervalOLED = 2000; 
const long interval1 = 250;
const long interval2 = 250;
bool pulse = false;
bool mod1flash = false;
bool mod2flash = false;

/* ...hardware SPI, using SCK/MOSI/MISO hardware SPI pins and then user selected CS/IRQ/RST */
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

//Pins on the MAX7219 - Bluefruite BLE 32u4
//int dataPin  int clkPin  int csPin  int numDevices = 1
LedControl lc=LedControl(18,19,20,1);

DisplaySSD1306_128x32_I2C display(-1); 
const int canvasWidth = 128; 
const int canvasHeight = 32;
uint8_t canvasData[canvasWidth*(canvasHeight/8)];
NanoCanvas1 canvas(canvasWidth, canvasHeight, canvasData);


void setup(void) {

  //need to wait for bluetooth module 
  delay(500);
  wdt_enable(WDTO_2S);

  for(int x=0; x<rowCount; x++) {
    pinMode(rows[x], INPUT);
  }
  for (int x=0; x<colCount; x++) {
    pinMode(cols[x], INPUT_PULLUP);
  }
  Serial.begin(115200);

  ble.begin();
  ble.sendCommandCheckOK(F( "AT+GAPDEVNAME=WASD-Pal" ));
  ble.sendCommandCheckOK(F( "AT+BleHIDEn=On" ));
  ble.sendCommandCheckOK(F( "AT+BleKeyboardEn=On" ));
  ble.sendCommandCheckOK(F( "AT+BLEBATTEN=1" ));
  //ble.sendCommandCheckOK(F( "AT+BLEBATTVAL=50" ));
  ble.echo(false);
  ble.reset();

  //Max7219 Config
  lc.shutdown(0,false);
  lc.setIntensity(0,15);
  lc.clearDisplay(0);
  drawAll();
  //ble.factoryReset();

  
  //OLED ss1306 Config
  display.begin();
  display.fill( 0x00 );
  display.setFixedFont(ssd1306xled_font6x8);
  
}

void loop(void){

  //Read the Keyboard Key state
  readMatrix();

  //Send the Key State via Bluetooth
  setKeys();

  //Update the OLED Display with Battery voltage / Percentage
  readBat();

  //Tell watchdog hes a good boy
  wdt_reset(); 
}

void readMatrix() {

  // iterate the columns
  for (int colIndex=0; colIndex < colCount; colIndex++) {
    // col: set to output to low
    byte curCol = cols[colIndex];
    pinMode(curCol, OUTPUT);
    digitalWrite(curCol, LOW);

    // row: interate through the rows
    for (int rowIndex=0; rowIndex < rowCount; rowIndex++) {
      byte rowCol = rows[rowIndex];
      pinMode(rowCol, INPUT_PULLUP);
      keys[colIndex][rowIndex] = digitalRead(rowCol);
      pinMode(rowCol, INPUT);
    }
    // disable the column
    pinMode(curCol, INPUT);
  }
}

void setKeys(){

  int shift = 0x00;
  int ctrl = 0x00;
  int win = 0x00;
  int alt = 0x00;
  
  if (keys[4][0] == LOW){
    shift = 0x02;
  }
  if (keys[5][0] == LOW){
    ctrl = 0x01;
  }
  if (keys[5][1] == LOW){
    win = 0x08;
  }
  if (keys[5][2] == LOW){
    alt = 0x04;
  }

  int (*lookuptable)[6] = values;
  //Assign the pointer to the correct values table based on which mod key is pressed
  if (keys[5][4] == LOW){
    lookuptable = mod1values;
    unsigned long currentMillis = millis(); // Get the current time
    if (currentMillis - previousMillis2 >= interval2) {
      drawMod1();
    }
  }else if (keys[5][5] == LOW){
    lookuptable = mod2values;1
    unsigned long currentMillis = millis(); // Get the current time
    if (currentMillis - previousMillis1 >= interval1) {
      drawMod2();
    }
  }else {
    drawAll();
  }

  //these get combined into a hex value for the actual key modifier
  int modifier = shift + ctrl + win + alt;

  //this is the actual keypress we will send
  int data[5] = {0x00,0x00,0x00,0x00,0x00};

  //we can only send 5 key presses at a time
  //drop all additional key presses after the first 5
  int max = 0;
  for (int x=0; x<rowCount; x++) {
    for (int y=0; y<colCount; y++) {
      //do not read the modifier keys in, ignore them as we track them above
      //   !shift              !ctrl               !win               !alt                !mod1               !mod2
      if ( !( x==4 && y==0) && !( x==5 && y==0) && !(x==5 && y==1) && !( x==5 && y==2) && !( x==5 && y==4) && !( x==5 && y==5) ){
        if (keys[x][y] == LOW && max < 5){
          //check which hex table to use then pull data from that table
          data[max] = lookuptable[x][y];
          max++;
        }
      }
    }
  }

  //MOD1 + ESC == factory reset the ble module and reboot the wasdpal
  if ( keys[5][4] == LOW && keys [1][0] == LOW ){
      ble.factoryReset();
      Serial.println("Resetting!");
      delay(4000);
  }


  char buffer[52];
  //format the buffer for the BLE signals we must send 
  sprintf(buffer, "AT+BLEKEYBOARDCODE=%.2x-00-%.2x-%.2x-%.2x-%.2x-%.2x", modifier, data[0],data[1],data[2],data[3],data[4]);

  //We don't want to send keys via bluetooth if the user is holding down the keys
  //wait till the data actually changes from previous
  //Sending 0x00 for keys tells the remote device we have released a keypress
  if (strcmp(buffer,previous)){
    strcpy(previous, buffer);

    //Actually send the key press via bluetooth
    ble.println(buffer);

  }
}


void readBat(){
  unsigned long currentMillis = millis(); // Get the current time
  if (currentMillis - previousMillis >= intervalOLED) {
    previousMillis = currentMillis; // Update the last action time
    float measuredvbat = analogRead(VBATPIN);
    measuredvbat *= 2;    // we divided by 2, so multiply back
    measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    measuredvbat /= 1024; // convert to voltage
    char battery[20]; 
    dtostrf(measuredvbat, 3, 2, battery);
    strcat(battery, "v");  
    display.printFixed(0,0, "Vbat: ");
    display.printFixed(32,0, battery);
    float percent = (measuredvbat - 3.22) * 100;

    if (percent < 0){
      percent = 0;
    }else if (percent > 100){
      percent = 100;
    }

    char percentBat[3]; 
    dtostrf(percent, -3, 0, percentBat);
    display.printFixed(64,0, percentBat);
    display.printFixed(84,0, "%");

    char sendBattLevel[20];
    sprintf(sendBattLevel, "AT+BLEBATTVAL=%s", percentBat);
    ble.println(sendBattLevel);

  }
}


void wasd(){
    lc.clearDisplay(0);
    lc.setRow(0,1,B00100000);
    lc.setRow(0,2,B01110000);
}

void drawMod1(){
  if(mod1flash){
    lc.setRow(0,0,B00000000); 
    lc.setRow(0,1,B01110000); 
    lc.setRow(0,2,B01110000); 
    lc.setRow(0,3,B01100000); 
    lc.setRow(0,4,B00001000);
    mod1flash = false;
  }else{
    lc.clearDisplay(0);
    mod1flash = true;
  }                         
}                           

void drawMod2(){
  if (mod2flash){
    lc.setRow(0,0,B00000000);
    lc.setRow(0,1,B00000000);
    lc.setRow(0,2,B00000000);
    lc.setRow(0,3,B00000000);
    lc.setRow(0,4,B00000100);
    mod2flash = false;
  }else{
    lc.clearDisplay(0);
    mod2flash = true;
  }  
}

void drawAll(){
  lc.setRow(0,0,B11111100);
  lc.setRow(0,1,B11111100);
  lc.setRow(0,2,B11111100);
  lc.setRow(0,3,B11111100);
  lc.setRow(0,4,B11111100);
}

void pulseDisplay(){
  if (pulse){
    drawAll();
    pulse=false;
  }else{
    lc.clearDisplay(0);
    pulse=true;
  }
}
