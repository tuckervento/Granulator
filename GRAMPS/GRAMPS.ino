#include <SdFat.h>
#include <SPI.h>
#include <Audio.h>
#include <math.h>

//the flags
//STATUS
#define PLAYING 0x01
#define SEEKING 0x02
#define REVERSE 0x04
#define PAUSED  0x08

//PARAMCHANGE
#define GRAINTIME     0x01
#define GRAINREPEAT   0x02
#define ATTACKSETTING 0x04
#define DECAYSETTING  0x08
#define PAUSELENGTH   0x10
#define PAUSEPOINT    0x20
#define TIMESTRETCH   0x40
#define FILENAME      0x80

//macros to manipulate the flags
#define RESET_FLAGS(x)  (x = 0x00)

//STATUS
#define PLAYING_ON(x)   (x |= PLAYING)
#define PLAYING_OFF(x)  (x &= ~PLAYING)
#define SEEK_ON(x)      (x |= SEEKING)
#define SEEK_OFF(x)     (x &= ~SEEKING)
#define REVERSE_ON(x)   (x |= REVERSE)
#define REVERSE_OFF(x)  (x &= ~REVERSE)
#define PAUSED_ON(x)    (x |= PAUSED)
#define PAUSED_OFF(x)   (x &= ~PAUSED)


//PARAMCHANGE
#define GRAINTIME_SIGNAL(x)     (x |= GRAINTIME)
#define GRAINTIME_HANDLE(x)     (x &= ~GRAINTIME)
#define GRAINREPEAT_SIGNAL(x)   (x |= GRAINREPEAT)
#define GRAINREPEAT_HANDLE(x)   (x &= ~GRAINREPEAT)
#define ATTACKSETTING_SIGNAL(x) (x |= ATTACKSETTING)
#define ATTACKSETTING_HANDLE(x) (x &= ~ATTACKSETTING)
#define DECAYSETTING_SIGNAL(x)  (x |= DECAYSETTING)
#define DECAYSETTING_HANDLE(x)  (x &= ~DECAYSETTING)
#define PAUSELENGTH_SIGNAL(x)   (x |= PAUSELENGTH)
#define PAUSELENGTH_HANDLE(x)   (x &= ~PAUSELENGTH)
#define PAUSEPOINT_SIGNAL(x)    (x |= PAUSEPOINT)
#define PAUSEPOINT_HANDLE(x)    (x &= ~PAUSEPOINT)
#define TIMESTRETCH_SIGNAL(x)   (x |= TIMESTRETCH)
#define TIMESTRETCH_HANDLE(x)   (x &= ~TIMESTRETCH)
#define FILENAME_SIGNAL(x)      (x |= FILENAME)
#define FILENAME_HANDLE(x)      (x &= ~FILENAME)

//flag-checkers
//STATUS
#define IS_PLAYING(x) (x & PLAYING)
#define IS_SEEKING(x) (x & SEEKING)
#define IS_REVERSE(x) (x & REVERSE)
#define IS_PAUSED(x)  (x & PAUSED)

//PARAMCHANGE
#define DID_GRAINTIME(x)      (x & GRAINTIME)
#define DID_GRAINREPEAT(x)    (x & GRAINREPEAT)
#define DID_ATTACKSETTING(x)  (x & ATTACKSETTING)
#define DID_DECAYSETTING(x)   (x & DECAYSETTING)
#define DID_PAUSELENGTH(x)    (x & PAUSELENGTH)
#define DID_PAUSEPOINT(x)     (x & PAUSEPOINT)
#define DID_TIMESTRETCH(x)    (x & TIMESTRETCH)
#define DID_FILENAME(x)       (x & FILENAME)

SdFat SD;

uint8_t _statusBits = 0x00;
uint8_t _paramChangeBits = 0x00;

