/*
SD Card Server example program
*/

#include <ESPAsyncWebServer.h>  //Get from: https://github.com/me-no-dev/ESPAsyncWebServer
#include <SdCardServer.h>
#include <SPI.h>
#include <pgmspace.h>
#include <WiFi.h>

#include "SdFat.h" //http://librarymanager/All#sdfat_exfat by Bill Greiman. Currently uses v2.1.1

//----------------------------------------
// Constants
//----------------------------------------

#define REDIRECT                0       // 0 - Display single link, 1 - redirect to /SD

#define SPI_CS_PIN              25
#define SPI_FREQUENCY           16      // MHz

//----------------------------------------
// Locals
//----------------------------------------

SdFat sd;
SdCardServer * sdCardServer;
unsigned long sdLastRetryTime;
AsyncWebServer * server;
char password[1024];          // WiFi network password
char ssid[1024];              // WiFi network name

//----------------------------------------
// Global routines
//----------------------------------------

// Entry point called by the runtime library initialization routine
// Initialize the device for operation
void setup ()
{
    Serial.begin (115200);

    // Setup the SD card pins
    pinMode (SPI_CS_PIN, OUTPUT);
    digitalWrite (SPI_CS_PIN, HIGH); //Be sure SD is deselected
    sdLastRetryTime = millis();

    // Read the WiFi network name (SSID)
    Serial.println ();
    Serial.printf ("SSID: %s\n", getString ("Please enter the WiFi network name (SSID): ", ssid));
    Serial.println ();

    // Read the WiFi network password
    Serial.printf ("Password: %s\n", getString ("Please enter the WiFi network password: ", password));
    Serial.println ();
}

// Idle loop
void loop ()
{
    bool wifiConnected;

    // Bring the SD card online
    if ((sdLastRetryTime - 250) > sdLastRetryTime)
        mountSdCard ();

    // Connect to the WiFi access point
    wifiConnected = connectWiFi ();

    // Display the IP address
    printIpAddress (wifiConnected);

    // Delay for a while
    delay (10);
}

//----------------------------------------
// Support routines
//----------------------------------------

// Connect WiFi to an access point or WiFi modem
bool connectWiFi ()
{
    int status;
    static int wifiBeginCalled;
    static int wifiConnected = false;

    // Determine the WiFi status
    status = WiFi.status ();
    switch (status) {
    default:
        Serial.printf ("Unknown WiFi status: %d\n", status);
        delay (100);
        break;

    case WL_DISCONNECTED:
    case WL_IDLE_STATUS:
    case WL_NO_SHIELD:
    case WL_SCAN_COMPLETED:
        break;

    case WL_NO_SSID_AVAIL:
        Serial.println ("Please set SSID and pass values!\n");
        while (1);

    case WL_CONNECTED:
        if (!wifiConnected) {
            wifiConnected = true;
            Serial.println ("WiFi Connected");

            // Start the SD card server
            sdCardServer = new SdCardServer(&sd, mountSdCard, "/SD/", "SD Card Files");
            if (sdCardServer) {

                // Start the WiFi server
                server = new AsyncWebServer(80);
                if (server) {

                    // index.html
                    sdCardServer->sdCardWebSite (server, REDIRECT);

                    //  All other pages
                    sdCardServer->onNotFound (server);

                    // Start server
                    server->begin();
                    break;
                }

                // Server not started
                delete sdCardServer;
                sdCardServer = NULL;
            }

            // Attempt to start the SD card server again
            WiFi.disconnect ();
            wifiConnected = false;
            wifiBeginCalled = false;
        }
        break;

    case WL_CONNECTION_LOST:
        Serial.println ("WiFi connection lost");
        WiFi.disconnect ();
        if (sdCardServer) {
            delete sdCardServer;
            sdCardServer = NULL;
        }
        if (server) {
            delete server;
            server = NULL;
        }
        wifiBeginCalled = false;
        wifiConnected = false;
        break;

    case WL_CONNECT_FAILED:
        if (sdCardServer) {
            delete sdCardServer;
            sdCardServer = NULL;
        }
        if (server) {
            delete server;
            server = NULL;
        }
        wifiBeginCalled = false;
        wifiConnected = false;
        break;;
    }

    // Attempt to connect to Wifi network
    if (!wifiBeginCalled) {
        wifiBeginCalled = true;
        WiFi.begin (ssid, password);
        Serial.println ("Waiting for WiFi connection...");
    }

    // Return the connection status
    return wifiConnected;
}

// Get a string from the user
const char * getString (const char * prompt, char * string)
{
    byte data;
    int length = 0;

    // Read the string from the serial port
    do {
        Serial.println (prompt);
        while(1) {
            // Read a character from the serial port
            while (!Serial.available ());
            data = Serial.read ();

            //Done at end of line
            if ((data == '\n') || (data == '\r')) {
                string[length] = 0;
                break;
            }
            string[length++] = data;
        };
    } while (!length);
    return string;
}

// Start the communications with the SD card
int mountSdCard ()
{
    static int sdCardOnline = false;

    //Attempt to initialize the microSD card
    if (!sdCardOnline) {
        if (!sd.begin (SdSpiConfig (SPI_CS_PIN,
                                    SHARED_SPI,
                                    SD_SCK_MHZ(SPI_FREQUENCY))))
            //SD card not available
            digitalWrite(SPI_CS_PIN, HIGH);

        //The SD card is available
        else {

            //Change to root directory
            if (sd.chdir ()) {
                sdCardOnline = true;
                Serial.println (F("SD card online"));
            }
            else
            {
                sd.end ();
                sdCardOnline = false;
                Serial.println (F("SD change directory failed"));
            }
        }
        sdLastRetryTime = millis();
    }
    return sdCardOnline;
}

// Output the IP address on the serial port
void printIpAddress(bool wiFiOnline) {
    static bool addressDisplayed = false;

    // Display the IP address
    if (wiFiOnline && (!addressDisplayed)) {
        Serial.print ("IP Address: ");
        IPAddress ip = WiFi.localIP();
        Serial.println(ip);
    }

    // Only display the address when the WiFi is connected
    addressDisplayed = wiFiOnline;
}

