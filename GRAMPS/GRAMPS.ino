#include <SdFat.h>
#include <SPI.h>
#include <Audio.h>

//the flags
//STATUS
#define PLAYING    0x01
#define SEEKING    0x02
#define REVERSE    0x04

//PARAMCHANGE
#define GRAINTIME     0x01
#define GRAINREPEAT   0x02
#define ATTACKSETTING 0x04
#define DECAYSETTING  0x08

//macros to manipulate the flags
#define RESET_FLAGS(x)     (x = 0x00)

//STATUS
#define PLAYING_ON(x)  (x |= PLAYING)
#define PLAYING_OFF(x) (x &= ~PLAYING)
#define SEEK_ON(x)     (x |= SEEKING)
#define SEEK_OFF(x)    (x &= ~SEEKING)
#define REVERSE_ON(x)  (x |= REVERSE)
#define REVERSE_OFF(x) (x &= ~REVERSE)

//PARAMCHANGE
#define GRAINTIME_CHANGED(x)     (x |= GRAINTIME)
#define GRAINTIME_HANDLED(x)     (x &= ~GRAINTIME)
#define GRAINREPEAT_CHANGED(x)   (x |= GRAINREPEAT)
#define GRAINREPEAT_HANDLED(x)   (x &= ~GRAINREPEAT)
#define ATTACKSETTING_CHANGED(x) (x |= ATTACKSETTING)
#define ATTACKSETTING_HANDLED(x) (x &= ~ATTACKSETTING)
#define DECAYSETTING_CHANGED(x)  (x |= DECAYSETTING)
#define DECAYSETTING_HANDLED(x)  (x &= ~DECAYSETTING)


//flag-checkers
//STATUS
#define IS_PLAYING(x)      (x & PLAYING)
#define IS_SEEKING(x)      (x & SEEKING)
#define IS_REVERSE(x)      (x & REVERSE)

//PARAMCHANGE
#define IS_GRAINTIME(x)      (x & GRAINTIME)
#define IS_GRAINREPEAT(x)    (x & GRAINREPEAT)
#define IS_ATTACKSETTING(x)  (x & ATTACKSETTING)
#define IS_DECAYSETTING(x)   (x & DECAYSETTING)


SdFat SD;

uint8_t _statusBits = 0x00;
uint8_t _paramChangeBits = 0x00;

//pins
uint8_t _buttonPlay = 53;
uint8_t _potVolume = A0;

//parameters
const uint16_t B = 1024; //fixed buffer size for segmentation

uint16_t _volume = 1023;

uint16_t _grainTime = 500;
uint8_t _attackSetting = 1, _decaySetting = 1, _grainRepeat = 1;
unsigned long _grainPosition;

File _wavFile;

void initInput()
{
  pinMode(_buttonPlay, INPUT);
  attachInterrupt(_buttonPlay, checkButtonPlay, CHANGE);
}

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

  initInput();

  Audio.begin(44100, 300);
}

void checkButtonPlay()
{
  digitalRead(_buttonPlay) ? PLAYING_ON(_statusBits) : PLAYING_OFF(_statusBits);
  if (IS_PLAYING(_statusBits)) {
    Serial.println("play button checked");
  }
  else {
    Serial.println("play off");
  }
}

void reverseBuffer(int16_t* p_buf, uint16_t p_size)
{
    uint16_t i = 0;
    uint16_t j = p_size-1;
    int16_t d;
    while (i < j) {
        d = p_buf[i];
        p_buf[i++] = p_buf[j];
        p_buf[j--] = d;
    }
}

void granulate()
{
  //Force grain time to 100ms right now
  //So samples/grain = 4410
  //not sure of unsigned short vs uint16_t, etc...
  //i believe unsigned short is faster, but uint16_t is more memory conservative?
  //but i'm not sure
  uint8_t grainWriteCounter = 0;
  uint16_t S = 441*(_grainTime/10); // Number of samples to read in block
  int16_t buf[B];

  uint16_t attackSamples = S * (_attackSetting / 100.0), decaySamples = S * (_decaySetting / 100.0);
  uint16_t relativeEnvelopeCounter, attackCounter = 0, decayCounter = 0, decayDifference;
  
  uint16_t samplesRemaining = S, samplesToRead = B;
  _grainPosition = _wavFile.position();
  uint8_t segmentCounter = 1;

  Serial.println("Playing");
  // until the file is not finished
  while (_wavFile.available()) { //start of grain
    if (_paramChangeBits != 0x00) { //if we have a change, re-calculate necessary parameters
      if (GRAINTIME_CHANGED(_paramChangeBits)) {
        S = 441*(_grainTime/10);
        attackSamples = S * (_attackSetting / 100.0);
        decaySamples = S * (_decaySetting / 100.0);
        GRAINTIME_HANDLED(_paramChangeBits);
      }
      if (GRAINREPEAT_CHANGED(_paramChangeBits)) {
        //nothing to do?
        GRAINREPEAT_HANDLED(_paramChangeBits);
      }
      if (ATTACKSETTING_CHANGED(_paramChangeBits)) {
        attackSamples = S * (_attackSetting / 100.0);
        ATTACKSETTING_HANDLED(_paramChangeBits);
      }
      if (DECAYSETTING_CHANGED(_paramChangeBits)) {
        decaySamples = S * (_decaySetting / 100.0);
        DECAYSETTING_HANDLED(_paramChangeBits);
      }
    }
    //reset counters
    samplesToRead = B;
    samplesRemaining = S;
    attackCounter = 0;
    decayCounter = decaySamples;
    segmentCounter = 1;
    
    while (samplesRemaining > 0) { //start of segment
      if (!IS_PLAYING(_statusBits)) {
        return;
      }
      //detect if we should reverse
      //if the reverse bit is set we need to seek appropriately
      //but we only need to do that when we're not reading the start of the grain
      //(which would be indicated by being the "leftover" != B)
      if (IS_REVERSE(_statusBits) && samplesToRead == B) {
        _wavFile.seek(_grainPosition + ((S - (segmentCounter * B)) * 2));
      }
      //read into buffer
      _wavFile.read(buf, samplesToRead*2);
      if (IS_REVERSE(_statusBits)) {
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
  
      Audio.prepare(buf, samplesToRead, _volume);
      Audio.write(buf, samplesToRead);
      
      segmentCounter++;
      //adjust samples remaining
      samplesRemaining -= samplesToRead;
      if (samplesRemaining < samplesToRead) { samplesToRead = samplesRemaining; }
    }

    grainWriteCounter++;
    //seek back if we need to re-do this grain
    if (grainWriteCounter < _grainRepeat) {
      _wavFile.seek(_grainPosition);
    }
    else {
      grainWriteCounter = 0;
      if (IS_SEEKING(_statusBits)) {
        _wavFile.seek(_grainPosition + (_grainRepeat*S*2));
      }
      _grainPosition = _wavFile.position();
    }
  }
}

void loop()
{
  // open wave file from sdcard
  _wavFile = SD.open("testtail.wav");
  if (!_wavFile) {
    // if the file didn't open, print an error and stop
    Serial.println("error opening test.wav");
    while (true);
  }

  while(true) {
    _wavFile.seek(0);
    granulate();
    while(PLAYING_OFF(_statusBits));
  }
  
  _wavFile.close();
}
