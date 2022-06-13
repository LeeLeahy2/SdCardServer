# Arduino SD Card Server Library
https://github.com/LeeLeahy2/SdCardServer
README file

## License
SD Card Server Library Copyright (C) 2022 Lee Leahy GNU GPL v3.0

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License v3.0 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/gpl.html>

## Introduction
The SD Card Server library provides routines to add a link to a web page that lists the files on the SD card.  Each link on this page displays the file modify date, name and its size.  Clicking on one of these links causes the file to be downloaded from the SD card to the computer running the browser.

## Constructor

### SdCardServer (sd, sdCardPresent, url, serverHeaderText)
##### Description
The constructor initializes an SdCardServer object.
##### Syntax
`SdCardServer (sd, sdCardPresent, url, serverHeaderText);`
##### Required parameter
**sd:** Address of an SdFat object associated with the SD card.  *(SdFat *)*

**sdCardPresent:** Routine address to determine if the SD card is present and available for use.

**url:** Zero terminated string containing the URL relative to the local website.  The URL needs to end in a trailing slash (/).  This web page will list the files on the SD card.  *(const char *)*
##### Optional parameters
**serverHeaderText:** Zero terminated string containing the server name that is added as an optional html header.  *(const char *)*
##### Returns
None.
##### Example
```c++
mySdCardServer = SdCardServer (*sdFat, sdCardPresent, "/SD/");
```

## Callback Function

### SD_CARD_PRESENT()
##### Description
Determine if the SD card is present and ready for use.  This routine exists in
the code calling the SdCardServer library.
##### Syntax
`SD_CARD_PRESENT();`
##### Required parameter
None.
##### Returns
Zero (0) if the SD card is not present.  Non-zero if the SD card is present.
##### Example
```c++
sdCardPresent();
```

## SD Card Server Library Functions

### sdCardWebSite(server)
##### Description
Create a website for the SD card.  Call this routine if a website does not already
exist.  If a website already exists then use sdCardListingWebPageLink to add a link
(HTML anchor) to an existing web page.
##### Syntax
`mySdCardServer.sdCardWebSite(server);`
##### Required parameter
**server:** Address of an AsyncWebServer object  *(AsyncWebServer *)*
**redirect:** Redirect the main web page to the SD card listing page if set true
##### Returns
None.
##### Example
```c++
mySdCardServer.sdCardWebSite = sdCardWebSite (&asyncWebServer);
```

### sdCardListingWebPageLink(buffer, maxLen, linkText, options)
##### Description
Add a link (HTML anchor) to an existing web page.  The link points to the SD
card listing web page.
##### Syntax
`mySdCardServer.sdCardListingWebPageLink(buffer, maxLen, linkText, options);`
##### Required parameter
**buffer:** Address of a zero terminated string to be concatenated with the SD card listing link  *(uint8_t *)*

**maxLen:** Maximum length of the next portion of the HTML response  *(int)*

**linkText:** Text to display for the link  *(const char *)*
##### Optional parameters
**options:** Address of a zero terminated string containing the options, use NULL if no options specified  *(const char *)*
##### Returns
None.
##### Example
```c++
bytesAddedToBuffer = mySdCardServer.sdCardListingWebPageLink (buffer, maxlen, "SD Card Files", "target=\"_blank\"");
```

### isSdCardWebPage(request)
##### Description
Display the SD card listing web page if the requested URL matches the URL passed
to the SdCardServer constructor.  Start the SD card file download if the requested
URL starts with the URL passed to the sdCardServer constructor and the file is
found on the SD card.

This routine is designed to be called from the server.onNotFound event routine.
##### Syntax
`mySdCardServer.isSdCardWebPage(request);`
##### Required parameter
**request:** Address of the AsyncWebServerRequest object  *(AsyncWebServerRequest *)*
##### Returns
Zero (0) if the requested URL does not match the URL passed to the SdCardServer
constructor.  Non-zero if the URL matches and the page is displayed.
##### Example
```c++
if (mySdCardServer.isSdCardWebPage(request))
    return;
...
// URL not found
request->send(404);
```

### onNotFound(server)
##### Description
Add the request not found event.  This event handles the SD card requests for
listing and file download.

Call this routine if and only if the AsyncWebServer.onNotFound event handler is
not delared by the code calling SdCardServer.  The call is made after the the
AsyncWebServer is initialized.

##### Syntax
`mySdCardServer.onNotFound(server);`
##### Required parameter
**server:** Address of an AsyncWebServer object  *(AsyncWebServer *)*
##### Returns
None.
##### Example
```c++
mySdCardServer.onNotFound(server);
```
