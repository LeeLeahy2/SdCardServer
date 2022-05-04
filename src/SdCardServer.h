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
//      Determine if the SD card is present and ready for use.  This routine
//      exists in the code calling the SdCardServer library.
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
    //      sd: Address of an SdFat object associated with the SD card.
    //      sdCardPresent: Routine address to determine if the SD card is
    //          present and available for use.
    //      url: Zero terminated string containing the URL relative to the
    //          local website.  The URL needs to end in a trailing slash (/).
    //          This web page will list the files on the SD card.
    //      serverHeaderText: Zero terminated string containing the server name
    //          that is added as an optional html header.
    //--------------------------------------------------------------------------
    SdCardServer (
        SdFat * sd,
        SD_CARD_PRESENT sdCardPresent,
        const char * url,
        const char * serverHeaderText = NULL
        );

    //--------------------------------------------------------------------------
    // isSdCardWebPage
    //      Display the SD card listing web page if the requested URL matches
    //      the URL passed to the SdCardServer constructor.  Start the SD card
    //      file download if the requested URL starts with the URL passed to
    //      the sdCardServer constructor and the file is found on the SD card.
    //
    //      This routine is designed to be called from the server.onNotFound
    //      event routine.
    //
    //  Inputs:
    //      request: Address of the AsyncWebServerRequest object
    //
    //  Returns:
    //      Zero (0) if the requested URL does not match the URL passed to
    //      the SdCardServer constructor.  Non-zero if the URL matches and
    //      the page is displayed.
    //--------------------------------------------------------------------------
    int
    isSdCardWebPage(
        AsyncWebServerRequest * request
        );

    //--------------------------------------------------------------------------
    // sdCardListingWebPageLink
    //      Add a link (HTML anchor) to an existing web page.  The link points
    //      to the SD card listing web page.
    //
    //  Inputs:
    //      buffer: Address of a zero terminated string to be concatenated with
    //          the SD card listing link
    //      maxLen: Maximum length of the next portion of the HTML response
    //      linkText: Text to display for the link
    //      options: Address of a zero terminated string containing the options,
    //          use NULL if no options specified
    //
    //  Returns:
    //      The number of characters written to the response buffer
    //--------------------------------------------------------------------------
    int
    sdCardListingWebPageLink(
        char * buffer,
        size_t maxLen,
        const char * linkText,
        const char * options = NULL
        );

    //--------------------------------------------------------------------------
    // sdCardWebSite
    //      Create a website for the SD card.  Call this routine if a website
    //      does not already exist.  If a website already exists then use
    //      sdCardListingWebPageLink to add a link (HTML anchor) to an existing
    //      web page.
    //
    //  Inputs:
    //      server: Address of an AsyncWebServer object
    //--------------------------------------------------------------------------
    void
    sdCardWebSite(
        AsyncWebServer * server
        );

    //--------------------------------------------------------------------------
    // onNotFound
    //      Add the request not found event.  This event handles the SD card
    //      requests for listing and file download.
    //
    //      Call this routine if and only if the AsyncWebServer.onNotFound event
    //      handler is not delared by the code calling SdCardServer.  The call
    //      is made after the the AsyncWebServer is initialized.
    //
    //  Inputs:
    //      server: Address of an AsyncWebServer object
    //--------------------------------------------------------------------------
    void
    onNotFound (
        AsyncWebServer * server
        );
};

#endif  // SD_CARD_SERVER_H_INCLUDED
