#include "ChipPinMapping.h"
#include <SPI.h>
#include <SdFat.h>
#include <Wire.h>
#include <U8g2lib.h>

//OLED
U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, 19, 18, U8X8_PIN_NONE);

//File Stream
SdFatSdio SD;
File vgm;
const unsigned int MAX_CMD_BUFFER = 32;
char cmdBuffer[MAX_CMD_BUFFER];
uint32_t bufferPos = 0;
const unsigned int MAX_FILE_NAME_SIZE = 1024;
char fileName[MAX_FILE_NAME_SIZE];
uint16_t numberOfFiles = 0;
int32_t currentFileNumber = 0;

//Timing Variables
float singleSampleWait = 0;
const int sampleRate = 44100; //44100 standard
const float WAIT60TH = ((1000.0 / (sampleRate/(float)735))*1000);
const float WAIT50TH = ((1000.0 / (sampleRate/(float)882))*1000);
uint32_t waitSamples = 0;
unsigned long preCalced8nDelays[16];
unsigned long preCalced7nDelays[16];
unsigned long lastWaitData61 = 0;
unsigned long cachedWaitTime61 = 0;
unsigned long pauseTime = 0;
unsigned long startTime = 0;

//Song Data Variables
#define MAX_PCM_BUFFER_SIZE 102400 //In bytes
uint8_t pcmBuffer[MAX_PCM_BUFFER_SIZE];
uint32_t pcmBufferPosition = 0;
uint8_t cmd;
uint32_t loopOffset = 0;
uint16_t loopCount = 0;
uint16_t nextSongAfterXLoops = 3;
enum PlayMode {LOOP, PAUSE, SHUFFLE, IN_ORDER};
PlayMode playMode = SHUFFLE;

//GD3 Data
String trackTitle;
String gameName;
String systemName;
String gameDate;

void SetClock(uint16_t oct, uint16_t dac, unsigned char target)
{
  SPI.begin();
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
  unsigned char CNF = 0b00000000;
  uint16_t BitMap = (oct << 12) | (dac << 2) | CNF;
  byte Byte1=(byte)(BitMap >> 8 );
  byte Byte2=(byte)BitMap;
  digitalWrite(target, LOW);
  SPI.transfer(Byte1);
  SPI.transfer(Byte2);
  digitalWrite(target, HIGH);
  SPI.endTransaction();
}

void FillBuffer()
{
    vgm.readBytes(cmdBuffer, MAX_CMD_BUFFER);
}

byte GetByte()
{
  if(bufferPos == MAX_CMD_BUFFER)
  {
    bufferPos = 0;
    FillBuffer();
  }
  return cmdBuffer[bufferPos++];
}

void SendSNByte(byte b)
{
  digitalWrite(SN_WE, HIGH);
  for(int i=0; i<8; i++)
  {
    digitalWrite(SN_DATA[i], ((b >> i)&1));
  }
  digitalWrite(SN_WE, LOW);
  delayMicroseconds(13);
  digitalWrite(SN_WE, HIGH);
}

void SendYMByte(byte b)
{
    for(int i=0; i<8; i++)
    {
      digitalWrite(YM_DATA[i], ((b >> i)&1));
    }
}

void ClearBuffers()
{
  pcmBufferPosition = 0;
  bufferPos = 0;
  for(int i = 0; i < MAX_CMD_BUFFER; i++)
    cmdBuffer[i] = 0;
  for(int i = 0; i < MAX_PCM_BUFFER_SIZE; i++)
    pcmBuffer[i] = 0;
}

void ClearTrackData()
{
  for(int i = 0; i < MAX_FILE_NAME_SIZE; i++)
    fileName[i] = 0;
  trackTitle = "";
  gameName = "";
  systemName = "";
  gameDate = "";
}

void SilenceAllChannels()
{
  SendSNByte(0x9f);
  SendSNByte(0xbf);
  SendSNByte(0xdf);
  SendSNByte(0xff);

  digitalWrite(YM_A0, LOW);
  digitalWrite(YM_A1, LOW);
  digitalWrite(YM_CS, HIGH);
  digitalWrite(YM_WR, HIGH);
  digitalWrite(YM_RD, HIGH);
  digitalWrite(YM_IC, HIGH);
  delay(1);
  digitalWrite(YM_IC, LOW);
  delay(1);
  digitalWrite(YM_IC, HIGH);
}

