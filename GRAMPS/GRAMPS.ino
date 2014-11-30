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
  if (!SD.begin(8, 4)) {
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
  uint16_t grainTime=200;
  uint8_t grainRepeat=1;
  const uint16_t S = 441*(grainTime/10); // Number of samples to read in block
  const uint16_t B = 1024; //fix at 1024 for now
  int16_t buf[B];
  uint16_t volume = 1023;

  uint8_t attackSetting = 30, releaseSetting = 30;

  uint16_t attackSamples = S * (attackSetting / 100.0), releaseSamples = S * (releaseSetting / 100.0);
  uint16_t relativeEnvelopeCounter, attackCounter = 0, releaseCounter = 0;
  
  uint16_t samplesRemaining = S, samplesToRead = B;

  Serial.println(S);
  Serial.println(attackSamples);
  Serial.println(releaseSamples);
  Serial.println("Playing");
  // until the file is not finished
  while (myFile.available()) {
    //reset counters
    samplesToRead = B;
    samplesRemaining = S;
    attackCounter = 0;
    releaseCounter = 0;
    
    while (samplesRemaining > 0) {
      //read into buffer
      myFile.read(buf, samplesToRead*2);
      
      if (attackCounter < attackSamples) {
        for (relativeEnvelopeCounter = 0; relativeEnvelopeCounter < samplesToRead && attackCounter < attackSamples; relativeAttackCounter++) {
          buf[relativeEnvelopeCounter] *= (attackCounter/(float)attackSamples);
          attackCounter++;
        }
      }
      
      if (releaseCounter < attackSamples) {
        for (relativeEnvelopeCounter = samplesToRead - 1; relativeEnvelopeCounter > 0 && attackCounter < attackSamples; relativeAttackCounter--) {
          buf[samplesToRead - relativeEnvelopeCounter - 1] *= (attackCounter/(float)attackSamples);
          releaseCounter++;
        }
      }
  
      /*for (releaseCounter = 0; releaseCounter < releaseSamples; releaseCounter++) {
        buf[S-releaseCounter-1] = (float)buf[S-releaseCounter-1]*((float)releaseCounter/(float)releaseSamples);
      }*/
  
      Audio.prepare(buf, samplesToRead, volume);
      Audio.write(buf, samplesToRead);
      
      //adjust samples remaining
      samplesRemaining -= samplesToRead;
      if (samplesRemaining < samplesToRead) { samplesToRead = samplesRemaining; }
    }
  }
  myFile.close();
}
