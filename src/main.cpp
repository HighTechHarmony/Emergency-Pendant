#include <Arduino.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <DNSServer.h>

const byte DNS_PORT = 53;
IPAddress apIP(8,8,4,4); // The default android DNS
DNSServer dnsServer;


#define BUTTON_PIN 7
#define LED_PIN 2


String ssid = "your_SSID";
String password = "your_PASSWORD";
String phone_ip = "192.168.1.1";
String emergency_number = "999"; 
String phone_passcode = "phonepass";

// Some global timing stuff
unsigned long previousTenthSecondMillis = 0L;
unsigned long previousMillisLED = 999;
long tenthSecond = 100UL;
long previousMillisCALLING = 0L;

byte buttonStillDown = 0;
boolean button_was_pressed = false;

String myState = "STARTUP";

void reset_wifi (void);
void check_pairing_button(void);
void do_emergency_call(void);
void handleSubmit ();
void handleRoot ();
void handleNotFound ();
bool readEEPROM();
void check_emergency_button();
String trim_trailing(String totrim);

WebServer server(80);

void setup() {
  /* Note, apparently in order to achieve correct functioning of deep sleep low power mode on the ESP32C3 
   * you need to set the CPU frequency to 80MHz.  This is done in the platformio.ini file.
   * also you need to comment out Serial.begin(115200) in order to achieve low power mode.
  */
  
  pinMode (LED_PIN, OUTPUT);
  pinMode (BUTTON_PIN, INPUT);
  pinMode (BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  while (!Serial);
  sleep(1); // Wait for serial monitor to open
  Serial.println("Starting up emergency pendant...");


  /**************** WiFi setup **********************************/
  /* See if we have a saved WiFi configuration in EEPROM, if not then start AP for configuration */
  if (!readEEPROM()) {
    Serial.println("No WiFi configuration found, starting AP for configuration");
    

    WiFi.softAP("PendantConfig");

    // WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    // if DNSServer is started with "*" for domain name, it will reply with
    // provided IP to all DNS request
    dnsServer.start(DNS_PORT, "*", apIP);


    // server.on("/", handleRoot);
    server.on("/submit", handleSubmit);

    server.on("/", handleRoot);
    server.on("/generate_204", handleRoot);
    server.onNotFound(handleNotFound);
    server.begin(); // Web server start
    Serial.println("HTTP server started");

    myState = "CONFIGURATION";
    server.begin();

    previousMillisLED = 0;
    while (1) { 
      dnsServer.processNextRequest();
      server.handleClient();

      // Flash the light every 250ms
      if (millis() - previousMillisLED >= 250) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        previousMillisLED += 250;
      }
      delay (1);
    } // Wait here until configuration is complete
  }

  /* See if the button is pressed at startup, if so then reset the WiFi configuration */
  Serial.println("Press prg button within 5 seconds for WiFi reset");
  int i=0;  
  while (i++ < 5000)
  {
    // Flash the light
    if (i % 200 == 0) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    if (!digitalRead(BUTTON_PIN)) {
      Serial.println ("Clearing WiFi configuration...");
      reset_wifi();      
    }
    delay(1);
  }
    
  // If we get to here, we are doing a normal startup and connect in STA mode
  // Turn off the light until normal WiFi connection has been established.
  digitalWrite(LED_PIN, 0);

  // WiFi.begin("core", "Sc0ttSus21!");  
  WiFi.begin (ssid.c_str(), password.c_str());
  int timeout = 10; // Timeout in seconds
  while (WiFi.status() != WL_CONNECTED) {    
    delay(500);
    Serial.print(".");
    if (--timeout == 0) {
      Serial.println("Failed to connect to WiFi");
      ESP.restart();
    }    
  }
  
  Serial.println("Connected to WiFi");
  Serial.println("IPAddress: "+WiFi.localIP().toString());

  digitalWrite(LED_PIN, 1);  // Turn on the LED indicator
  
  myState = "IDLE";

}


void loop() {


  /* Failed attempt at trying to make some fancy pants light indication */
  // const long interval = 1000; // interval at which to blink (milliseconds)
  // int brightness = 0; // LED brightness
  // int fadeAmount = 5; // how many points to fade the LED by
  // unsigned long previousMillis = 0; // will store last time LED was updated

  // unsigned long currentMillis = millis();
  // if (currentMillis - previousMillis >= interval) {
  //   previousMillis = currentMillis;
  //   analogWrite(LED_PIN, brightness);
  //   brightness = brightness + fadeAmount;
  //   if (brightness <= 0 || brightness >= 255) {
  //     fadeAmount = -fadeAmount;
  //   }
  // }


  /* Not necessary, performed entirely in setup function */
  // if (myState == "CONFIGURATION") {
  // // Flash the light every 250ms
  //   if (millis() - previousMillisLED >= 250) {
  //     digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  //     previousMillisLED += 250;
  //   }
    
  // }

  /* Do our thing */
  if (myState == "IDLE") {
    check_emergency_button();    

    delay(1);    
  }

  /* If the state has been CALLING for more than 10 seconds, then give up and go back to IDLE */
  if (myState == "CALLING") {
    if (millis() - previousMillisCALLING >= 10000) {
      Serial.println("Hanging up and returning to IDLE state");
      myState = "IDLE";
      digitalWrite(LED_PIN, 1);  // Turn on the LED indicator     
      
    }
  }
  
}





