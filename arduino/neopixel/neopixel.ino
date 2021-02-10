/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution>
*********************************************************************/

// This sketch is intended to be used with the NeoPixel control
// surface in Adafruit's Bluefruit LE Connect mobile application.
//
// - Compile and flash this sketch to the nRF52 Feather
// - Open the Bluefruit LE Connect app
// - Switch to the NeoPixel utility
// - Click the 'connect' button to establish a connection and
//   send the meta-data about the pixel layout
// - Use the NeoPixel utility to update the pixels on your device

/* NOTE: This sketch required at least version 1.1.0 of Adafruit_Neopixel !!! */

// JA: TOTAL HACK -- probalby want to go back to a binary format to improve bluetooth exchanges

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <bluefruit.h>

#define SILENT
#define NEOPIXEL_VERSION_STRING "Neopixel v2.0"
#define PIN                     30   /* Pin used to drive the NeoPixels */

#define MAXCOMPONENTS  4
uint8_t *pixelBuffer = NULL;

// JA HARD CODE SETUP
#define STATIC_SETUP

#ifdef STATIC_SETUP
#define STATIC_SETUP_WIDTH 8
#define STATIC_SETUP_HEIGHT 4
#define STATIC_SETUP_STRIDE 8
#define STATIC_SETUP_COMPONENT_VALUE 82
#define STATIC_SETUP_IS400HZ 0

uint8_t width = STATIC_SETUP_WIDTH;
uint8_t height = STATIC_SETUP_HEIGHT;
uint8_t stride = STATIC_SETUP_STRIDE;
uint8_t componentsValue = STATIC_SETUP_COMPONENT_VALUE;
bool is400Hz = STATIC_SETUP_IS400HZ;

#else
uint8_t width = 0;
uint8_t height = 0;
uint8_t stride;
uint8_t componentsValue;
bool is400Hz;
#endif

uint8_t components = 3;     // only 3 and 4 are valid values

Adafruit_NeoPixel neopixel = Adafruit_NeoPixel();

// BLE Service
BLEDfu  bledfu;
BLEDis  bledis;
BLEUart bleuart;

// largest line could include a full pixel image
// I XXX XXX XXX ...
// 4 * 3 * 4 * 8 + 1 + 1 = 386
#define MAX_CMD_LEN 396
char bleCommand[MAX_CMD_LEN+1]; // +1 for terminator
bool bleCmd;
size_t bleCmdLen;

void flash(uint8_t r=128, uint8_t g=128, uint8_t b=128, uint8_t brightness=255, int delay=50)
{
  int pi = 0;
  neopixel.setBrightness(brightness);
  for (int j = 0; j < height; j++)
  {
    for (int i = 0; i < width; i++) { 
     neopixel.setPixelColor(pi, neopixel.Color(r, g, b));
     neopixel.show();
     delayMicroseconds(delay); 
     neopixel.setPixelColor(pi, neopixel.Color(0, 0, 0));
     neopixel.show();
     pi++;
    }
    pi += stride - width;  
  }
}
void setup()
{
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb

  Serial.println("Flashy 0.2");
  Serial.println("----------");

  
  // Config Neopixels
  neopixel.begin();

#ifdef STATIC_SETUP
  commandSetup(false);
  flash(0,0,255);
#endif
  
  // Init Bluefruit
  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName("Bluefruit52");
  Bluefruit.Periph.setConnectCallback(connect_callback);

  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin();

  // Configure and Start Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();  

  // Configure and start BLE UART service
  bleuart.begin();

  // Set up and start advertising
  startAdv();
}

void startAdv(void)
{  
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  
  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  flash(0,255,0);
  
  Serial.print("Connected to ");
  Serial.println(central_name);
  resetCmd();                    // get ready for first command;
}

void resetCmd() {
  bleCommand[0]=0;
  bleCmd = false;
  bleCmdLen = 0;
}

void readCmd() 
{
  if (bleCmd == true) return;
  while (bleuart.available()) {
   char c = bleuart.read();
    if  (c=='\n') {
      bleCommand[bleCmdLen] = 0;
      if  (bleCmdLen<MAX_CMD_LEN) bleCmdLen++;
      bleCmd=true;
   } else {
      bleCommand[bleCmdLen] = c;
      if (bleCmdLen<MAX_CMD_LEN) bleCmdLen++;
   }
   // Serial.printf("%d: c=%c bleCommand[%d]=%c\n", bleCmdLen, c, bleCmdLen-1, bleCommand[bleCmdLen-1]);
  }
}

