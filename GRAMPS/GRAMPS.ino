#include <SdFat.h>
#include <SPI.h>
#include <Audio.h>

SdFat SD;

void setup()
{
  // debug output at 9600 baud
  Serial.begin(9600);

  // setup SD-card
  Serial.print("Initializing SD card...");
  if (!SD.begin(8)) {
    Serial.println(" failed!");
    return;
  }
  Serial.println(" done.");
  // hi-speed SPI transfers
  SPI.setClockDivider(4);

  Audio.begin(44100, 300);
}

void loop()
{
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
  uint8_t grainRepeat=4;
  const uint32_t S=441*(grainTime/10); // Number of samples to read in block
  int16_t buf[S];
  uint16_t volume = 1023;

  uint8_t attackSetting = 40, attackCounter = 0;
  uint8_t releaseSetting = 0, releaseCounter = 0;

  uint32_t attackSamples = S / attackSetting, releaseSamples = S / releaseSetting;

  Serial.print("Playing");
  // until the file is not finished
  while (myFile.available()) {
    // read from the file into buffer
    myFile.read(buf, sizeof(buf));

    for (attackCounter = 0; attackCounter < attackSamples; attackCounter++) {
      buf[attackCounter] = (float)buf[attackCounter]*((float)attackCounter/(float)attackSamples);
    }

    for (releaseCounter = 0; releaseCounter < releaseSamples; releaseCounter++) {
      buf[S-releaseCounter-1] = (float)buf[S-releaseCounter-1]*((float)releaseCounter/(float)releaseSamples);
    }

    Audio.prepare(buf, S, volume);
    for (int i = 0; i < grainRepeat; i++) {
      // Prepare samples
      // Feed samples to audio
      Audio.write(buf, S);
    }
  }
  myFile.close();
}
