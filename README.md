# SesquiSmartESP32 - sesquismart-esp32
Free (as in speech) firmware for the CoreBoxx/Inbxx semi-smart add-on running on an Arduino Nano ESP32.

The original semi-smart add-on by Voxel3d (https://www.printables.com/model/1243667-coreboxx-heated-semi-smart-add-on) uses
the Arduino Nano (with an ATmega 328P), which is quite limited in its capabilities, while the semi-smart advanced version
(https://www.printables.com/model/1480364-semi-smart-advanced) uses an Arduino Nano R4.

To allow for truly convenient control of the INBXX, this version based on an Arduino Nano ESP32 is now available.

This firmware for the Nano ESP32 has been written from scratch and is **licensed under the GPL**. It is and will remain open and free (as in speech).

Features:
* fully managed with it's built-in webserver - control and manage INBXX operation conveniently from your desktop.
* SH1107 128x128 OLED display.
* WS2811/WS2812 RGB-LED stripe support.
* ...

This firmware leverages the capabilities of the Nano ESP32 and its dual-core processor. Consequently, a backport to the Nano R4 is unfortunately not possible.

Hardware - what do you need:
* an INBXX with a semi-smart add-on. Heating unit and HMI/Display SH1107 of the advanced version.
* the SesquiSmartESP32 Piggyback. The Nano ESP32 is not 100% compatible with the Arduino Nano pinout used on the semi-smart
mainboard and uses 3.3V instead of 5V. For the Nano ESP32 the piggyback is necessary for reliable operation.
(_NOTE: currently there is no ready-made PCB available. You have to solder your own!_)
