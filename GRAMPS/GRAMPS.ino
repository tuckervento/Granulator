/*

 Demonstrates the use of the Audio library for the Arduino Due

 Hardware required :
 *Arduino shield with a SD card on CS 4 (the Ethernet sheild will work)
 *Speaker attached to ground and DAC0
*/

#include <SD.h>
#include <SPI.h>
#include <Audio.h>

void setup()
{
  // debug output at 9600 baud
  Serial.begin(9600);

  // setup SD-card
  Serial.print("Initializing SD card...");
  if (!SD.begin(4)) {
    Serial.println(" failed!");
    return;
  }
  Serial.println(" done.");
  // hi-speed SPI transfers
  SPI.setClockDivider(4);

  // 44100 Hz stereo => 88200 sample rate
  // 100 mSec of prebuffering.
  Audio.begin(44100, 200);
}

void loop()
{
  int count=0;

  // open wave file from sdcard
  File myFile = SD.open("test.wav");
  if (!myFile) {
    // if the file didn't open, print an error and stop
    Serial.println("error opening test.wav");
    while (true);
  }

  //Force grain time to 100ms right now
  //So samples/grain = 4410
  //not sure of unsigned short vs uint16_t, etc...
  //i believe unsigned short is faster, but uint16_t is more memory conservative?
  //but i'm not sure
  uint16_t grainTime=100;
  uint8_t grainRepeat=3;
  const uint32_t S=441*(grainTime/10); // Number of samples to read in block
  short buffer[S];

  Serial.print("Playing");
  // until the file is not finished
  while (myFile.available()) {
    // read from the file into buffer
    myFile.read(buffer, sizeof(buffer));

    for (int i = 0; i < grainRepeat; i++) {
      // Prepare samples
      int volume = 1023;
      Audio.prepare(buffer, S, volume);
      // Feed samples to audio
      Audio.write(buffer, S);
  
      // Every 100 block print a '.'
      count++;
      if (count == 100) {
        Serial.print(".");
        count = 0;
      }
    }
  }
  myFile.close();

  Serial.println("End of file. Thank you for listening!");
  while (true) ;
}