void printCmd() 
{
   Serial.printf("command: bleCmd:%d bleCmdLen:%d\n", bleCmd, bleCmdLen);
   for (int i=0; i<bleCmdLen;i++) {
      Serial.write(bleCommand[i]);
   }
   Serial.println();
}
void loop()
{
  // Echo received data
  if ( Bluefruit.connected() && bleuart.notifyEnabled() )
  {
#if 0
    size_t len = bleuart.readBytesUntil('\n', bleCommand, MAX_CMD_LEN);
    bleCommand[len] = 0; // null terminate line readBytesUntil removed '\n'
    if (len==0) return; // nothing to do yet
#else
   readCmd();
   if (bleCmd == false) return;
#endif   
#ifndef SILENT    
    printCmd();
#endif
    
    switch (bleCommand[0]) {
      case 'V': {   // Get Version
          commandVersion();
          break;
        }
  
      case 'S': {   // Setup dimensions, components, stride...
          commandSetup(true);
          break;
       }

      case 'C': {   // Clear with color
          commandClearColor();
          break;
      }

      case 'B': {   // Set Brightness
          commandSetBrightness();
          break;
      }
            
      case 'P': {   // Set Pixel
          commandSetPixel();
          break;
      }
  
      case 'I': {   // Receive new image
          commandImage();
          break;
       }
      case 'F': {  // flash
        commandFlash();
        break;
      }
    }
    resetCmd();
  }
}

void swapBuffers()
{
  uint8_t *base_addr = pixelBuffer;
  int pixelIndex = 0;
  for (int j = 0; j < height; j++)
  {
    for (int i = 0; i < width; i++) {
      if (components == 3) {
        neopixel.setPixelColor(pixelIndex, neopixel.Color(*base_addr, *(base_addr+1), *(base_addr+2)));
      }
      else {
        neopixel.setPixelColor(pixelIndex, neopixel.Color(*base_addr, *(base_addr+1), *(base_addr+2), *(base_addr+3) ));
      }
      base_addr+=components;
      pixelIndex++;
    }
    pixelIndex += stride - width;   // Move pixelIndex to the next row (take into account the stride)
  }
  neopixel.show();

}

void commandFlash() {
#ifndef SILENT
  Serial.println(F("Command: Flash"));
#endif
  uint8_t r, g, b, brightness; 
  int delay;
  if (sscanf(&bleCommand[1], "%d %d %d %d %d", &r, &g, &b, &brightness, &delay) != 5) {
    flash(); 
  } else {
    flash(r,g,b,brightness,delay);
  }
  sendResponse("OK");
}

void commandVersion() {
  Serial.println(F("Command: Version check"));
  sendResponse(NEOPIXEL_VERSION_STRING);
}

void commandSetup(bool connected) {
  Serial.println(F("Command: Setup"));

#ifndef STATIC_SETUP
  width = bleuart.read();
  height = bleuart.read();
  stride = bleuart.read();
  componentsValue = bleuart.read();
  is400Hz = bleuart.read();
#else
  width = STATIC_SETUP_WIDTH;
  height = STATIC_SETUP_HEIGHT;
  stride = STATIC_SETUP_STRIDE;
  componentsValue = STATIC_SETUP_COMPONENT_VALUE;
  is400Hz = STATIC_SETUP_IS400HZ;
 #endif  
  neoPixelType pixelType;
  pixelType = componentsValue + (is400Hz ? NEO_KHZ400 : NEO_KHZ800);

  components = (componentsValue == NEO_RGB || componentsValue == NEO_RBG || componentsValue == NEO_GRB || componentsValue == NEO_GBR || componentsValue == NEO_BRG || componentsValue == NEO_BGR) ? 3:4;
  
  Serial.printf("\tsize: %dx%d\n", width, height);
  Serial.printf("\tstride: %d\n", stride);
  Serial.printf("\tpixelType %d\n", pixelType);
  Serial.printf("\tcomponentsValue: %d\n", componentsValue);
  Serial.printf("\tis400Hz: %d\n", is400Hz);
  Serial.printf("\tcomponents: %d\n", components);

  if (pixelBuffer != NULL) {
      delete[] pixelBuffer;
  }

  uint32_t size = width*height;
  pixelBuffer = new uint8_t[size*components];
  neopixel.updateLength(size);
  neopixel.updateType(pixelType);
  neopixel.setPin(PIN);

  // Done
   if ( connected )  sendResponse("OK");
}

void commandSetBrightness() {
#ifndef SILENT  
  Serial.println(F("Command: SetBrightness"));
#endif

   // Read value
  //uint8_t brightness = bleuart.read();
  uint8_t brightness;

  if (sscanf(&bleCommand[1], "%d", &brightness) != 1) {
    Serial.println(F("ERROR: B <VALUE>"));
    sendResponse("ERROR");
    return;
  }
  
  // Set brightness
  neopixel.setBrightness(brightness);

#ifndef SILENT
  Serial.printf("\tbrightness: %d\n", brightness);
#endif
  
  // Refresh pixels
  swapBuffers();

  // Done
  sendResponse("OK");
}

