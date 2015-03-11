#include <IRremote.h>
#include "Tlc5940.h"
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_ILI9341.h"
#include <ArduinoJson.h>

// For the Adafruit shield, these are the default.
#define TFT_DC 19
#define TFT_CS 4
#define TFT_BACKLIGHT 15
#define SD_CS  18
// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
//Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
// If using the breakout, change pins as desired
//Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, 23);


int tft_backlight_brightness = 255;



#define PIN_IR_TRANSMITTER           3

#define PIN_MOTION_SENSOR           22
#define PIN_IR_RECEIVER             21
#define PIN_BUZZER                  20


#define PIN_MQ135_D                 30
#define PIN_MQ135_A                 A7
#define PIN_MQ2_D                   29
#define PIN_MQ2_A                   A4

#define PIN_LDR                     A3


IRrecv irrecv(PIN_IR_RECEIVER);
decode_results results;

IRsend irsend;
unsigned long last_ir_cmd_received;

// Following is not needed.  They were needed for a 1.44 color SPI display, that was
// used earlier.  Just letting this remain for future reference when both TLC5940 and
// the 1.44 SPI are used at the same time.  
#define __DC           15   // PD7
#define __CS           4    // PB4
#define __RST          23  // PC7
// MOSI --> (SDA) --> D5   - PB5
// SCLK --> (SCK) --> D7   - PB7

// Color definitions
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0  
#define WHITE   0xFFFF

// TFT_ILI9163C tft = TFT_ILI9163C(__CS, __DC, __RST);


int NUM_LEDS=10;
int LEDs[10][3] = { 
                  {0,1,2},
                  {3,4,5},
                  {6,7,8},
                  {9,10,11},
                  {12,13,14},                  
                  {17,18,19},
                  {20,21,22},
                  {23,24,25},
                  {26,27,28},
                  {29,30,31}
                 };                  

int count = 1000;                 
int value = 0;
int r,g,b;
int led,color;

int mq2=0,mq135=0,ldr=0,pir=0;
float temperature = 0, humidity = 0;
int lightbulb = -1, buzzer = -1;

int last_mq2=0,last_mq135=0,last_ldr=0,last_pir=0;
float last_temperature = 0, last_humidity = 0;
int last_lightbulb = -1, last_buzzer = -1;



void clearDisp()
{
//    tft.clearScreen();
    tft.fillScreen(ILI9341_BLACK);
    
} 

void buzz(long frequency, long length) {
  long delayValue = 1000000/frequency/2; // calculate the delay value between transitions
  //// 1 second's worth of microseconds, divided by the frequency, then split in half since
  //// there are two phases to each cycle
  long numCycles = frequency * length/ 1000; // calculate the number of cycles for proper timing
  //// multiply frequency, which is really cycles per second, by the number of seconds to 
  //// get the total number of cycles to produce
 for (long i=0; i < numCycles; i++){ // for the calculated length of time...
    digitalWrite(PIN_BUZZER,HIGH); // write the buzzer pin high to push out the diaphram
    delayMicroseconds(delayValue); // wait for the calculated delay value
    digitalWrite(PIN_BUZZER, LOW); // write the buzzer pin low to pull back the diaphram
    delayMicroseconds(delayValue); // wait againf or the calculated delay value
  }
}

// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.  20 pixels seems a
// good balance.

#define BUFFPIXEL 20

void bmpDraw(char *filename, uint8_t x, uint16_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  if((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');

  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print(F("File not found"));
    return;
  }

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x+w-1, y+h-1);

        for (row=0; row<h; row++) { // For each scanline...

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col=0; col<w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.color565(r,g,b));
          } // end pixel
        } // end scanline
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) Serial.println(F("BMP format not recognized."));
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