uint32_t Read32() //Read 32 bit value from buffer
{
  byte v0 = GetByte();
  byte v1 = GetByte();
  byte v2 = GetByte();
  byte v3 = GetByte();
  return uint32_t(v0 + (v1 << 8) + (v2 << 16) + (v3 << 24));
}

uint32_t ReadRaw32() //Read 32 bit value straight from SD card
{
  byte v0 = vgm.read();
  byte v1 = vgm.read();
  byte v2 = vgm.read();
  byte v3 = vgm.read();
  return uint32_t(v0 + (v1 << 8) + (v2 << 16) + (v3 << 24));
}

uint32_t EoFOffset = 0;
uint32_t VGMVersion = 0;
uint32_t GD3Offset = 0;
void GetHeaderData() //Scrape off the important VGM data from the header, then drop down to the GD3 area for song info data
{
  Read32(); //V - G - M 0x00->0x03
  EoFOffset = Read32(); //End of File offset 0x04->0x07
  VGMVersion = Read32(); //VGM Version 0x08->0x0B
  for(int i = 0x0C; i<0x14; i++)GetByte(); //Skip 0x0C->0x14
  GD3Offset = Read32(); //GD3 (song info) data offset 0x14->0x17

  uint32_t bufferReturnPosition = vgm.position();
  vgm.seek(0);
  vgm.seekCur(GD3Offset+0x14);
  uint32_t GD3Position = 0x00;
  ReadRaw32(); GD3Position+=4;  //G - D - 3
  ReadRaw32(); GD3Position+=4;  //Version data
  uint32_t dataLength = ReadRaw32(); //Get size of data payload
  GD3Position+=4;

  String rawGD3String;
  // Serial.print("DATA LENGTH: ");
  // Serial.println(dataLength);

  for(int i = 0; i<dataLength; i++) //Convert 16-bit characters to 8 bit chars. This may cause issues with non ASCII characters. (IE Japanese chars.)
  {
    char c1 = vgm.read();
    char c2 = vgm.read();
    if(c1 == 0 && c2 == 0)
      rawGD3String += '\n';
    else
      rawGD3String += char(c1);
  }
  GD3Position = 0;

  while(rawGD3String[GD3Position] != '\n') //Parse out the track title.
  {
    trackTitle += rawGD3String[GD3Position];
    GD3Position++;
  }
  GD3Position++;

  while(rawGD3String[GD3Position] != '\n') GD3Position++; //Skip Japanese track title.
  GD3Position++;
  while(rawGD3String[GD3Position] != '\n') //Parse out the game name.
  {
    gameName += rawGD3String[GD3Position];
    GD3Position++;
  }
  GD3Position++;
  while(rawGD3String[GD3Position] != '\n') GD3Position++;//Skip Japanese game name.
  GD3Position++;
  while(rawGD3String[GD3Position] != '\n') //Parse out the system name.
  {
    systemName += rawGD3String[GD3Position];
    GD3Position++;
  }
  GD3Position++;
  while(rawGD3String[GD3Position] != '\n') GD3Position++;//Skip Japanese system name.
  GD3Position++;
  while(rawGD3String[GD3Position] != '\n') GD3Position++;//Skip English authors
  GD3Position++;
  // while(rawGD3String[GD3Position] != 0) //Parse out the music authors (I skipped this since it sometimes produces a ton of data! Uncomment this, comment skip, add vars if you want this.)
  // {
  //   musicAuthors += rawGD3String[GD3Position];
  //   GD3Position++;
  // }
  while(rawGD3String[GD3Position] != '\n') GD3Position++;//Skip Japanese authors.
  GD3Position++;
  while(rawGD3String[GD3Position] != '\n') //Parse out the game date
  {
    gameDate += rawGD3String[GD3Position];
    GD3Position++;
  }
  GD3Position++;
  Serial.println(trackTitle);
  Serial.println(gameName);
  Serial.println(systemName);
  Serial.println(gameDate);
  Serial.println("");
  vgm.seek(bufferReturnPosition); //Send the file seek back to the original buffer position so we don't confuse our program.
  waitSamples = Read32(); //0x18->0x1B : Get wait Samples count
  loopOffset = Read32();  //0x1C->0x1F : Get loop offset Postition
  for(int i = 0; i<5; i++) Read32(); //Skip right to the VGM data offset position;
  uint32_t vgmDataOffset = Read32();
  if(vgmDataOffset == 0 || vgmDataOffset == 12) //VGM starts at standard 0x40
  {
    Read32(); Read32();
  }
  else
  {
    for(int i = 0; i < vgmDataOffset; i++) GetByte();  //VGM starts at different data position (Probably VGM spec 1.7+)
  }
}

