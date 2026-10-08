# ESP32_Arduino_BT_Speaker
A Bluetooth speaker implemented in Arduino for ESP32

This presents a Bluetooth (Classic) Audio Sink via A2DP -- Acting as a Bluetooth Speaker/Headphones.
The audio is routed via I2S (at 44.1kHz for A2DP) to an I2S DAC.

Buttons are defined for 'play/pause', 'next' and 'previous' music control.

For volume control, the centre pin of a logarithmic potentiometer is attached to an analog input pin, and the code averages over a number of readings as well.

The 'now playing' metadata (sent by the connected audio transmitting device) is displayed on an I2C display. The sketch is designed for a resolution of 128x64, and the 'now playing' metadata has end-of-text strings, so the text can scroll (with looping) on the display.

The code includes functions to dynamically allocate memory for the text and to safely clean up that memory by deallocating.
