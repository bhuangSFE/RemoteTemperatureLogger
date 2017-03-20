/********************************************************************************
 * RemoteTemperatureLogger.ino
 * 
 * Written by: B. Huang (brian@sparkfun.com)
 * Please let me know if you have problems with this code. 
 *  
 * Reads temperature and pressure data and posts this to data.sparkfun.com
 * useing SparkFun ESP8266 Thing Dev, BMP180, and Phant. 
 ********************************************************************************/

// Include the ESP8266 WiFi library. (Works a lot like the Arduino WiFi library.)
#include <ESP8266WiFi.h>

// Include the SparkFun Phant library.
#include "Phant.h"

// Include the BMP180 library and the wire library for the sensor
#include "SFE_BMP180.h"
#include <Wire.h>

//////////////////////
// WiFi Definitions //
//////////////////////
const char WiFiSSID[] = "put your SSID here";
const char WiFiPSK[] = "put your WiFi password here";

/////////////////////
// Pin Definitions //
/////////////////////
const int LED_PIN = 5; // Thing's onboard, green LED

////////////////
// Phant Keys //
////////////////
const char PhantHost[] = "data.sparkfun.com";
const char PublicKey[] = "put your PUBLIC key here";
const char PrivateKey[] = "put your PRIVATE key here";
// fields: id,n,pressure,tempf,time

/////////////////
// Post Timing //
/////////////////
const unsigned long postRate = 60000;  //rate to post data in milliseconds
unsigned long lastPost = 0;
unsigned long ndx = 0;

SFE_BMP180 pressure;
#define ALTITUDE 1655.0 //altitute or elevation of your location in meters
char status;
double T, P, p0, a, tempf; //global variables used by bmp180

/********************************************************************************/
void setup()
{
  Serial.begin(9600);
  Serial.println("Starting Now!");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  initBMP180();
  blink(3);  //three quick blinks at the beginning indicates successful init of BMP
  delay(500);
  connectWiFi();
  digitalWrite(LED_PIN, HIGH); //light on indicates successful connection to WiFi
}

/********************************************************************************/
void loop()
{
  if (lastPost + postRate <= millis())
  {
    readBMP180();
    blink(1);  //quick blinks indicates a successful data read
    delay(100);  
    if (postToPhant())
      lastPost = millis();
    else
      delay(5000);  // wait 5 seconds before re-trying
  }
}

/********************************************************************************/
void blink(int numTimes)
{
  for (int x=0; x<numTimes;x++)
  {
    digitalWrite(LED_PIN, HIGH);
    delay(100);  
    digitalWrite(LED_PIN, LOW);
    delay(100);  
  }
}
/********************************************************************************/
int postToPhant()
{
  // LED turns on when we enter, it'll go off when we
  // successfully post.
  digitalWrite(LED_PIN, HIGH);

  // Declare an object from the Phant library - phant
  Phant phant(PhantHost, PublicKey, PrivateKey);

  // Do a little work to get a unique-ish name. Append the
  // last two bytes of the MAC (HEX'd) to "Thing-":
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();

  // Add the four field/value pairs defined by our stream:
  phant.add("id", macID);
  phant.add("n", ndx);
  phant.add("pressure", P);
  phant.add("tempf", tempf);
  phant.add("time", millis()/1000);

  ndx++;
  
  // Connect to data.sparkfun.com, and post our data:
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(PhantHost, httpPort))
  {
    // If we fail to connect, return 0.
    return 0;
  }
  // If we successfully connected, print our Phant post:
  client.print(phant.post());

  // Read all the lines of the reply from server and print them to Serial
  while (client.available()) {
    String line = client.readStringUntil('\r');
    //Serial.print(line); // Trying to avoid using serial
  }

  // Before we exit, turn the LED off.
  blink(3); //two successive blinks indicates success
  return 1; // Return success
}
/********************************************************************************/
void connectWiFi()
{
  byte ledStatus = LOW;

  // Set WiFi mode to station (as opposed to AP or AP_STA)
  WiFi.mode(WIFI_STA);

  // WiFI.begin([ssid], [passkey]) initiates a WiFI connection
  // to the stated [ssid], using the [passkey] as a WPA, WPA2,
  // or WEP passphrase.
  WiFi.begin(WiFiSSID, WiFiPSK);

  // Use the WiFi.status() function to check if the ESP8266
  // is connected to a WiFi network.
  while (WiFi.status() != WL_CONNECTED)
  {
    // Blink the LED
    digitalWrite(LED_PIN, ledStatus); // Write LED high/low
    ledStatus = (ledStatus == HIGH) ? LOW : HIGH;

    // Delays allow the ESP8266 to perform critical tasks
    // defined outside of the sketch. These tasks include
    // setting up, and maintaining, a WiFi connection.
    delay(100);
    // Potentially infinite loops are generally dangerous.
    // Add delays -- allowing the processor to perform other
    // tasks -- wherever possible.
  }
}

/********************************************************************************/
void initBMP180()
{
  if (pressure.begin())
    Serial.println("BMP180 init success");
  else
  {
    // Oops, something went wrong, this is usually a connection problem,
    // see the comments at the top of this sketch for the proper connections.

    Serial.println("BMP180 init fail\n\n");
    while (1)
    {
      digitalWrite(LED_PIN, HIGH);
      delay(50);
      digitalWrite(LED_PIN, LOW);
      delay(50);
    } // Pause forever w/ fast blink.
  }
}

/********************************************************************************/
void readBMP180()
{
  byte status = pressure.startTemperature();
  if (status != 0)
  {
    // Wait for the measurement to complete:
    delay(status);
    status = pressure.getTemperature(T);
    if (status != 0)
    {
      tempf = (9.0 / 5.0) * T + 32.0;
      // Start a pressure measurement:
      status = pressure.startPressure(3);  // 3 indicates highest resolution meas.
      if (status != 0)
      {
        // Wait for the measurement to complete:
        delay(status);
        // Retrieve the completed pressure measurement:
        status = pressure.getPressure(P, T);
        if (status != 0)
        {
          // Parameters: P = absolute pressure in mb, ALTITUDE = current altitude in m.
          // Result: p0 = sea-level compensated pressure in mb
          p0 = pressure.sealevel(P, ALTITUDE); // we're at 1655 meters (Boulder, CO)

          // Parameters: P = absolute pressure in mb, p0 = baseline pressure in mb.
          // Result: a = altitude in m.

          a = pressure.altitude(P, p0);
        }
        else Serial.println("error retrieving pressure measurement\n");
      }
      else Serial.println("error starting pressure measurement\n");
    }
    else Serial.println("error retrieving temperature measurement\n");
  }
  else Serial.println("error starting temperature measurement\n");  
}