void RemoveSVI() //Sometimes, Windows likes to place invisible files in our SD card without asking... GTFO!
{
  File nextFile;
  nextFile.openNext(SD.vwd(), O_READ);
  char name[MAX_FILE_NAME_SIZE];
  nextFile.getName(name, MAX_FILE_NAME_SIZE);
  String n = String(name);
  if(n == "System Volume Information")
  {
      if(!nextFile.rmRfStar())
        Serial.println("Failed to remove SVI file");
  }
  SD.vwd()->rewind();
  nextFile.close();
}

void DrawOledPage()
{
  u8g2.clearDisplay();
  u8g2.setFont(u8g2_font_helvR08_te);
  u8g2.sendBuffer();
  char *cstr = &trackTitle[0u];
  u8g2.drawStr(0,10, cstr);
  cstr = &gameName[0u];
  u8g2.drawStr(0,20, cstr);
  cstr = &gameDate[0u];
  u8g2.drawStr(0,30, cstr);
  cstr = &systemName[0u];
  u8g2.drawStr(0,40, cstr);
  String fileNumberData = "File: " + String(currentFileNumber+1) + "/" + String(numberOfFiles);
  cstr = &fileNumberData[0u];
  u8g2.drawStr(0,50, cstr);
  String loopShuffleStatus;
  String loopStatus;
  if(playMode == LOOP)
    loopStatus = "LOOP ON";
  else
    loopStatus = "LOOP OFF";
  String shuffleStatus;
  if(playMode == SHUFFLE)
    shuffleStatus = "SHFL ON";
  else
    shuffleStatus = "SHFL OFF";
  loopShuffleStatus = loopStatus + " | " + shuffleStatus;
  cstr = &loopShuffleStatus[0u];
  u8g2.drawStr(0, 60, cstr);
  u8g2.sendBuffer();
}

