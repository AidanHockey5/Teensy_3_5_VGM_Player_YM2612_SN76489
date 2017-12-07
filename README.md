# Teensy 3.5 VGM Player Featuring YM2612 & SN76489
A hardware Sega Genesis / Master system video game music player based on the Teensy 3.5

You can view this project in action here: https://youtu.be/oJf--M76etQ

For more information, visit: http://www.aidanlawrence.com/hardware-sega-genesis-vgm-player-v-3/

This is the official successor to my original ESP8266 VGM player https://github.com/AidanHockey5/ESP8266_VGM_Player_PSG_YM2612

This project was built using PlatformIO with Visual Studio Code.

# Required Libraries
This project requires [SdFat](https://github.com/greiman/SdFat) (included) and [U8G2](https://github.com/olikraus/u8g2) (not included)

# Information about the main sound chips and VGM

The YM2612, AKA OPN2 (FM Operator, type N), is a frequency modulation synthesizer that was found in the Sega Genesis. With the exception of a few scraps of information online, this chip has mainly been lost to time. There is no official data sheet and Yamaha semiconductor no longer holds records of them. It was quite a challenge to get anything out of this chip, much less a full VGM file player.

The SN76489, AKA PSG (Programmable Sound Generator), is a simple 3-square wave channel, 1-noise channel sound chip that was a staple in early home computers and video game consoles. It was most notably found in the Sega Master System - the predecessor of the Sega Genesis/Megadrive.

VGM stands for Video Game Music, and it is a 44.1KHz logging format that stores real soundchip register information. My player will parse these files and send the data to the appropriate chips. You can learn more about the VGM file format here: http://www.smspower.org/Music/VGMFileFormat

http://www.smspower.org/uploads/Music/vgmspec170.txt?sid=58da937e68300c059412b536d4db2ca0

# Hook-up Guide

This set of hook-up tables is based on the pin numbers listed in ChipPinMapping.h. You may change any of these pins to suit your build inside of ChipPinMapping.h, but it is not recommended to change any SPI or I2C data bus pins.

Teensy 3.5 | YM2612
------------ | -------------
0-7 | D0-D7
38  | IC
39 | CS (previously 37)
36 | WR
35 | RD
34 | A0
33 | A1

Teensy 3.5 | SN76489 (PSG)
------------ | -------------
24-31 | D0-D7
37 | SN_WE (previously 39)

# Clocking the Sound Chips
You may attach a 7.67-7.68 MHz full-can crystal (or other signal source) to øM on the YM2612 and a 3.57-3.58 MHz full-can crystal (or other signal source) to the CLOCK pin on the SN67489. If you can not find crystals at this frequency, you can use LTC6903 SPI programmable oscillators. As my project stands, I'm only using an LTC6903 for the SN76489 and a 7.68 MHz crystal for the YM2612, but I've also included commented-out code for an LTC6903 applied to the YM2612.

Teensy 3.5 | LTC6903 (clocking SN76489 @ ~3.57 MHz)
------------ | -------------
11 | SDI
13 | SCK
32 | SEN/ADR

LTC6903 CLK_OUT -> SN76489 CLOCK PIN

Teensy 3.5 | LTC6903 (clocking YM2612 @ 7.67 MHz)
------------ | -------------
11 | SDI
13 | SCK
14 | SEN/ADR

# OLED Display
This project utilizes a 128x64 monochromatic I2C OLED display to show track information parsed from the GD3 data within the VGM file. Be careful, as this is a 3.3 volt part and must not be powered by 5 volts. The connection pins are:

Teensy 3.5 | 128x64 I2C OLED
------------ | -------------
18 | SDA
19 | SCL

# SD Card Information
The Teensy 3.5 has a built-in, high speed micro-SD card reader. You must format your SD card to Fat32 in order for this device to work correctly. Your SD card must only contain uncompressed .vgm files. VGZ FILES WILL NOT WORK! You may download .vgz files and use [7zip](http://www.7-zip.org/download.html) to extract the uncompressed file out of them. Vgm files on the SD card do not need to have the .vgm extension. As long as they contain valid, uncompressed vgm data, they will be read by the program regardless of their name.
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

Additionally, you can connect buttons to pins 23, 22, and 21 for dedicated playback controls. These buttons should connect to ground when pushed since the software turns on the internal pullup resistors for those pins.

Button Pin | Result
------------ | -------------
23 | Next Track
22 | Random Track
21 | Previous Track

These button pins can also be changed in ChipPinMapping.h

# Schematic
![Schematic](https://github.com/AidanHockey5/Teensy_3_5_VGM_Player_YM2612_SN76489/raw/master/SchematicsAndInfo/Teensy_3_5_VGM_Player.sch.png)

[PDF](https://github.com/AidanHockey5/Teensy_3_5_VGM_Player_YM2612_SN76489/raw/master/SchematicsAndInfo/Teensy_3_5_VGM_Player.pdf)