void setup()
{
    Serial.begin(115200);
    Serial1.begin(115200);
 
    irrecv.enableIRIn(); // Starts the receiver

    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(TFT_BACKLIGHT, OUTPUT);

//    Tlc.init();
    
    tft.begin();
    tft.setRotation(2);
//    analogWrite(TFT_BACKLIGHT, tft_backlight_brightness);
//    digitalWrite(TFT_BACKLIGHT, HIGH);  

    clearDisp();    

    tft.setTextColor(WHITE);  
    tft.setTextSize(1);
    tft.setCursor(60,220);
    tft.println("Initializing...");
    Serial.println("Initialiazing...");
    
    tft.setCursor(30, 240);
    Serial.print("Initializing SD card...");
    tft.print("Initializing SD card...");
    if (!SD.begin(SD_CS)) {
        Serial.println("failed!");
        tft.println("failed!");
    }
    Serial.println("OK!");
    tft.println("OK!");
    
    delay(100);
    bmpDraw("arduino.bmp", 60, 50);
    bmpDraw("esp8266.bmp", 60, 130);

    digitalWrite(PIN_BUZZER,HIGH);
    delay(50);
    digitalWrite(PIN_BUZZER,LOW);

    delay(3000);
    buzz(2500, 50);
    bmpDraw("bground.bmp", 0, 0);
    delay(2000);

    tft.setTextSize(2);
}


void fade(int led_color)
{
    char stri[20];
    sprintf(stri,"fade%d",color);

    value=value+10;
    if (value > 4000)
        value = 0;
   
    Tlc.clear();
   
    for (led=0;led<NUM_LEDS;led++)
    {
        Tlc.set(LEDs[led][led_color], value);
    }
   
    Tlc.update();
}

void all_fade()
{
    value=value+10;
    if (value > 4000)
        value = 0;
   
    Tlc.clear();
   
    for (led=0;led<NUM_LEDS;led++)
    {
        Tlc.set(LEDs[led][0], value);
        Tlc.set(LEDs[led][1], value);
        Tlc.set(LEDs[led][2], value);
    }
   
    Tlc.update();
}

void random_colors()
{
    Tlc.clear();

    int rand1 = random(0, 1000);
    int rand2 = random(0, 1);
    int rand3 = random(0, 1000);
    
    for (led=0;led<NUM_LEDS;led++)
    {
        Tlc.set(LEDs[led][0], rand1);
        Tlc.set(LEDs[led][1], rand2);       
        Tlc.set(LEDs[led][2], rand3);
    }
   
    Tlc.update();
  
}


void larson_scanner(int led_color)
{
    for(int j=0;j<10;j++)
    {
        for (led=0;led<NUM_LEDS;led++)
         {
            Tlc.clear();
            Tlc.set(LEDs[led][led_color], 100);
        
            if(led>0)
                Tlc.set(LEDs[led-1][led_color], 200);
           
            if(led>1)
                Tlc.set(LEDs[led-2][led_color], 4000);

            if(led>2)
                Tlc.set(LEDs[led-3][led_color], 200);

            if(led>3)
                Tlc.set(LEDs[led-4][led_color], 100);

            Tlc.update();
            delay(30);
        }

        for (led=NUM_LEDS-1;led>=0;led--)
        {
            Tlc.clear();
            Tlc.set(LEDs[led][led_color], 100);
        
            if(led<15)
                Tlc.set(LEDs[led+1][led_color], 200);
           
            if(led<14)
                Tlc.set(LEDs[led+2][led_color], 4000);

            if(led<13)
                Tlc.set(LEDs[led+3][led_color], 200);

            if(led<12)
                Tlc.set(LEDs[led+4][led_color], 100);

            Tlc.update();
            delay(30);
        }
    }
   
}

