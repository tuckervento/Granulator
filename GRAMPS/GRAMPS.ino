#include <SdFat.h>
#include <SPI.h>
#include <Audio.h>

// the flags
#define SEEKING    0x01  // 0000 0001
#define REVERSE    0x02  // 0000 0010

// macros to manipulate the flags
#define RESET_FLAGS(x)     (x = 0x00)

#define SEEK_ON(x)    (x |= SEEKING)
#define SEEK_OFF(x)    (x &= ~SEEKING)

#define REVERSE_ON(x)    (x |= REVERSE)
#define REVERSE_OFF(x)    (x &= ~REVERSE)

// these evaluate to non-zero if the flag is set
#define IS_SEEKING(x)      (x & SEEKING)
#define IS_REVERSE(x)      (x & REVERSE)

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

void reverseBuffer(int16_t* p_buf, uint16_t p_size) {
    uint16_t i = 0;
    uint16_t j = p_size-1;
    int16_t d;
    while (i < j) {
        d = p_buf[i];
        p_buf[i++] = p_buf[j];
        p_buf[j--] = d;
    }
}

void loop()
{
  // open wave file from sdcard
  File wavFile = SD.open("testtail.wav");
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

  uint8_t statusBits = 0x00;

  //force seek
  SEEK_OFF(statusBits);
  REVERSE_ON(statusBits);

  Serial.println("Playing");
  // until the file is not finished
  while (wavFile.available()) {
    //reset counters
    samplesToRead = B;
    samplesRemaining = S;
    attackCounter = 0;
    decayCounter = decaySamples;
    segmentCounter = 1;
    
    while (samplesRemaining > 0) {
      //detect if we should reverse
      //if the reverse bit is set we need to seek appropriately
      //but we only need to do that when we're not reading the start of the grain
      //(which would be indicated by being the "leftover" != B)
      if (IS_REVERSE(statusBits) && samplesToRead != B) {
        wavFile.seek(grainPosition + (S - segmentCounter * B) * 2);
      }
      //read into buffer
      wavFile.read(buf, samplesToRead*2);
      if (IS_REVERSE(statusBits)) {
        reverseBuffer(buf, samplesToRead);
      }
      
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
      if (IS_SEEKING(statusBits)) {
        wavFile.seek(grainPosition + (grainRepeat - 1)*S*2);
      }
    }
  }
  wavFile.close();
}