void commandClearColor() {
#ifndef SILENT
  Serial.println(F("Command: ClearColor"));
#endif
  // Read color
  uint8_t color[MAXCOMPONENTS];

  if (components == 3 ) {
    if (sscanf(&bleCommand[1], "%d %d %d\n", &color[0], &color[1], &color[2]) != 3) {
      Serial.println(F("ERROR: C <R> <G> <B>"));
      sendResponse("ERROR");
      return;
    }
  } else {
    if (sscanf(&bleCommand[1], "%d %d %d %d\n", &color[0], &color[1], &color[2], &color[3]) != 4) {
      Serial.println(F("ERROR: C <R> <G> <B> <I>"));
      sendResponse("ERROR");
      return; 
    }
  }

  // Set all leds to color
  int size = width * height;
  uint8_t *base_addr = pixelBuffer;
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < components; j++) {
      *base_addr = color[j];
      base_addr++;
    }
  }

  // Swap buffers
#ifndef SILENT
  Serial.println(F("ClearColor completed"));
#endif
  
  swapBuffers();

#ifndef SILENT
  if (components == 3) {
    Serial.printf("\tclear (%d, %d, %d)\n", color[0], color[1], color[2]);
  }
  else {
    Serial.printf("\tclear (%d, %d, %d, %d)\n", color[0], color[1], color[2], color[3]);
  }
#endif
  // Done
  sendResponse("OK");
}

void commandSetPixel() {
#ifndef SILENT  
  Serial.println(F("Command: SetPixel"));
#endif
  
  // Read position
  uint8_t x;
  uint8_t y;
  uint8_t icolor[MAXCOMPONENTS];

  if (components == 3) {
    if (sscanf(&bleCommand[1], "%d %d %d %d %d", &x, &y, &icolor[0], &icolor[1], &icolor[2] ) != 5) {
      Serial.println(F("ERROR: P <X> <Y> <R> <G> <B>"));
      Serial.println(bleCommand);
      sendResponse("ERROR");
      return;
    }
  } else {
    if (sscanf(&bleCommand[1], "%d %d %d %d %d %d", &x, &y, &icolor[0], &icolor[1], &icolor[2], &icolor[3] ) != 6) {
      Serial.println(F("ERROR: P <X> <Y> <R> <G> <B> <I>"));
      sendResponse("ERROR");
      return; 
    }
  }
  
  
  // Read colors
  uint32_t pixelOffset = y*width+x;
  uint32_t pixelDataOffset = pixelOffset*components;
  uint8_t *base_addr = pixelBuffer+pixelDataOffset;
  
  for (int j = 0; j < components;) {
      *base_addr = icolor[j];
      base_addr++;
      j++;
  }

  // Set colors
  uint32_t neopixelIndex = y*stride+x;
  uint8_t *pixelBufferPointer = pixelBuffer + pixelDataOffset;
  uint32_t color;
  if (components == 3) {
    color = neopixel.Color( *pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2) );
#ifndef SILENT
    Serial.printf("\t%d %d color (%d, %d, %d)\n",x, y, *pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2) );
#endif
  }
  else {
    color = neopixel.Color( *pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2), *(pixelBufferPointer+3) );
#ifndef SILENT    
    Serial.printf("\tcolor (%d, %d, %d, %d)\n", *pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2), *(pixelBufferPointer+3) );    
#endif    
  }
  neopixel.setPixelColor(neopixelIndex, color);
  neopixel.show();

  // Done
  sendResponse("OK");
}

void commandImage() {
#ifndef SILENT
  Serial.printf("Command: Image %dx%d, %d, %d\n", width, height, components, stride);
  Serial.flush();
#endif  

  // Receive new pixel buffer
  int size = width * height;
  char *next = &bleCommand[1] ;
  char *start;
  uint8_t *base_addr = pixelBuffer;
  uint8_t val;
  char c;
   
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < components; j++) {
        c = *next;
        if (c == 0) goto done;
        start = next;
        val = (int8_t)strtol(start, &next, 10);
        if (start == next) goto done;
        *base_addr = val;     
        //Serial.printf("%d.%d: %d *next=c\n", i,j, *base_addr, *next);
        base_addr++;
      }
    }
done:
/*
    if (components == 3) {
      uint32_t index = i*components;
      Serial.printf("\tp%d (%d, %d, %d)\n", i, pixelBuffer[index], pixelBuffer[index+1], pixelBuffer[index+2] );
    }
    */

  // Swap buffers
#ifndef SILENT  
  Serial.println(F("Image received"));
  Serial.flush();
#endif
  swapBuffers();
  // Done
  sendResponse("OK");
}

void sendResponse(char const *response) {
#ifndef SILENT  
    Serial.printf("Send Response: %s\n", response);
#endif
    bleuart.flush();    
    bleuart.write(response, strlen(response)*sizeof(char));
    bleuart.flush();  
    
}