void larson_scanner_random()
{
    int rand0 = random(0, 1000);
    int rand1 = random(0, 1000);
    int rand2 = random(0, 1000);
    
    for(int j=0;j<10;j++)
    {
        for (led=0;led<NUM_LEDS;led++)
         {
            Tlc.clear();
            Tlc.set(LEDs[led][0], rand0);
            Tlc.set(LEDs[led][1], rand1);
            Tlc.set(LEDs[led][2], rand2);
        
            if(led>0)
            {
                Tlc.set(LEDs[led-1][0], rand0+200);
                Tlc.set(LEDs[led-1][1], rand1+200);
                Tlc.set(LEDs[led-1][2], rand2+200);
            }
            
            if(led>1)
            {
                Tlc.set(LEDs[led-2][0], rand0+400);
                Tlc.set(LEDs[led-2][1], rand1+400);
                Tlc.set(LEDs[led-2][2], rand2+400);
            }

            if(led>2)
            {
                Tlc.set(LEDs[led-3][0], rand0+200);
                Tlc.set(LEDs[led-3][1], rand1+200);
                Tlc.set(LEDs[led-3][2], rand2+200);
            }

            if(led>3)
            {
                Tlc.set(LEDs[led][0], rand0);
                Tlc.set(LEDs[led][1], rand1);
                Tlc.set(LEDs[led][2], rand2);
            }

            Tlc.update();
            delay(30);
        }

        rand0 = random(0, 1000);
        rand1 = random(0, 1000);
        rand2 = random(0, 1000);
    

        for (led=NUM_LEDS-1;led>=0;led--)
        {
            Tlc.clear();
            Tlc.set(LEDs[led][0], rand0);
            Tlc.set(LEDs[led][1], rand1);
            Tlc.set(LEDs[led][2], rand2);
        
            if(led<NUM_LEDS-1)
            {
                Tlc.set(LEDs[led+1][0], rand0+200);
                Tlc.set(LEDs[led+1][1], rand1+200);
                Tlc.set(LEDs[led+1][2], rand2+200);
            }
           
            if(led<NUM_LEDS-2)
            {
                Tlc.set(LEDs[led+2][0], rand0+400);
                Tlc.set(LEDs[led+2][1], rand1+400);
                Tlc.set(LEDs[led+2][2], rand2+400);
            }

            if(led<NUM_LEDS-3)
            {
                Tlc.set(LEDs[led+3][0], rand0+200);
                Tlc.set(LEDs[led+3][1], rand1+200);
                Tlc.set(LEDs[led+3][2], rand2+200);
            }

            if(led<NUM_LEDS-4)
            {
                Tlc.set(LEDs[led][0], rand0);
                Tlc.set(LEDs[led][1], rand1);
                Tlc.set(LEDs[led][2], rand2);
            }

            Tlc.update();
            delay(30);
        }
    }
   
}

void larson_scanner_random2()
{
    int rand0 = random(0, 3000);
    int rand1 = random(0, 3000);
    int rand2 = random(0, 3000);
    
    for(int j=0;j<10;j++)
    {
        for (led=0;led<NUM_LEDS;led++)
         {
            rand0 = random(0, 3000);
            rand1 = random(0, 3000);
            rand2 = random(0, 3000);

            Tlc.clear();
            Tlc.set(LEDs[led][0], rand0);
            Tlc.set(LEDs[led][1], rand1);
            Tlc.set(LEDs[led][2], rand2);
        
            if(led>0)
            {
                Tlc.set(LEDs[led-1][0], rand0+20);
                Tlc.set(LEDs[led-1][1], rand1+20);
                Tlc.set(LEDs[led-1][2], rand2+20);
            }
            
            if(led>1)
            {
                Tlc.set(LEDs[led-2][0], rand0+40);
                Tlc.set(LEDs[led-2][1], rand1+40);
                Tlc.set(LEDs[led-2][2], rand2+40);
            }

            if(led>2)
            {
                Tlc.set(LEDs[led-3][0], rand0+20);
                Tlc.set(LEDs[led-3][1], rand1+20);
                Tlc.set(LEDs[led-3][2], rand2+20);
            }

            if(led>3)
            {
                Tlc.set(LEDs[led][0], rand0);
                Tlc.set(LEDs[led][1], rand1);
                Tlc.set(LEDs[led][2], rand2);
            }

            Tlc.update();
            delay(30);
        }

    

        for (led=NUM_LEDS-1;led>=0;led--)
        {
            rand0 = random(0, 3000);
            rand1 = random(0, 3000);
            rand2 = random(0, 3000);

            Tlc.clear();
            Tlc.set(LEDs[led][0], rand0);
            Tlc.set(LEDs[led][1], rand1);
            Tlc.set(LEDs[led][2], rand2);
        
            if(led<NUM_LEDS-1)
            {
                Tlc.set(LEDs[led+1][0], rand0+20);
                Tlc.set(LEDs[led+1][1], rand1+20);
                Tlc.set(LEDs[led+1][2], rand2+20);
            }
           
            if(led<NUM_LEDS-2)
            {
                Tlc.set(LEDs[led+2][0], rand0+40);
                Tlc.set(LEDs[led+2][1], rand1+40);
                Tlc.set(LEDs[led+2][2], rand2+40);
            }

            if(led<NUM_LEDS-3)
            {
                Tlc.set(LEDs[led+3][0], rand0+20);
                Tlc.set(LEDs[led+3][1], rand1+20);
                Tlc.set(LEDs[led+3][2], rand2+20);
            }

            if(led<NUM_LEDS-4)
            {
                Tlc.set(LEDs[led][0], rand0);
                Tlc.set(LEDs[led][1], rand1);
                Tlc.set(LEDs[led][2], rand2);
            }

            Tlc.update();
            delay(30);
        }
    }
   
}

