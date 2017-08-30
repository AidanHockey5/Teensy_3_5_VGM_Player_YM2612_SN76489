# Teensy 3.5 VGM Player Featuring YM2612 & SN76489
A hardware Sega Genesis / Master system video game music player based on the Teensy 3.5

This is the offical successor to my original ESP8266 VGM player https://github.com/AidanHockey5/ESP8266_VGM_Player_PSG_YM2612

This project was built using PlatformIO for Atom.

# Required Libraries
This project requires [SdFat](https://github.com/greiman/SdFat) (included) and [U8G2](https://github.com/olikraus/u8g2) (not included)

# Information about the main sound chips and VGM

The YM2612, AKA OPN2 (FM Operator, type N), is a frequency modulation synthesizer that was found in the Sega Genesis. With the exception of a few scraps of information online, this chip has mainly been lost to time. There is no official data sheet and Yamaha semiconductor no longer holds records of them. It was quite a challenge to get anything out of this chip, much less a full VGM file player.

The SN76489, AKA PSG (Programmable Sound Generator), is a simple 3-square wave channel, 1-noise channel sound chip that was a staple in early home computers and video game consoles. It was most notably found in the Sega Master System - the predecessor of the Sega Genesis/Megadrive.

VGM stands for Video Game Music, and it is a 44.1KHz logging format that stores real soundchip register information. My player will parse these files and send the data to the appropriate chips. You can learn more about the VGM file format here: http://www.smspower.org/Music/VGMFileFormat

http://www.smspower.org/uploads/Music/vgmspec170.txt?sid=58da937e68300c059412b536d4db2ca0

# Hook-up Guide

This set of hook-up tables is based on the pin numbers listed in ChipPinMapping.h. You may change any of these pins to suit your build inside of ChipPinMapping.h, but it is not reccomened to change any SPI or I2C data bus pins.

Teensy 3.5 | YM2612
------------ | -------------
0-7 | D0-D7
38  | IC
37 | CS
36 | WR
35 | RD
34 | A0
33 | A1

Teensy 3.5 | SN76489 (PSG)
------------ | -------------
24-31 | D0-D7
39 | SN_WE

# Clocking the Sound Chips
You may attach a 7.67-7.68 MHz full-can crystal (or other signal source) to øM on the YM2612 and a 3.57-3.58 MHz full-can crystal (or other signal source) to the CLOCK pin on the SN67489. If you can not find crystals at this frequency, you can use LTC6903 SPI programmable oscillators. As my project stands, I'm only using an LTC6903 for the SN76489 and a 7.68 MHz crystal for the YM2612, but I've also included commented-out code for an LTC6903 applied to the YM2612.

Teensy 3.5 | LTC6903 (clocking SN76489 @ ~3.57 MHz)
------------ | -------------
11 | SDI
13 | SCK
32 | SEN/ADR

LTC6903 CLK_OUT -> SN76489 CLOCK PIN

If you'd like to use an LTC6903 for the YM2612, you must comment-out ```#define YM_CLOCK_CS 14``` in ChipPinMapping.h. You must also comment-out:

```
// pinMode(YM_CLOCK_CS, OUTPUT);
// digitalWrite(YM_CLOCK_CS, HIGH);
// SetClock(12, 912, YM_CLOCK_CS); //7.67 MHz
```

inside of main.cpp setup function.

Teensy 3.5 | LTC6903 (clocking YM2612 @ 7.67 MHz)
------------ | -------------
11 | SDI
13 | SCK
14 | SEN/ADR

# SD Card Information
The Teensy 3.5 has a built-in, high speed micro-SD card reader. You must format your SD card to Fat32 in order for this device to work correctly. Your SD card must only contain uncompressed .vgm files. VGZ FILES WILL NOT WORK! You may download .vgz files and use [7zip](http://www.7-zip.org/download.html) to extract the uncompressed file out of them. Vgm files on the SD card do not need to have the .vgm extention. As long as they contain valid, uncompressed vgm data, they will be read by the program regardless of their name.
You can find VGM files by Googling "myGameName VGM," or by checking out sites like http://project2612.org/

# Control Over Serial
You can use a serial connection to control playback features. The commands are as follows:

Command | Result
------------ | -------------
\+ | Next Track
\- | Previous Track
\* | Random Track
\/ | Toggle Shuffle Mode
\. | Toggle Song Looping
r: | Request song

A song request is formatted as follows: ```r:mySongFile.vgm```
Once a song request is sent through the serial console, an attempt will be made to open that song file. The file must exist on the Teensy's SD card, and spelling/capitalization must be correct.
