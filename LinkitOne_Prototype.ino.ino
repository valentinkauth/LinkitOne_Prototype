#include "LTask.h"
// Accelerometer
#include <ADXL345.h>
// Batter level and state
#include <LBattery.h>
// SD card
#include <LSD.h>
// WiFi connection
#include <LWiFi.h>
// Sending data to web server
#include "LWiFiClient.h"
// Date and time
#include <LDateTime.h>
// UDP connection
#include <LWiFiUDP.h>

// WiFi AP login data
#define WIFI_AP "Valentin iPhone"
#define WIFI_PASSWORD "valentin"
#define WIFI_AUTH LWIFI_WPA  // choose from LWIFI_OPEN, LWIFI_WPA, or LWIFI_WEP.
#define SERVER_URL "https://bootcamp01.000webhostapp.com/"

// SD card file names
#define CACHE_FILE "cache.txt"
#define LOCALSTORAGE_FILE "local.csv"

// WiFi client for server communication
LWiFiClient client;

// Time server
#define TIME_SERVER "0.nl.pool.ntp.org" // a list of NTP servers: http://tf.nist.gov/tf-cgi/servers.cgi
// UDP package for the time server
LWiFiUDP Udp;
unsigned int localPort = 2390;      // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// datetimeinfo variable that should hold the current time after the setup
datetimeInfo currentTime;

//variable adxl is an instance of the ADXL345 library (accelerometer)
ADXL345 adxl;

// Intervals to proceed with periodical operations
// 20 seconds
const long millisIntervalStore = 20000;
// 60 seconds
const long millisIntervalSend = 60000;

// long value for millis operations
unsigned long previousMillisStore = 0;
unsigned long previousMillisSend = 0;

// Boolean that shows whether usage/vibration was detected
boolean usageDetected = false;

// Initial previous measured value
int previousValue = 0;
int previousValueX = 0;
int previousValueY = 0;

// treshold when acceleration is triggered as movement/usage
const int acceleromationTreshold = 20;

// manual time variables
int year = 2017;
int mon = 4;
int day = 13;
int hour = 0;
int min = 0;
int sec = 0;


void setup() {
  Serial.begin(9600);

  // Initialize the SD card
  Serial.print("Initializing SD card...");
  LSD.begin();
  Serial.println("card initialized.");
  Serial.println();

  // Start WiFI
  LWiFi.begin();
  Serial.print("Connecting to WiFi Access Point...");
  while (0 == LWiFi.connect(WIFI_AP, LWiFiLoginInfo(WIFI_AUTH, WIFI_PASSWORD))) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("connected.");
  Serial.println();
  // Print status when connected
  printWifiStatus();

  // Get time via Udp connection from NTP server
  getNtpTime();

  // Set time (ntp time if udp request successful or manual time)
  setTime();

  // Power on the accelerometer
  adxl.powerOn();

}

void loop() {

  // Get timestamp of current loop
  LDateTime.getTime(&currentTime);

  unsigned long currentMillisStore = millis();
  unsigned long currentMillisSend = millis();

  int currentValue; // currently z axis values for upright position
  int y, x;
  //read the accelerometer values and store them in variables x,y,z
  adxl.readXYZ(&x, &y, &currentValue);

  // Print all measured differences for treshold testing purposes
  Serial.print("Z: ");
  Serial.println(abs(currentValue - previousValue));
  Serial.print("X: ");
  Serial.println(abs(x - previousValueX));
  Serial.print("Y: ");
  Serial.println(abs(y - previousValueY));
  Serial.println();
  
  
  // If no usage was detected, still look for it.
  if (!usageDetected) {
    if (abs(currentValue - previousValue) > acceleromationTreshold) {
      usageDetected = true;
      // Print manual timestamp of usage measurement
      Serial.print(getDateString(currentTime));
      Serial.println(": Usage detected! No more measurements till next update");
      Serial.println();
    }
  }

  // Store data every x minutes/seconds
  if (currentMillisStore - previousMillisStore >= millisIntervalStore) {
    // save the last time an update was sent
    previousMillisStore = currentMillisStore;

    // Write to SD card
    writeToStorage();
  }


  // Send data every x minutes/seconds
  if (currentMillisSend - previousMillisSend >= millisIntervalSend) {
    // save the last time an update was sent
    previousMillisSend = currentMillisSend;

    // Send data to web server
    //sendData();
  }

  // Update the previous measured value with the current measured value
  previousValue = currentValue;
  previousValueX = x;
  previousValueY = y;

  // Wait to not overload
  delay(700);

}


