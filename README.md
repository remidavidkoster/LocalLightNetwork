# Redako-LLN

This place will host the "Local Light Network" repository - a 10 year old project (then called "everything wireless boards"), brought back to life, on the STM32WB platform, instead of the Arduino & NRF24L01+ hardware back in the days. Expect a couple of PCBs, a couple of firmware projects, and perhaps some Python code to end up here.

Things to be developed:

- Hardware to control high CRI LEDs (and strips) with high resolution & high frequency PWM
  - Firmware for the above, with gamma correction and smooth dimming sequences
- Hardware to input encoders, sliders, and buttons, and use these to control the light modules
  - Communication and linking firmware to match controls to lights
- Optionally, a bridge, to interface all of this with home assistant
  - (Or If I don't feel like writing too much firmware I might just use the STM32WB ZigBee stack altogether)
- Some kind of dashboard. Either home assistant based, or a discrete STM32WB with touch display. 



