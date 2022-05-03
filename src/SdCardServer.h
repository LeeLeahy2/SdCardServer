// Arduino SD Card Server Library
// https://github.com/LeeLeahy2/SdCardServer
// Copyright (C) 2022 by Lee Leahy and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html

#ifndef SD_CARD_SERVER_H_INCLUDED
#define SD_CARD_SERVER_H_INCLUDED

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SdFat.h" //http://librarymanager/All#sdfat_exfat by Bill Greiman. Currently uses v2.1.1

//------------------------------------------------------------------------------
// SD_CARD_PRESENT
//      Determine if the SD card is present and ready for use
//
//  Inputs:
//      var: String containing the token found in the HTML response
//
//  Returns:
//      Zero (0) if the SD card is not present.  Non-zero if the SD card is
//      present.
//------------------------------------------------------------------------------
typedef
int
(* SD_CARD_PRESENT) (
    void
    );

class SdCardServer
{
public:
    //--------------------------------------------------------------------------
    // SdCardServer
    //      Initialize an SdCardServer object
    //
    //  Inputs:
    //      sdFat: Address of an SdFat object associated with the SD card.
    //      sdCardPresent: Routine address to determine if the SD card is
    //          present and available for use.
    //      server: Zero terminated string containing the server name that
    //          is added as an html header.
    //      url: Zero terminated string containing the URL relative to the
    //          local website.  The URL needs to end in a trailing slash (/).
    //          This web page will list the files on the SD card.
    //--------------------------------------------------------------------------
    SdCardServer (
        SdFat * sd,
        SD_CARD_PRESENT sdCardPresent,
        const char * server,
        const char * url
        );

    //--------------------------------------------------------------------------
    // sdCardWebPage
    //      Display the SD card web page if the requested URL matches the
    //      URL passed to the SdCardServer initializer
    //
    //  Inputs:
    //      request: Address of the AsyncWebServerRequest object
    //
    //  Returns:
    //      Zero (0) if the requested URL does not match the URL passed to
    //      the SdCardServer initializer.  Non-zero if the URL matches and
    //      the page is displayed.
    //--------------------------------------------------------------------------
    int
    sdCardWebPage(
        AsyncWebServerRequest * request
        );

private:

    const char * server;    // Zero terminated string for web server name
    SD_CARD_PRESENT sdCardPresent; // Routine to determine if SD card is present
    const char * webPage;   // Zero terminated string for SD card's web pages
    int webPageMissingSlash;// Non zero if last character is a not a slash
    int webPageLength;      // Length of the webPage string

    void
    listingPage (
        AsyncWebServerRequest * request
        );

    int
    mountSdCard(
        void
        );

    uint64_t
    sdCardSize(
        void
        );
};

#endif  // SD_CARD_SERVER_H_INCLUDED