//pins
uint8_t _buttonPlay = 53, _buttonSeek = 51, _buttonReverse = 49, _buttonPause = 47, _buttonResetParams = 45, _buttonFilename3 = 52, _buttonFilename2 = 50, _buttonFilename1 = 48, _buttonFilename0 = 46, _buttonFilenameGo = 44;
uint8_t _potVolume = A0, _potGrainTime = A1, _potGrainRepeat = A2, _potAttackSetting = A3, _potDecaySetting = A4, _potPauseLength = A5, _potPausePoint = A6, _potTimestretch = A7;//extra:A8
uint16_t _potVolumePrevVal = 0, _potGrainTimePrevVal = 0, _potGrainRepeatPrevVal = 0, _potAttackSettingPrevVal = 0, _potDecaySettingPrevVal = 0, _potPauseLengthPrevVal = 0, _potPausePointPrevVal = 0, _potTimestretchPrevVal = 0;
//parameters
const uint16_t B = 1024; //fixed buffer size for segmentation

//filenames...
const char* NAME00 = "0000";
const char* NAME01 = "0001";
const char* NAME02 = "0010";
const char* NAME03 = "0011";
const char* NAME04 = "0100";
const char* NAME05 = "0101";
const char* NAME06 = "0110";
const char* NAME07 = "0111";
const char* NAME08 = "1000";
const char* NAME09 = "1001";
const char* NAME10 = "1010";
const char* NAME11 = "1011";
const char* NAME12 = "1100";
const char* NAME13 = "1101";
const char* NAME14 = "1110";
const char* NAME15 = "1111";

uint8_t _nameSelect = 0x00;

uint16_t _volume = 1023;

uint16_t _grainTime = 500;
uint8_t _attackSetting = 1, _decaySetting = 1, _grainRepeat = 1, _pauseLoopLength = 2, _timestretchValue = 100;
unsigned long _grainPosition, _pauseLoopPoint;

File _wavFile;

void initInput()
{
  pinMode(_buttonPlay, INPUT);
  pinMode(_buttonSeek, INPUT);
  pinMode(_buttonReverse, INPUT);
  pinMode(_buttonPause, INPUT);
  pinMode(_buttonResetParams, INPUT);
  pinMode(_buttonFilename0, INPUT);
  pinMode(_buttonFilename1, INPUT);
  pinMode(_buttonFilename2, INPUT);
  pinMode(_buttonFilename3, INPUT);
  pinMode(_buttonFilenameGo, INPUT);
  attachInterrupt(_buttonPlay, checkButtonPlay, CHANGE);
  attachInterrupt(_buttonSeek, checkButtonSeek, CHANGE);
  attachInterrupt(_buttonReverse, checkButtonReverse, CHANGE);
  attachInterrupt(_buttonPause, checkButtonPause, CHANGE);
  attachInterrupt(_buttonResetParams, resetParams, RISING);
  attachInterrupt(_buttonFilenameGo, checkButtonFilename, RISING);
  //analogReadResolution(12); seems like we don't want this much accuracy if our circuits are so noisy
  checkParams();
}

