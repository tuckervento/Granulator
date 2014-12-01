#include <SdFat.h>
#include <SPI.h>
#include <Audio.h>
#include <MuxShield.h>

//initialize SD and MuxShield libraries
SdFat SD;
MuxShield muxShield;

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

  //configure mux rows
  muxShield.setMode(2, ANALOG_IN);
  muxShield.setMode(3, DIGITAL_IN);
}

void checkInputs()
{
  while (!digitalReadMS(3, 0)) {
    //play button is off
  }
}

void loop()
{
  checkInputs();
  // open wave file from sdcard
  File wavFile = SD.open("test.wav");
  if (!wavFile) {
    // if the file didn't open, print an error and stop
    Serial.println("error opening test.wav");
    while (true);
  }
  
  //Force grain time to 100ms right now
  //So samples/grain = 4410
  //not sure of unsigned short vs uint16_t, etc...
  //i believe unsigned short is faster, but uint16_t is more memory conservative?
  //but i'm not sure
  uint16_t grainTime = 100;
  uint8_t grainRepeat = 2, grainWriteCounter = 0;
  const uint16_t S = 441*(grainTime/10); // Number of samples to read in block
  const uint16_t B = 1024; //fix at 1024 for now
  int16_t buf[B];
  uint16_t volume = 1023;

  uint8_t attackSetting = 0, decaySetting = 90;

  uint16_t attackSamples = S * (attackSetting / 100.0), decaySamples = S * (decaySetting / 100.0);
  uint16_t relativeEnvelopeCounter, attackCounter = 0, decayCounter = 0, decayDifference;
  
  uint16_t samplesRemaining = S, samplesToRead = B;
  unsigned long grainPosition = wavFile.position();
  uint8_t segmentCounter = 1;

  Serial.println("Playing");
  // until the file is not finished
  while (wavFile.available()) {
    //reset counters
    samplesToRead = B;
    samplesRemaining = S;
    attackCounter = 0;
    decayCounter = decaySamples;
    segmentCounter = 1;
    
    checkInputs();
    while (samplesRemaining > 0) {
      //read into buffer
      wavFile.read(buf, samplesToRead*2);
      
      if (attackCounter < attackSamples) {
        for (relativeEnvelopeCounter = 0; relativeEnvelopeCounter < samplesToRead && attackCounter < attackSamples; relativeEnvelopeCounter++) {
          buf[relativeEnvelopeCounter] *= (attackCounter/(float)attackSamples);
          attackCounter++;
        }
      }
      
      //detect if we're in decay zone
      //if the decayCounter is a different value, we've already started calculation
      if ((decayCounter != decaySamples) || (S - decaySamples) < (B * segmentCounter)) {
        decayDifference = (B * segmentCounter - (S - decaySamples));
        for (relativeEnvelopeCounter = (decayDifference < B) ? (B - decayDifference - 1) : 0;
          relativeEnvelopeCounter < samplesToRead && decayCounter > 0; relativeEnvelopeCounter++) {
          decayCounter--;
          buf[relativeEnvelopeCounter] *= (decayCounter/(float)decaySamples);
        }
      }
  
      Audio.prepare(buf, samplesToRead, volume);
      Audio.write(buf, samplesToRead);
      
      segmentCounter++;
      //adjust samples remaining
      samplesRemaining -= samplesToRead;
      if (samplesRemaining < samplesToRead) { samplesToRead = samplesRemaining; }
    }

    grainWriteCounter++;
    //seek back if we need to re-do this grain
    if (grainWriteCounter < grainRepeat) {
      wavFile.seek(grainPosition);
    }
    else {
      grainWriteCounter = 0;
      grainPosition = wavFile.position();
    }
  }
  wavFile.close();
}
