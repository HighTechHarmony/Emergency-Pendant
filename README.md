# Emergency-Pendant
ESP32 based emergency calling pendant, for use with Grandstream GXP series VoIP phones

It was built and tested on a Seeed Studio Xiao ESP32C3, but could and should work on other ESP32 variations.

## Configuration

GXP phone must have the following config changes in order for the CTI to work. You can read more about this here:
https://www.grandstream.com/hubfs/Product_Documentation/CTI_Guide.pdf

### Short version: 
Network->Remote Control->Action URI Support: Enabled
Network->Remote Control->Remote Control Pop Up Window Support: Disabled
Action URI Allowed IP List: IP address of ESP8266, or "any" (no quotes, all lowercase)
Settings->Call Features->Click To Dial Feature: Enabled
reboot



Pendant boots to hotspot (SSID "PendantConfig" w/ captive portal. Connect using smartphone. 
Enter details: SSID and preshared key of your WiFi network, IP address of the GXP phone, passcode for phone (which is the admin password), and an emergency number to dial
Pendant reboots and connects to WiFi and waits for button activation.


## Operation

Emergency button is activated (long press or many repeated presses, whatever),
Connects to the Grandstream CPT and initiates a call to the designated number
Since the GXP features on-hook dialing, speaker phone is automatically activated, and works reasonably well even if the user is somewhat far-field 
Emergency number can be a loved one’s cell, or a ring group on a PBX that calls multiple contacts simultaneously

After 10 seconds the pendant will return to idle state and await a key press.