void loop()
{
    int i=0, c=0, opening_brace=0, pos=0, drawWidth = 0;
    char str[256],buf[256];
    
//    analogWrite(TFT_BACKLIGHT, tft_backlight_brightness);
//    tft_backlight_brightness += 10;
    
//    if (tft_backlight_brightness > 250)
//    {
//        tft_backlight_brightness = 0;
//    }

    if(Serial1.available())
    {
//	while(c = Serial1.read() >= 32)
        while (Serial1.available())
        {
            c = (int) Serial1.read();
            if (c == 123)
            {
                break;
            }
        }

        if (c == 123) // {
        {
	    str[pos++] = (char)c;
	    opening_brace++;

	    while(Serial1.available())
	    {
                 c = (int) Serial1.read();
	         if ((c >= 32) && (c <= 126))
		 {
		     str[pos++] = (char)c;
		 }

		 if (c == 125)  // }
		 {
		     opening_brace--;
		 }

		 if (opening_brace == 0 )
		 {
                     str[pos++] = 0;
                     Serial.print("Received from ESP8266: ");
                     Serial.println(str);
                     
                     StaticJsonBuffer<200> jsonBuffer;
                     JsonObject& root = jsonBuffer.parseObject(str);

                     if (root.success())
                     {
                         if(strstr(str,"LightBulb"))
                         {
                             lightbulb = root["LightBulb"];
                             Serial.print("lightbulb=");
                             Serial.println(lightbulb);
                         }
                         else if(strstr(str,"Buzzer"))
                         {
                             buzzer = root["Buzzer"];
                             Serial.print("buzzer=");
                             Serial.println(buzzer);
                         }
                         else if(strstr(str,"Temperature"))
                         {
                             temperature = atof(root["Temperature"]);
                             Serial.print("temperature=");
                             Serial.println(temperature);
                         }
                         else if(strstr(str,"Humidity"))
                         {
                             humidity = atof(root["Humidity"]);
                             Serial.print("humidity=");
                             Serial.println(humidity);
                         }
                     }
    		     break;
		 }
	    }
        }
    }    

    if(count++>100)
    {
        if(buzzer == 1)
        {
            digitalWrite(PIN_BUZZER,HIGH);
            delay(10);
            digitalWrite(PIN_BUZZER,LOW);
        }
        
        count = 0;
        mq2=analogRead(PIN_MQ2_A);
        mq135=analogRead(PIN_MQ135_A);
        ldr=analogRead(PIN_LDR);
        pir=digitalRead(PIN_MOTION_SENSOR);

        if (pir == 1)
        {
//        buzz(500, 5);
        }

        if (last_mq2 != mq2)
        {
            last_mq2 = mq2;
            drawWidth = map(mq2,0,300,0,220);
            tft.fillRoundRect(1, 1, 220,       20, 10, BLACK);
            tft.drawRoundRect(1, 1, 220,       20, 10, YELLOW);
            tft.fillRoundRect(1, 1, drawWidth, 20, 10, YELLOW);
            tft.fillRoundRect(1, 22, 80,       20, 10, ILI9341_BLUE);
            tft.setCursor(1,23);
            tft.print("MQ2 ");    
            tft.print(mq2);
        }

        if (last_mq135 != mq135)
        {
            last_mq135 = mq135;
            drawWidth = map(mq135,0,300,0,220);
            tft.fillRoundRect(1, 42, 220,       20, 10, BLACK);
            tft.drawRoundRect(1, 42, 220,       20, 10, YELLOW);
            tft.fillRoundRect(1, 42, drawWidth, 20, 10, YELLOW);
            tft.fillRoundRect(1, 63, 110,       20, 10, ILI9341_BLUE);
            tft.setCursor(1,64);
            tft.print("MQ135 ");    
            tft.print(mq135);
        }

        if (last_ldr != ldr)
        {
            last_ldr = ldr;
            drawWidth = map(ldr,0,800,0,220);
            tft.fillRoundRect(1, 83, 220,       20, 10, BLACK);
            tft.drawRoundRect(1, 83, 220,       20, 10, WHITE);
            tft.fillRoundRect(1, 83, drawWidth, 20, 10, WHITE);
            tft.fillRoundRect(1, 104, 190,      20, 10, ILI9341_BLUE);
            tft.setCursor(1,105);
            tft.print("LightSensor ");    
            tft.print(ldr);
        }

        if (last_temperature != temperature)
        {
            last_temperature = temperature;
            drawWidth = map(temperature,0,50,0,220);
            tft.fillRoundRect(1, 124, 220,       20, 10, BLACK);
            tft.drawRoundRect(1, 124, 220,       20, 10, RED);
            tft.fillRoundRect(1, 124, drawWidth, 20, 10, RED);
            tft.fillRoundRect(1, 145, 200,       20, 10, ILI9341_BLUE);
            tft.setCursor(1,146);
            tft.print("Temperature ");    
            tft.print(temperature);
            tft.print("*C");    
        }

        if (last_humidity != humidity)
        {
            last_humidity = humidity;
            drawWidth = map(humidity,0,100,0,220);
            tft.fillRoundRect(1, 165, 220,       20, 10, BLACK);
            tft.drawRoundRect(1, 165, 220,       20, 10, WHITE);
            tft.fillRoundRect(1, 165, drawWidth, 20, 10, WHITE);
            tft.fillRoundRect(1, 186, 150,       20, 10, ILI9341_BLUE);
            tft.setCursor(1,187);
            tft.print("Humidity   ");    
            tft.print(humidity);
            tft.print(" %");    
        }

        if (last_pir != pir)
        {
            last_pir = pir;           
            tft.fillRect(10, 206, 59, 53, ILI9341_BLUE);
            
            if(pir == 1)
            {
                bmpDraw("motion.bmp",10, 206);
            }
        }
       
        if (last_lightbulb != lightbulb)
        {
            last_lightbulb = lightbulb;

            tft.fillRect(90, 206, 108, 48, ILI9341_BLUE);
            if (lightbulb == 0)
            {
                bmpDraw("lightoff.bmp",90, 206);
            }
            else if(lightbulb == 1)
            {
                bmpDraw("lighton.bmp",90, 206);
            }
        }

        if (last_buzzer != buzzer)
        {
            last_buzzer = buzzer;

            tft.fillRect(180, 206, 108, 156, ILI9341_BLUE);
            
            if(buzzer == 1)
            {
                bmpDraw("buzzer.bmp",180, 206);
            }
        }

#if 0
        if (last_pir != pir)
        {
            last_pir = pir;           
            tft.fillRoundRect(30, 206, 180, 20, 10, ILI9341_BLUE);
            
            if (pir == 0)
            {
                tft.fillRoundRect(30, 206, 180, 20, 10, GREEN);
            }
            else if(pir == 1)
            {
                tft.fillRoundRect(30, 206, 180, 20, 10, RED);
            }
            tft.setCursor(40,208);
            tft.print("MotionDetector");    
        }
       
        if (last_lightbulb != lightbulb)
        {
            last_lightbulb = lightbulb;

            tft.fillRoundRect(1, 230, 220,       20, 10, ILI9341_BLUE);
            tft.drawRoundRect(140, 230, 80,        20, 10, BLUE);
            
            if (lightbulb == 0)
            {
                tft.fillRoundRect(140, 230, 80, 20, 10, RED);
                tft.fillRoundRect(140, 230, 40, 20, 10, WHITE);
                tft.setCursor(1,232);
                tft.print("Light Bulb:    OFF");    
            }
            else if(lightbulb == 1)
            {
                tft.fillRoundRect(140, 230, 80, 20, 10, GREEN);
                tft.fillRoundRect(180, 230, 40, 20, 10, WHITE);
                tft.setCursor(1,232);
                tft.print("Light Bulb: ON");    
            }
        }

        if (last_buzzer != buzzer)
        {
            last_buzzer = buzzer;
            drawWidth = map(buzzer,0,100,0,120);
            tft.fillRoundRect(1, 252, 120,       20, 10, BLACK);
            tft.drawRoundRect(1, 252, 120,       20, 10, RED);
            tft.fillRoundRect(1, 252, drawWidth, 20, 10, RED);
            tft.fillRoundRect(1, 273, 120,       20, 10, ILI9341_BLUE);
            tft.setCursor(1,273);
            tft.print("Buzzer: ");    
            tft.print(buzzer);
        }
#endif

        StaticJsonBuffer<200> jsonBuffer;
        StaticJsonBuffer<200> jsonBuffer2;
        JsonObject& root = jsonBuffer.createObject();
        JsonObject& root2 = jsonBuffer2.createObject();

        root["MotionDetector"] = pir;
        root["LightSensor"] = ldr;
        root["MQ2"] = mq2;
        root["MQ135"] = mq135;
    
        root2["EnvData"] = root;

        root2.printTo(Serial);
        Serial.println();
        
        // Send to ESP8266
        root2.printTo(Serial1);
        Serial1.flush();
    }
    delay(10);
    
//unsigned long irsend_cmd = 551502015;
//irsend.sendNEC(irsend_cmd, 32);
//Serial.print("Sending IR cmd:");
//Serial.println(last_ir_cmd_received);
//    for (int i = 0; i < 3; i++) {
//irsend.sendNEC(last_ir_cmd_received, 32);
irsend.sendNEC(0x20DF40BF, 32);
Serial.print("TIMER_PWM_PIN:");
Serial.println(TIMER_PWM_PIN);
//       delay(40);
//   }
 
    if (irrecv.decode(&results))
    {
        unsigned long value = results.value;
        StaticJsonBuffer<200> jsonBuffer3;
        JsonObject& root3 = jsonBuffer3.createObject();
         
        last_ir_cmd_received = value;
        Serial.print("IR value=");
        Serial.print(value, HEX);
        Serial.print(":decode_type=");
        Serial.print(results.decode_type);
        Serial.print(":bits=");
        Serial.print(results.bits);
        Serial.print(":rawlen=");
        Serial.println(results.rawlen);
//        Serial.print("rawbuf=");
//        Serial.println(results.rawbuf);

        switch (value)
        {
            case 16738455: // 0
                break;
            case 16724175: // 1 
                break;
            case 16718055: // 2 
                break;
            case 16743045: // 3
                break;
            case 16716015: // 4
                break;
            case 16726215: // 5
                break;
            case 16734885: // 6
                break;
            case 16748655: // + 
                break;
            case 16754775: // -
                break;
            case 16753245: // "Power"
                if(lightbulb == 0)
                {
                    lightbulb = 1;
                }
                else if(lightbulb == 1)
                {
                    lightbulb = 0;
                }
                else if(lightbulb == -1)
                {
                    lightbulb = 1;
                }
                
                root3["LightBulb"] = (lightbulb)?1:0;
                root3.printTo(Serial);
                Serial.println();
        
               // Send to ESP8266
                root3.printTo(Serial1);
                Serial1.flush();
                break;
            case 16769565: // "Mute"
                break;
            case 16736925: // "Mode"
                break;
            case 16712445: //  |<< (rewind)
                break;    
            case 16761405: //  >>| (forward)
                break;
        }
        irrecv.resume(); // Receives the next value from the button you press
    }

#if 0
    for(i=0;i<100;i++)
    {  
        fade(0);
        delay(100);
    }
   
    for(i=0;i<100;i++)
    {  
        fade(1);
        delay(100);
    }
    
    for(i=0;i<100;i++)
    {  
        fade(2);
        delay(100);
    }

    for(i=0;i<10;i++)
    {  
        int r=random(0,3);
        for(int j=0;j<10;j++)
        {
            fade(r);
        }
        delay(1000);
    }

    for(i=0;i<100;i++)
    {  
        int r=random(0,3);
        fade(r);
        delay(1000);
    }

    value = 0;
    for(i=0;i<100;i++)
    {  
        all_fade();
        delay(1000);
    }

    for(i=0;i<100;i++)
    {  
        random_colors();
        delay(1000);
    }

    larson_scanner(0);
    larson_scanner(1);
    larson_scanner(2);
    larson_scanner_random();
    larson_scanner_random2();

#endif
}