enum StartUpProfile {FIRST_START, NEXT, PREVIOUS, RNG, REQUEST};
void StartupSequence(StartUpProfile sup, String request = "")
{
  File nextFile;
  ClearTrackData();
  switch(sup)
  {
    case FIRST_START:
    {
      nextFile.openNext(SD.vwd(), O_READ);
      nextFile.getName(fileName, MAX_FILE_NAME_SIZE);
      nextFile.close();
      currentFileNumber = 0;
    }
    break;
    case NEXT:
    {
      if(currentFileNumber+1 >= numberOfFiles)
      {
          SD.vwd()->rewind();
          currentFileNumber = 0;
      }
      else
          currentFileNumber++;
      nextFile.openNext(SD.vwd(), O_READ);
      nextFile.getName(fileName, MAX_FILE_NAME_SIZE);
      nextFile.close();
    }
    break;
    case PREVIOUS:
    {
      if(currentFileNumber != 0)
      {
        currentFileNumber--;
        SD.vwd()->rewind();
        for(int i = 0; i<=currentFileNumber; i++)
        {
          nextFile.close();
          nextFile.openNext(SD.vwd(), O_READ);
        }
        nextFile.getName(fileName, MAX_FILE_NAME_SIZE);
        nextFile.close();
      }
      else
      {
        currentFileNumber = numberOfFiles-1;
        SD.vwd()->rewind();
        for(int i = 0; i<=currentFileNumber; i++)
        {
          nextFile.close();
          nextFile.openNext(SD.vwd(), O_READ);
        }
        nextFile.getName(fileName, MAX_FILE_NAME_SIZE);
        nextFile.close();
      }
    }
    break;
    case RNG:
    {
      randomSeed(micros());
      uint16_t randomFile = currentFileNumber;
      while(randomFile == currentFileNumber)
        randomFile = random(numberOfFiles-1);
      currentFileNumber = randomFile;
      SD.vwd()->rewind();
      nextFile.openNext(SD.vwd(), O_READ);
      {
        for(int i = 0; i<randomFile; i++)
        {
          nextFile.close();
          nextFile.openNext(SD.vwd(), O_READ);
        }
      }
      nextFile.getName(fileName, MAX_FILE_NAME_SIZE);
      nextFile.close();
    }
    break;
    case REQUEST:
    {
      SD.vwd()->rewind();
      bool fileFound = false;
      Serial.print("REQUEST: ");Serial.println(request);
      for(int i = 0; i<numberOfFiles; i++)
      {
        nextFile.close();
        nextFile.openNext(SD.vwd(), O_READ);
        nextFile.getName(fileName, MAX_FILE_NAME_SIZE);
        String tmpFN = String(fileName);
        tmpFN.trim();
        if(tmpFN == request.trim())
        {
          currentFileNumber = i;
          fileFound = true;
          break;
        }
      }
      nextFile.close();
      if(fileFound)
      {
        Serial.println("File found!");
      }
      else
      {
        Serial.println("ERROR: File not found! Continuing with current song.");
        return;
      }
    }
    break;
  }
  waitSamples = 0;
  loopOffset = 0;
  lastWaitData61 = 0;
  cachedWaitTime61 = 0;
  pauseTime = 0;
  startTime = 0;
  loopCount = 0;
  cmd = 0;
  ClearBuffers();
  Serial.print("Current file number: "); Serial.print(currentFileNumber+1); Serial.print("/"); Serial.println(numberOfFiles);
  if(vgm.isOpen())
    vgm.close();
  vgm = SD.open(fileName, FILE_READ);
  if(!vgm)
    Serial.println("File open failed!");
  else
    Serial.println("Opened successfully...");
  FillBuffer();
  GetHeaderData();
  singleSampleWait = ((1000.0 / (sampleRate/(float)1))*1000);

    for(int i = 0; i<16; i++)
    {
      if(i == 0)
      {
        preCalced8nDelays[i] = 0;
        preCalced7nDelays[i] = 1;
      }
      else
      {
        preCalced8nDelays[i] = ((1000.0 / (sampleRate/(float)i))*1000);
        preCalced7nDelays[i] = ((1000.0 / (sampleRate/(float)i+1))*1000);
      }
    }

    SilenceAllChannels();
    digitalWrite(SN_WE, HIGH);
    DrawOledPage();
    delay(500);
}

void setup()
{
  //Set up SN_Clock
  pinMode(SN_CLOCK_CS, OUTPUT);
  digitalWrite(SN_CLOCK_CS, HIGH);
  SetClock(11, 831, SN_CLOCK_CS); //3.58 MHz
  // pinMode(YM_CLOCK_CS, OUTPUT);
  // digitalWrite(YM_CLOCK_CS, HIGH);
  //SetClock(12, 912, YM_CLOCK_CS); //7.67 MHz

  //Setup Data pins
  for(int i = 0; i<8; i++)
  {
    pinMode(YM_DATA[i], OUTPUT);
    pinMode(SN_DATA[i], OUTPUT);
  }

  //Button Setup
  pinMode(FWD_BTN, INPUT_PULLUP);
  pinMode(RNG_BTN, INPUT_PULLUP);
  pinMode(PRV_BTN, INPUT_PULLUP);

  //Sound chip control pins
  pinMode(SN_WE, OUTPUT);
  pinMode(YM_IC, OUTPUT);
  pinMode(YM_CS, OUTPUT);
  pinMode(YM_WR, OUTPUT);
  pinMode(YM_RD, OUTPUT);
  pinMode(YM_A0, OUTPUT);
  pinMode(YM_A1, OUTPUT);
  Serial.begin(115200);
  SilenceAllChannels();
  if(!SD.begin())
  {
      Serial.println("Card Mount Failed");
      return;
  }
  RemoveSVI();
  File countFile;
  while ( countFile.openNext( SD.vwd(), O_READ ))
  {
    countFile.close();
    numberOfFiles++;
  }
  countFile.close();
  SD.vwd()->rewind();
  u8g2.begin();
  u8g2.firstPage();
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(30,10,"Aidan Lawrence");
  u8g2.drawStr(50,20,"2017");
  u8g2.drawStr(30,50,"Sega Genesis");
  u8g2.drawStr(10,60,"Hardware VGM Player");
  u8g2.sendBuffer();
  delay(1500);
  u8g2.clearDisplay();
  u8g2.sendBuffer();
  StartupSequence(FIRST_START);
}