void check_emergency_button() {
  // Button polling do this every 100ms
    if (millis() - previousTenthSecondMillis >= tenthSecond) {
      // Serial.println("Check");

      // check the button (pin 0 --> button --> gnd)
      if (!digitalRead(BUTTON_PIN)) {
        if ((buttonStillDown++ > 0 ) && (buttonStillDown < 3)) {
          button_was_pressed=true;        
        } 
        else 
        {
          if (buttonStillDown > 50 ) {  // 5 seconds

            // do the held down thing
            Serial.println("Held");
            //reset_wifi();
            button_was_pressed=0;
            
            
            /* If we are not already trying to make a call, then make a call */
            // MAKE EMERGENCY CALL
            if (myState != "CALLING") {
              myState = "CALLING";
              do_emergency_call();
            }

          }
        }
      } else {  // Button is not pressed
        buttonStillDown = 0;
        // if (button_was_pressed && myState != "PAIRING") {
        //   // do the single click thing
        //   Serial.println("Clicked");        
        //   button_was_pressed=0;
          
        //   reset_ble();
        // }
      }

      previousTenthSecondMillis += tenthSecond;
    }
}



// This is run when the user presses the button for WiFi reset (during bootup)
void reset_wifi (void)
{
  Serial.println("Resetting WiFi config...");    
  // ESPConnect.erase();
  // WiFi.disconnect(true, true); // Clear memorized STA configuration
  // if (!WiFi.isConnected())
  //   Serial.println("WiFi disconnected");
  //   digitalWrite(LED_PIN, 0);
  // delay(100);
  EEPROM.begin(512);
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();

  ESP.restart();
  
}

void do_emergency_call() {
  
  Serial.println("Making emergency call!!");

  // Start the timer for the call attempt
  previousMillisCALLING = millis();

  HTTPClient http;
  // http.begin("http://192.168.15.87/cgi-bin/api-make_call?passcode=ledsrgood&phonenumber=8027352133");
  String api_string = "http://" + phone_ip + "/cgi-bin/api-make_call?passcode=" + phone_passcode + "&phonenumber=" + emergency_number;
  Serial.println("api_string =" + api_string );
  http.begin (api_string);

  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println(httpCode);
    Serial.println(payload);
  }
  http.end();
  
}

/* Attempts to read the config out of EEPROM, sets global variables and returns true if successful, otherwise returns false */
bool readEEPROM() {
  EEPROM.begin(512);
  ssid = EEPROM.readString(0);
  password = EEPROM.readString(32);
  phone_ip = EEPROM.readString(64);
  phone_passcode = EEPROM.readString(96);
  emergency_number = EEPROM.readString(128);
  

  if (ssid == "" || password == "" || phone_ip == "" || emergency_number == "") {
    return false;
  }

  else {
    return true;
  }

}



void handleRoot() {
  server.send(200, "text/html", "<form action=\"/submit\" method=\"POST\">SSID:<br><input type=\"text\" name=\"ssid\"><br>Password:<br><input type=\"password\" name=\"password\"><br>Phone IP Address:<br><input type=\"text\" name=\"phone_ip\"><br>Phone passcode:<br><input type=\"text\" name=\"phone_passcode\"><br>Emergency Phone Number:<br><input type=\"text\" name=\"emergency_number\"><br><br><input type=\"submit\" value=\"Submit\"></form>");
}

void handleSubmit() {
  ssid = trim_trailing (server.arg("ssid"));
  password = trim_trailing(server.arg("password"));
  phone_ip = trim_trailing(server.arg("phone_ip"));
  phone_passcode = trim_trailing(server.arg("phone_passcode"));
  emergency_number = trim_trailing(server.arg("emergency_number"));


  EEPROM.begin(512);
  EEPROM.writeString(0, ssid);
  EEPROM.writeString(32, password);
  EEPROM.writeString(64, phone_ip);
  EEPROM.writeString(96, phone_passcode);
  EEPROM.writeString(128, emergency_number);
  EEPROM.commit();

  Serial.println (ssid);
  Serial.println (password);
  Serial.println (phone_ip);
  Serial.println (phone_passcode);
  Serial.println (emergency_number);

  server.send(200, "text/html", "<h1>Success</h1><p>Restarting...</p>");

  ESP.restart();
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

/* Helper function to trim off any whitespace or carriage returns from the end of a string */
String trim_trailing(String totrim) {
  int i = totrim.length() - 1;
  while (i >= 0 && (totrim[i] == ' ' || totrim[i] == '\r' || totrim[i] == '\n')) {
    i--;
  }
  return totrim.substring(0, i + 1);
}