// Write current Timestamp and usage to the SD card
void writeToStorage() {

  // String that gets written to local storage and cache files
  String dataString = "";
  dataString += getDateString(currentTime);
  dataString += ", ";

  if (usageDetected) {
    dataString += "1";
  } else {
    dataString += "0";
  }

  Serial.println("Trying to access files on SD card");

  // Open cache file with write access
  LFile dataFile = LSD.open(CACHE_FILE, FILE_WRITE);
  Serial.println("Cache file opened");
  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
    Serial.println("Cache file written and closed");
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening cache file");
  }

  // Open local storage file with write access
  dataFile = LSD.open(LOCALSTORAGE_FILE, FILE_WRITE);
  Serial.println("Local storage file opened");
  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
    Serial.println("Local storage file written and closed");
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening local storage file");
  }

  Serial.println();

  // Reset detection boolean
  usageDetected = false;
}


// Send data from cache/buffer file to web server
void sendData() {
  Serial.println("Sending data to web server...");
  // TODO: If sending successful: empty cache file, if sending not successfull: keep going with
  // filled cache will till next try.

  // keep retrying until connected to website
  Serial.print("Connecting to website...");
  while (0 == client.connect(SERVER_URL, 80))
  {
    Serial.println(".");
    delay(1000);
  }
  // TODO: Stop after 5 attempts to not interupt the whole code

  // Open file from sd card
  LFile dataFile = LSD.open(CACHE_FILE);
  // TODO: Open the file and post the contents or the whole file to the php web server for processing
  // TODO: If post successfull empty the cache file contents, in not successful, keep the cache contents
  if (dataFile) {
        Serial.println("test.txt:");
        dataFile.seek(0);
        // read from the file until there's nothing else in it:
        while (dataFile.available()) {            
            Serial.write(dataFile.read());
        }
        // close the file:
        dataFile.close();
    } else {
        // if the file didn't open, print an error:
        Serial.println("sendData(): error opening test.txt");
    }
  
  

}


// Empty (or delete) the cache file on the SD card
void emptyCacheFile() {
  // TODO: Clear the chache file contents (e.g. when sending to server was successful)
}



//________________________
// HELPERS

// get the current WiFi status printed
void printWifiStatus() {
  // print the current overall state of the connection
  Serial.print("State: ");
  Serial.println(LWiFi.status());
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(LWiFi.SSID());
  // print your WiFi shield's IP address:
  IPAddress ip = LWiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  // print the subnet mask
  Serial.print("subnet mask: ");
  Serial.println(LWiFi.subnetMask());
  // print the gateway ip
  Serial.print("gateway IP: ");
  Serial.println(LWiFi.gatewayIP());
  // print the received signal strength:
  long rssi = LWiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.println();
}


// Set time from global time variables
void setTime() {
  // Set time stamp manually
  datetimeInfo now;
  now.year = year;
  now.mon = mon;
  now.day = day;
  now.hour = hour;
  now.min = min;
  now.sec = sec;
  LDateTime.setTime(&now);
}

// Retunrs readable date string for timestamp
String getDateString(datetimeInfo dti) {
  // Output format: "DD/MM/YY hh:mm:ss"
  String dateStr;
  dateStr += dti.day;
  dateStr += "/";
  dateStr += dti.mon;
  dateStr += "/";
  dateStr += dti.year;
  dateStr += ", ";
  dateStr += dti.hour;
  dateStr += ".";
  dateStr += dti.min;
  dateStr += ".";
  dateStr += dti.sec;
  return dateStr;
}


// NTP time server stuff
//____________________________________________

// Connect to udp/time server, https://github.com/brucetsao/techbang/blob/master/201511/LinkIt-ONE-IDE/hardware/arduino/mtk/libraries/LGPRS/examples/GPRSUdpNtpClient/GPRSUdpNtpClient.ino
void getNtpTime() {
  Serial.print("\nStarting connection to time server...");
  while (!Udp.begin(localPort)) {
    Serial.println("retry begin");
    delay(1000);
  }
  Serial.println("connected");

  sendNTPpacket();
  // wait to see if a reply is available
  delay(1000);

  Serial.println( Udp.parsePacket() );
  if ( Udp.parsePacket() ) {
    Serial.println("Udp packet received");
    // We've received a packet, read the data from it
    memset(packetBuffer, 0xcd, NTP_PACKET_SIZE);
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = " );
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    Serial.println(epoch);

    // print the hour, minute and second:
    Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
    // Set global hour variable
    hour = (epoch  % 86400L) / 3600;
    Serial.print(hour); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    // Set global minute variable
    min = (epoch  % 3600) / 60;
    Serial.print(min); // print the minute (3600 equals secs per minute)
    Serial.print(':');
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    // Set global second variable
    sec = epoch % 60;
    Serial.println(sec); // print the second
  }
  Serial.println();
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket() {
  Serial.println("sendNTPpacket");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(TIME_SERVER, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