void setup()
{
  // debug output at 9600 baud
  Serial.begin(9600);

  // setup SD-card
  Serial.print("SD");
  if (!SD.begin(8, 4)) {
    Serial.println(" NO");
    return;
  }
  Serial.println(" OK");
  // hi-speed SPI transfers
  SPI.setClockDivider(4);

  initInput();

  Audio.begin(44100, 300);
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

void insertIntoBuffer(int16_t* p_buf, uint16_t p_size, int16_t p_value, uint16_t p_index)
{
  uint16_t i = p_size+1;
  while (i > p_index) {
    p_buf[i] = p_buf[--i];
  }
  p_buf[i] = p_value;
}

void granulate()
{
  uint8_t grainWriteCounter = 0;
  uint16_t S = 441*(_grainTime/10); // Number of samples to read in block
  int16_t buf[B];

  uint16_t attackSamples = S * (_attackSetting / 100.0), decaySamples = S * (_decaySetting / 100.0);
  uint16_t relativeEnvelopeCounter, attackCounter = 0, decayCounter = 0, decayDifference;
  int16_t deltaS;
  
  uint16_t samplesRemaining = S, samplesToRead = B;
  _grainPosition = _wavFile.position();
  uint8_t segmentCounter = 1;
  uint8_t pauseLoopCounter = 0, pointChangeHandled = 0;

  float tsNPerSample, tsInterval, tsIntervalAverage;
  uint16_t tsExtraSampleCounter = 0, tsIntervalSum = 0, tsIterator;
  uint8_t tsIntervalN = 0, tsIntervalCounter = 0, tsIntervalValue, tsStretching = 0, tsPerSampleBase, tsPerSampleCount;
  int16_t tsInterpolatedValue, tsScalingFactor;

  Serial.println("P");
  while (_wavFile.available()) { //start of grain loop
    //reset counters
    samplesToRead = B;
    samplesRemaining = S;
    attackCounter = 0;
    decayCounter = decaySamples;
    segmentCounter = 1;
    
    while (samplesRemaining > 0) { //start of segment loop
      checkParams();
      if (_paramChangeBits != 0x00) { //if we have a change, re-calculate necessary parameters
        if (DID_GRAINTIME(_paramChangeBits)) {
          deltaS = S - 441*(_grainTime/10);
          S -= deltaS;
          samplesRemaining -= deltaS;
          attackSamples = S * (_attackSetting / 100.0);
          decaySamples = S * (_decaySetting / 100.0);
          GRAINTIME_HANDLE(_paramChangeBits);
        }
        if (DID_GRAINREPEAT(_paramChangeBits)) {
          //nothing to do?
          GRAINREPEAT_HANDLE(_paramChangeBits);
        }
        if (DID_ATTACKSETTING(_paramChangeBits)) {
          attackSamples = S * (_attackSetting / 100.0);
          ATTACKSETTING_HANDLE(_paramChangeBits);
        }
        if (DID_DECAYSETTING(_paramChangeBits)) {
          decaySamples = S * (_decaySetting / 100.0);
          DECAYSETTING_HANDLE(_paramChangeBits);
        }
        if (DID_PAUSEPOINT(_paramChangeBits) && !pointChangeHandled) {
          _pauseLoopPoint = _grainPosition;
          pauseLoopCounter = 0;
          pointChangeHandled = 1; //debounce
          PAUSEPOINT_HANDLE(_paramChangeBits);
        }
        if (DID_PAUSELENGTH(_paramChangeBits)) {
          //nothing to do?
          PAUSELENGTH_HANDLE(_paramChangeBits);
        }
        if (DID_TIMESTRETCH(_paramChangeBits)) {
          //?
          TIMESTRETCH_HANDLE(_paramChangeBits);
        }
        if (DID_FILENAME(_paramChangeBits)) {
          return;
        }
      }
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

      tsExtraSampleCounter = 0;
      if (_timestretchValue < 100) {
        tsNPerSample = 100.0/(float)_timestretchValue;
        samplesToRead = floor(((float)samplesToRead)/tsNPerSample);
        tsNPerSample -= 1.0;
        if (tsNPerSample >= 1.0) {
          tsPerSampleBase = floor(tsNPerSample);
          tsNPerSample -= (float)tsPerSampleBase;
        }
        else { tsPerSampleBase = 0; }
        tsInterval = 1.0 / tsNPerSample;
        tsIntervalValue = ceil(tsInterval);
        tsIntervalCounter = 0;
        tsIntervalSum = 0;
        tsStretching = 1;
        tsIntervalN = 0;
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
      
      if (tsStretching) {
        if (tsPerSampleBase != 0) {
          for (tsIterator = 0; tsIterator < (samplesToRead + tsExtraSampleCounter); tsIterator++) {
            tsPerSampleCount = tsPerSampleBase;
            if (++tsIntervalCounter == tsIntervalValue) {
              tsIntervalCounter = 0;
              tsPerSampleCount++;

              if (tsIntervalValue != tsInterval) {
                tsIntervalSum += tsIntervalValue;
                tsIntervalAverage = (float)tsIntervalSum / (float) ++tsIntervalN;
                if (tsIntervalAverage < tsInterval && tsIntervalValue < tsIntervalAverage) { tsIntervalValue++; }
                else if (tsIntervalAverage > tsInterval && tsIntervalValue > tsIntervalAverage) { tsIntervalValue--; }
              }
            }

            tsScalingFactor = ((float)buf[tsIterator+1] - (float)buf[tsIterator]) / tsPerSampleCount;
            tsInterpolatedValue = buf[tsIterator];
            while (tsPerSampleCount-- > 0) {
              tsInterpolatedValue += tsScalingFactor;
              insertIntoBuffer(buf, samplesToRead + tsExtraSampleCounter, tsInterpolatedValue, tsIterator);
              tsExtraSampleCounter++;
              tsIterator++;
            }
          }
        }
        else {
          for (tsIterator = 0; tsIterator < (samplesToRead + tsExtraSampleCounter); tsIterator += tsIntervalValue) {
            tsInterpolatedValue = floor(((float)(buf[tsIterator] + buf[tsIterator-1]))/2.0);
            insertIntoBuffer(buf, samplesToRead + tsExtraSampleCounter, tsInterpolatedValue, tsIterator);

            tsExtraSampleCounter++;

            if (tsIntervalValue != tsInterval) {
              tsIntervalSum += tsIntervalValue;
              tsIntervalAverage = (float)tsIntervalSum / (float) ++tsIntervalN;
              if (tsIntervalAverage < tsInterval && tsIntervalValue < tsIntervalAverage) { tsIntervalValue++; }
              else if (tsIntervalAverage > tsInterval && tsIntervalValue >= tsIntervalAverage) { tsIntervalValue--; }
            }
          }
        }
        tsStretching = 0;
      }
  
      Audio.prepare(buf, samplesToRead + tsExtraSampleCounter, _volume);
      Audio.write(buf, samplesToRead + tsExtraSampleCounter);
      
      segmentCounter++;
      //adjust samples remaining
      samplesRemaining -= samplesToRead;
      if (samplesRemaining < samplesToRead) { samplesToRead = samplesRemaining; }
      else { samplesToRead = B; } //reset from timestretch
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
      if (IS_PAUSED(_statusBits) && ++pauseLoopCounter >= _pauseLoopLength) {
        pointChangeHandled = 0; //if we made it to a loop reset the switch is probably done bouncing
        _wavFile.seek(_pauseLoopPoint);
        pauseLoopCounter = 0;
      }
      _grainPosition = _wavFile.position();
    }
  }
}

void loop()
{
  _wavFile = SD.open(NAME00);
  if (!_wavFile) {
    Serial.println("ER");
    while (true);
  }

  while(true) {
    Serial.println("here");
    if (IS_PLAYING(_statusBits)) {
      _wavFile.seek(0);
      granulate();
    }
    if (DID_FILENAME(_paramChangeBits)) {
      _wavFile.close();
      switch(_nameSelect) {
        case (0x00):
          _wavFile = SD.open(NAME00);
          break;
        case (0x01):
          _wavFile = SD.open(NAME01);
          break;
        case (0x02):
          _wavFile = SD.open(NAME02);
          break;
        case (0x03):
          _wavFile = SD.open(NAME03);
          break;
        case (0x04):
          _wavFile = SD.open(NAME04);
          break;
        case (0x05):
          _wavFile = SD.open(NAME05);
          break;
        case (0x06):
          _wavFile = SD.open(NAME06);
          break;
        case (0x07):
          _wavFile = SD.open(NAME07);
          break;
        case (0x08):
          _wavFile = SD.open(NAME08);
          break;
        case (0x09):
          _wavFile = SD.open(NAME09);
          break;
        case (0x0A):
          _wavFile = SD.open(NAME10);
          break;
        case (0x0B):
          _wavFile = SD.open(NAME11);
          break;
        case (0x0C):
          _wavFile = SD.open(NAME12);
          break;
        case (0x0D):
          _wavFile = SD.open(NAME13);
          break;
        case (0x0E):
          _wavFile = SD.open(NAME14);
          break;
        case (0x0F):
          _wavFile = SD.open(NAME15);
          break;
        default:
          _wavFile = SD.open(NAME00);
          break;
      }
      FILENAME_HANDLE(_paramChangeBits);
    }
    //while(!IS_PLAYING(_statusBits)) {};
  }
  
  _wavFile.close();
}

void checkButtonPlay()
{
  digitalRead(_buttonPlay) ? PLAYING_ON(_statusBits) : PLAYING_OFF(_statusBits);
  if (IS_PLAYING(_statusBits)) { Serial.println("checked"); }
  else { Serial.println("nope"); }
}

void checkButtonSeek()
{
  digitalRead(_buttonSeek) ? SEEK_ON(_statusBits) : SEEK_OFF(_statusBits);
}

void checkButtonReverse()
{
  digitalRead(_buttonReverse) ? REVERSE_ON(_statusBits) : REVERSE_OFF(_statusBits);
}

void checkButtonPause()
{
  digitalRead(_buttonPause) ? PAUSED_ON(_statusBits) : PAUSED_OFF(_statusBits);
  if (IS_PAUSED(_statusBits)) { PAUSEPOINT_SIGNAL(_paramChangeBits); } //for now
}

void checkButtonFilename()
{
  digitalRead(_buttonFilename0) ? _nameSelect |= 0x01 : _nameSelect &= ~0x01;
  digitalRead(_buttonFilename1) ? _nameSelect |= 0x02 : _nameSelect &= ~0x02;
  digitalRead(_buttonFilename2) ? _nameSelect |= 0x04 : _nameSelect &= ~0x04;
  digitalRead(_buttonFilename3) ? _nameSelect |= 0x08 : _nameSelect &= ~0x08;
  FILENAME_SIGNAL(_paramChangeBits);
}

void resetParams()
{
  _timestretchValue = 100;
  _potTimestretchPrevVal = 1018;
}

void checkParams()
{
  uint16_t val;
  int8_t diff;

  val = analogRead(_potGrainTime);
  diff = _potGrainTimePrevVal - val;
  if (diff > 13 || diff < -13) {
    //20ms - 2000ms?  In increments of 20ms
    _potGrainTimePrevVal = val;
    val /= 10;
    _grainTime = (val > 0) ? val*20 : 20;
    GRAINTIME_SIGNAL(_paramChangeBits);
    Serial.println(_grainTime);
  }

  val = analogRead(_potGrainRepeat);
  diff = _potGrainRepeatPrevVal - val;
  if (diff > 13 || diff < -13) {
    //1-10
    _potGrainRepeatPrevVal = val;
    val /= 100;
    _grainRepeat = (val > 0) ? val : 1;
    GRAINREPEAT_SIGNAL(_paramChangeBits);
  }

  val = analogRead(_potAttackSetting);
  diff = _potAttackSettingPrevVal - val;
  if (diff > 13 || diff < -13) {
    //1-100
    _potAttackSettingPrevVal = val;
    val /= 10;
    _attackSetting = (val <= 100) ? val : 100;
    ATTACKSETTING_SIGNAL(_paramChangeBits);
  }

  val = analogRead(_potDecaySetting);
  diff = _potDecaySettingPrevVal - val;
  if (diff > 13 || diff < -13) {
    //1-100
    _potDecaySettingPrevVal = val;
    val /= 10;
    _decaySetting = (val <= 100) ? val : 100;
    if ((_attackSetting + _decaySetting) > 100) { //attack eats decay
      _decaySetting = 100 - _attackSetting;
    }
    DECAYSETTING_SIGNAL(_paramChangeBits);
  }

  val = analogRead(_potPauseLength);
  diff = _potPauseLengthPrevVal - val;
  if (diff > 13 || diff < -13) {
    //1-10
    _potPauseLengthPrevVal = val;
    val /= 100;
    _pauseLoopLength = (val > 0) ? val : 1;
    PAUSELENGTH_SIGNAL(_paramChangeBits);
  }

  val = analogRead(_potPausePoint);
  diff = _potPausePointPrevVal - val;
  if (diff > 13 || diff < -13) {
    //what do you plan to do here
  }

  val = analogRead(_potTimestretch);
  diff = _potTimestretchPrevVal - val;
  if (diff > 13 || diff < -13) {
    _potTimestretchPrevVal = val;
    val /= 10;
    _timestretchValue = (val > 100) ? 100 : val;
    if (!_timestretchValue) { _timestretchValue = 1; }
    TIMESTRETCH_SIGNAL(_paramChangeBits);
    //1-100
  }

  val = analogRead(_potVolume);
  diff = _potVolumePrevVal - val;
  if (diff > 13 || diff < -13) {
    _volume = val;
    _potVolumePrevVal = val;
  }
}
