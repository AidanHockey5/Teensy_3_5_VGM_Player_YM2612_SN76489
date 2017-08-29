# Teensy 3.5 VGM Player Featuring YM2612 & SN76489
A hardware Sega Genesis / Master system video game music player based on the Teensy 3.5

This is the offical successor to my original ESP8266 VGM player https://github.com/AidanHockey5/ESP8266_VGM_Player_PSG_YM2612

I have begun to port over the code from the previous project in order for it to function on the Teensy 3.5.
Initial tests have proven very good, as the Teensy's bountiful direct GPIO pins and blazing fast ARM processor have proven to be perfect for driving these two sound chips.

Goals for this project include:
* SD card support
* OLED HUD screen support
* G3D parsing support
* "Pick your song" via serial support.
* Full playback speed, even for songs with large PCM samples.

This project was built using PlatformIO for Atom.