void loop()
{
  while(Serial.available())
  {
    switch(Serial.read())
    {
      case '+': //Next song
        StartupSequence(NEXT);
      break;
      case '-': //Previous Song
        StartupSequence(PREVIOUS);
      break;
      case '*': //Pick random song
        StartupSequence(RNG);
      break;
      case '/': //Toggle shuffle mode
        playMode == SHUFFLE ? playMode = IN_ORDER : playMode = SHUFFLE;
        playMode == SHUFFLE ? Serial.println("SHUFFLE ON") : Serial.println("SHUFFLE OFF");
        DrawOledPage();
      break;
      case '.': //Toggle loop mode
        playMode == LOOP ? playMode = IN_ORDER : playMode = LOOP;
        playMode == LOOP ? Serial.println("LOOP ON") : Serial.println("LOOP OFF");
        DrawOledPage();
      break;
      case 'r': //Song Request, format:  r:mySongFileName.vgm - An attempt will be made to find and open that file.
        String req = Serial.readString(1024);
        req.remove(0, 1); //Remove colon character
        StartupSequence(REQUEST, req);
      break;
    }
  }

  if(!digitalRead(FWD_BTN))
    StartupSequence(NEXT);
  if(!digitalRead(PRV_BTN))
    StartupSequence(PREVIOUS);
  if(!digitalRead(RNG_BTN))
    StartupSequence(RNG);


  if(loopCount >= nextSongAfterXLoops)
  {
    if(playMode == SHUFFLE)
      StartupSequence(RNG);
    if(playMode == IN_ORDER)
      StartupSequence(NEXT);
  }
  if(playMode == PAUSE)
    return;
  unsigned long timeInMicros = micros();
  if( timeInMicros - startTime <= pauseTime)
  {
    return;
  }
  cmd = GetByte();
  switch(cmd) //Use this switch statement to parse VGM commands
    {
      case 0x50:
      SendSNByte(GetByte());
      startTime = timeInMicros;
      pauseTime = singleSampleWait;
      break;

      case 0x52:
      {
      byte address = GetByte();
      byte data = GetByte();
      digitalWrite(YM_A1, LOW);
      digitalWrite(YM_A0, LOW);
      digitalWrite(YM_CS, LOW);
      //Areas like this may require 1 microsecond delays.
      SendYMByte(address);
      digitalWrite(YM_WR, LOW);
      //delayMicroseconds(1);
      digitalWrite(YM_WR, HIGH);
      digitalWrite(YM_CS, HIGH);
      //delayMicroseconds(1);
      digitalWrite(YM_A0, HIGH);
      digitalWrite(YM_CS, LOW);
      SendYMByte(data);
      digitalWrite(YM_WR, LOW);
      //delayMicroseconds(1);
      digitalWrite(YM_WR, HIGH);
      digitalWrite(YM_CS, HIGH);
      }
      startTime = timeInMicros;
      pauseTime = singleSampleWait;
      //delay(singleSampleWait);
      break;

      case 0x53:
      {
      byte address = GetByte();
      byte data = GetByte();
      digitalWrite(YM_A1, HIGH);
      digitalWrite(YM_A0, LOW);
      digitalWrite(YM_CS, LOW);
      SendYMByte(address);
      digitalWrite(YM_WR, LOW);
      //delayMicroseconds(1);
      digitalWrite(YM_WR, HIGH);
      digitalWrite(YM_CS, HIGH);
      //delayMicroseconds(1);
      digitalWrite(YM_A0, HIGH);
      digitalWrite(YM_CS, LOW);
      SendYMByte(data);
      digitalWrite(YM_WR, LOW);
      //delayMicroseconds(1);
      digitalWrite(YM_WR, HIGH);
      digitalWrite(YM_CS, HIGH);
      }
      startTime = timeInMicros;
      pauseTime = singleSampleWait;
      //delay(singleSampleWait);
      break;


      case 0x61:
      {
        //Serial.print("0x61 WAIT: at location: ");
        //Serial.print(parseLocation);
        //Serial.print("  -- WAIT TIME: ");
      uint32_t wait = 0;
      for ( int i = 0; i < 2; i++ )
      {
        wait += ( uint32_t( GetByte() ) << ( 8 * i ));
      }

      if(floor(lastWaitData61) != wait) //Avoid doing lots of unnecessary division.
      {
        lastWaitData61 = wait;
        if(wait == 0)
          break;
        cachedWaitTime61 = ((1000.0 / (sampleRate/(float)wait))*1000);
      }
      //Serial.println(cachedWaitTime61);

      startTime = timeInMicros;
      pauseTime = cachedWaitTime61;
      //delay(cachedWaitTime61);
      break;
      }
      case 0x62:
      startTime = timeInMicros;
      pauseTime = WAIT60TH;
      //delay(WAIT60TH); //Actual time is 16.67ms (1/60 of a second)
      break;
      case 0x63:
      startTime = timeInMicros;
      pauseTime = WAIT50TH;
      //delay(WAIT50TH); //Actual time is 20ms (1/50 of a second)
      break;
      case 0x67:
      {
        //Serial.print("DATA BLOCK 0x67.  PCM Data Size: ");
        GetByte();
        GetByte(); //Skip 0x66 and data type
        pcmBufferPosition = bufferPos;
        uint32_t PCMdataSize = 0;
        for ( int i = 0; i < 4; i++ )
        {
          PCMdataSize += ( uint32_t( GetByte() ) << ( 8 * i ));
        }
        //Serial.println(PCMdataSize);

        for ( int i = 0; i < PCMdataSize; i++ )
        {
           if(PCMdataSize <= MAX_PCM_BUFFER_SIZE)
              pcmBuffer[ i ] = (uint8_t)GetByte();
        }
        //Serial.println("Finished buffering PCM");
        break;
      }

      case 0x70:
      case 0x71:
      case 0x72:
      case 0x73:
      case 0x74:
      case 0x75:
      case 0x76:
      case 0x77:
      case 0x78:
      case 0x79:
      case 0x7A:
      case 0x7B:
      case 0x7C:
      case 0x7D:
      case 0x7E:
      case 0x7F:
      {
        //Serial.println("0x7n WAIT");
        uint32_t wait = cmd & 0x0F;
        //Serial.print("Wait value: ");
        //Serial.println(wait);
        startTime = timeInMicros;
        pauseTime = preCalced7nDelays[wait];
        //delay(preCalced7nDelays[wait]);
      break;
      }
      case 0x80:
      case 0x81:
      case 0x82:
      case 0x83:
      case 0x84:
      case 0x85:
      case 0x86:
      case 0x87:
      case 0x88:
      case 0x89:
      case 0x8A:
      case 0x8B:
      case 0x8C:
      case 0x8D:
      case 0x8E:
      case 0x8F:
        {
        uint32_t wait = cmd & 0x0F;
        byte address = 0x2A;
        byte data = pcmBuffer[pcmBufferPosition++];
        digitalWrite(YM_A1, LOW);
        digitalWrite(YM_A0, LOW);
        digitalWrite(YM_CS, LOW);
        //delayMicroseconds(1);
        SendYMByte(address);
        digitalWrite(YM_WR, LOW);
        //delayMicroseconds(1);
        digitalWrite(YM_WR, HIGH);
        digitalWrite(YM_CS, HIGH);
        //delayMicroseconds(1);
        digitalWrite(YM_A0, HIGH);
        digitalWrite(YM_CS, LOW);
        SendYMByte(data);
        digitalWrite(YM_WR, LOW);
        //delayMicroseconds(1);
        digitalWrite(YM_WR, HIGH);
        digitalWrite(YM_CS, HIGH);
        startTime = timeInMicros;
        pauseTime = preCalced8nDelays[wait];
        }
        break;
      case 0xE0:
      {
        //Serial.print("LOCATION: ");
        //Serial.print(parseLocation, HEX);
        //Serial.print(" - PCM SEEK 0xE0. NEW POSITION: ");

        pcmBufferPosition = 0;
        for ( int i = 0; i < 4; i++ )
        {
          pcmBufferPosition += ( uint32_t( GetByte() ) << ( 8 * i ));
        }
      }
        //Serial.println(pcmBufferPosition);
      break;
      case 0x66:
      if(loopOffset == 0)
        loopOffset = 64;
      loopCount++;
      vgm.seek(loopOffset);
      FillBuffer();
      bufferPos = 0;
      break;
      default:
      break;
    }
}
