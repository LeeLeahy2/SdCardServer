// Arduino SD Card Server Library
// https://github.com/LeeLeahy2/SdCardServer
// Copyright (C) 2022 by Lee Leahy and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html

#include <malloc.h>
#include <string.h>

#include "SdCardServer.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// Constants
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#define MAX_FILE_NAME_SIZE      (256 * 3)   // File names are 255 UTF-8 characters

//#define NEXT_ENTRY_SIZE         ((2 * MAX_FILE_NAME_SIZE) + 128)
#define NEXT_ENTRY_SIZE         MAX_FILE_NAME_SIZE

typedef enum {
    LS_HEADER = 0,
    LS_DISPLAY_FILES,
    LS_TRAILER,
    LS_DONE
} LISTING_STATE;

//------------------------------------------------------------------------------
// HTML header pieces
//------------------------------------------------------------------------------

static prog_char htmlHeaderStart[] PROGMEM = "<!DOCTYPE HTML>\n<html lang=\"en\">\n<head>\n  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/>\n  <title>";

static prog_char htmlHeaderEndBodyStart[] PROGMEM = R"rawliteral(</title>
</head>
<body>)rawliteral";

static prog_char htmlBodyEnd[] PROGMEM = R"rawliteral(</body>
</html>
)rawliteral";

//------------------------------------------------------------------------------
// html anchor
//------------------------------------------------------------------------------

static prog_char htmlAnchorStart[] PROGMEM = R"rawliteral(<a target=)rawliteral";

static prog_char htmlAnchorBlank[] PROGMEM = R"rawliteral(_blank)rawliteral";

static prog_char htmlAnchorHref[] PROGMEM = R"rawliteral( href=)rawliteral";

static prog_char htmlAnchorCenter[] PROGMEM = R"rawliteral(>)rawliteral";

static prog_char htmlAnchorEnd[] PROGMEM = R"rawliteral(</a>)rawliteral";

//------------------------------------------------------------------------------
// html lists
//------------------------------------------------------------------------------

static prog_char htmlListItemStart[] PROGMEM = R"rawliteral(    <li>)rawliteral";

static prog_char htmlListItemEnd[] PROGMEM = R"rawliteral(</li>
)rawliteral";

static prog_char htmlUlListStart[] PROGMEM = R"rawliteral(  <ol>
)rawliteral";

static prog_char htmlUlListEnd[] PROGMEM = R"rawliteral(  </ol>
)rawliteral";

//------------------------------------------------------------------------------
// sd/
//------------------------------------------------------------------------------

static const char sdFilesH1[] PROGMEM = "%SZ% SD Card";

static prog_char sdHeader[] PROGMEM = R"rawliteral(%H%%H1%%/HB%
  <h1>%H1%</h1>
)rawliteral";

static prog_char sdNoCard[] PROGMEM = R"rawliteral(
  <p>MicroSD card socket is empty!</p>
)rawliteral";

static prog_char sdNoFiles[] PROGMEM = R"rawliteral(
  <p>No files found!</p>
)rawliteral";

static prog_char sdDirectory[] PROGMEM = "/sd/";

//------------------------------------------------------------------------------
// index.html
//------------------------------------------------------------------------------

static const char htmlTitle[] PROGMEM = "SD Card Server";

static const char index_html[] PROGMEM = R"rawliteral(%H%%T%%/HB%
  <h1>%T%</h1>
  <p>%A%%SD%%Q%>%H1%</a></p>
%/B%
)rawliteral";

static const char no_sd_card_html[] PROGMEM = R"rawliteral(%H%%T%%/HB%
  <h1>%T%</h1>
  <p>ERROR - SD card not present!</a></p>
%/B%
)rawliteral";

static const char invalid_SD_card_format_html[] PROGMEM = R"rawliteral(%H%%T%%/HB%
  <h1>%T%</h1>
  <p>ERROR - SD card has invalid format!</a></p>
%/B%
)rawliteral";

static const char not_implemented_html[] PROGMEM = R"rawliteral(%H%%T%%/HB%
  <h1>%T%</h1>
  <p>ERROR - Not implemented!</a></p>
%/B%
)rawliteral";

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// Locals
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

static char htmlBuffer[256];    // Buffer for HTML token replacement
static int sdCardEmpty;         // No files found in the FAT file system
static float sdCardSizeMB;      // Size of the SD card in MB (1000 * 1000 bytes)
static SdFat * sdFat;           // Address of a SdFat object
static SdFile sdFile;           // File on the SD card
static SdFile sdRootDir;        // Root directory file on the SD card
static LISTING_STATE state;     // Listing state

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// Support Classes
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

class HtmlPrint : public Print
{
    uint8_t * buf;

public:
    HtmlPrint(void) {
        buf = NULL;
    }

    void setBufferAddress(uint8_t * buffer) {
        buf = buffer;
    }

    size_t write(uint8_t data) {
        *buf++ = data;
        *buf = 0;
        return 1;
    }

    size_t write(const uint8_t *buffer, size_t size) {
        memcpy (buf, buffer, size);
        buf[size] = 0;
        buf += size;
        return size;
    }
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// Support routines
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//------------------------------------------------------------------------------
// processor
//      Process the tokens in the HTML strings passed to AsyncWebServer in
//      the HTML response
//
//  Inputs:
//      var: String containing the token found in the HTML response
//
//  Returns:
//      Returns the string to replace the token in the HTML response
//------------------------------------------------------------------------------
static
String
processor (
    const String& var
    )
{
    if (var == "A") {
        strcpy_P(htmlBuffer, htmlAnchorStart);
        strcat(htmlBuffer, "\"");
        strcat_P(htmlBuffer, htmlAnchorBlank);
        strcat(htmlBuffer, "\"");
        strcat_P(htmlBuffer, htmlAnchorHref);
        strcat(htmlBuffer, "\"");
        return String(htmlBuffer);
    }
    if (var == "AC")
        return String(htmlAnchorCenter);
    if (var == "/A")
        return String(htmlAnchorEnd);
    if (var == "/B")
        return String(htmlBodyEnd);
    if (var == "H")
        return String(htmlHeaderStart);
    if (var == "H1")
        return String(sdFilesH1);
    if (var == "/HB")
        return String(htmlHeaderEndBodyStart);
    if (var == "LI")
        return String(htmlListItemStart);
    if (var == "/LI")
        return String(htmlListItemEnd);
    if (var == "SD")
        return String(sdDirectory);
    if (var == "SZ") {
        sprintf (htmlBuffer, "%3.0f %s",
                 sdCardSizeMB < 1000. ? sdCardSizeMB : sdCardSizeMB / 1000.,
                 sdCardSizeMB < 1000. ? "MB" : "GB");
        return String(htmlBuffer);
    }
    if (var == "Q")
        return String("\"");
    if (var == "T")
        return String(htmlTitle);
    if (var == "UL")
        return String(htmlUlListStart);
    if (var == "/UL")
        return String(htmlUlListEnd);
    return String();
}

//------------------------------------------------------------------------------
// addFileName
//      Add a file name and file size to the SD card listing page
//
//  Inputs:
//      htmlPrint: Address of an HtmlPrint class object
//      buffer: Address of a buffer to receive the file link
//------------------------------------------------------------------------------
static
void
buildHtmlAnchor(HtmlPrint * htmlPrint, uint8_t *buffer) {
    uint64_t u64;
    uint32_t u32;

    // Build the HTML anchor
    strcpy((char *)buffer, "%LI%%A%%SD%");
    htmlPrint->setBufferAddress(buffer +strlen((char *)buffer));
    sdFile.printName(htmlPrint);
    strcat((char *)buffer, "\">");
    htmlPrint->setBufferAddress(buffer +strlen((char *)buffer));
    sdFile.printName(htmlPrint);
    strcat((char *)buffer, "%/A% (");

    //  Display the file size
    u64 = sdFile.fileSize();
    u32 = u64 / (1ull * 1000 * 1000 * 1000);
    if (u32)
        strcat((char *)buffer, String(u32).c_str());
    u32 = u64 % (1ull * 1000 * 1000 * 1000);
    strcat((char *)buffer, String(u32).c_str());
    strcat((char *)buffer, ")%/LI%");
}

//------------------------------------------------------------------------------
// cardListing
//      Start the listing of files on the SD card
//
//  Inputs:
//      buffer: Address of a buffer to receive the next portion of the HTML response
//      maxLen: Maximum length of the next portion of the HTML response
//
//  Returns:
//      The number of characters written to the response buffer
//------------------------------------------------------------------------------
static
int
cardListing (
    uint8_t * buffer,
    size_t maxLen
    )
{
    uint8_t * bufferStart;
    int bytesWritten;
    HtmlPrint * htmlPrint;
Serial.println ("cardListing called");
Serial.printf ("maxLen: %d\n", maxLen);

    // Redirect the output to the web page
    htmlPrint = new HtmlPrint();
    bufferStart = buffer;
    *buffer = 0;
    do {
        // Determine if the buffer can contain another file link
        if (maxLen <= NEXT_ENTRY_SIZE)
            // Not enough space
            break;

        // Add the next file name
        *buffer = 0;
        switch (state) {
        case LS_HEADER:
            // Add the header, start the body and add the heading
Serial.println("Adding header");
            strcpy_P((char *)buffer, sdHeader);
            state = LS_DISPLAY_FILES;
            break;

        case LS_DISPLAY_FILES:
            // Add the next file name
            sdFile = SdFile();
            if (!sdFile.openNext(&sdRootDir, O_RDONLY)) {
Serial.println("File open failed!");
                state = LS_TRAILER;
                if (!sdCardEmpty) {
Serial.println("Some files displayed");
                    // No more files, at least one file displayed
                    break;
                }

                // No files on the SD card
                strcpy_P ((char *)buffer, sdNoFiles);
                break;
            }

            // Start the list if necessary
            if (sdCardEmpty) {
                sdCardEmpty = 0;
                strcpy_P((char *)buffer, htmlUlListStart);
                buffer += strlen((char *)buffer);
            }

            // Add the anchor if another file exists
Serial.printf("Adding Anchor: ");
            buildHtmlAnchor (htmlPrint, buffer);
Serial.println((char *)buffer);

            // Close the file
            sdFile.close();
            break;

        case LS_TRAILER:
            // Finish the list if there are files listed
            if (!sdCardEmpty)
                strcat_P((char *)buffer, htmlUlListEnd);

            // Finish the page body
Serial.println("Adding trailer");
            strcat_P((char *)buffer, htmlBodyEnd);
            state = LS_DONE;
            break;
        }

        // Account for this page text
        bytesWritten = strlen((char *)buffer);
        buffer += bytesWritten;
        maxLen -= bytesWritten;

    // Determine if the listing is complete, end of the page reached
    } while ((maxLen > NEXT_ENTRY_SIZE) && (state != LS_DONE));

    // The listing is now complete.  Access to the SD card file system is no
    // longer necessary.  Close the root directory which was opened in
    // ListingPage below.
    bytesWritten = buffer - bufferStart;
Serial.printf ("bytesWritten: %d\n", bytesWritten);
    if (!bytesWritten)
{
Serial.println("Closing root directory");
        sdRootDir.close();
}

    // Done redirecting the output
    delete htmlPrint;

    // Return this portion of the page to the web server for transmission
Serial.printf ("cardListing returning: %d\n", bytesWritten);
    return bytesWritten;
}

//------------------------------------------------------------------------------
// listingPage
//      The requested URL matches the SD card listing page
//
//  Inputs:
//      request: Address of the AsyncWebServerRequest object
//------------------------------------------------------------------------------
void
SdCardServer::listingPage (
    AsyncWebServerRequest * request
    )
{
    AsyncWebServerResponse * response;

    if (!sdCardSize())
        // SD card not present
        request->send_P(200, "text/html", no_sd_card_html, processor);
    else {
        // Open the root directory.  This entry must remain open after this
        // function exits to allow the code above to access the SD card file
        // system and send more data as buffers become available in the web
        // server.
Serial.println("Opening root directory");
        sdCardEmpty = 1;
        sdRootDir = SdFile();
        if (!sdRootDir.openRoot(sdFat->vol())) {
            // Invalid SD card format
Serial.println("Invalid SD card format");
            request->send_P(200, "text/html", invalid_SD_card_format_html, processor);
        } else {
            state = LS_HEADER;
            response = request->beginChunkedResponse("text/html", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
                return cardListing(buffer, maxLen);
            }, processor);

            // Send the response
            if (server)
                response->addHeader("Server", server);
            request->send(response);
        }
    }
}

/*
void
buildHtmlAnchor(HtmlPrint * htmlPrint, uint8_t *buffer) {
    uint64_t u64;
    uint32_t u32;

    // Start the list if necessary
    if (sdCardEmpty) {
        strcpy_P((char *)buffer, htmlUlListStart);
        buffer += strlen((char *)buffer);
    }
    sdCardEmpty = 0;

    // Build the HTML anchor
    strcpy((char *)buffer, "%LI%%A%%SD%");
    htmlPrint->setBufferAddress(buffer +strlen((char *)buffer));
    sdFile.printName(htmlPrint);
    strcat((char *)buffer, "\">");
    htmlPrint->setBufferAddress(buffer +strlen((char *)buffer));
    sdFile.printName(htmlPrint);
    strcat((char *)buffer, "%/A% (");

    //  Display the file size
    u64 = sdFile.fileSize();
    u32 = u64 / (1ull * 1000 * 1000 * 1000);
    if (u32)
        strcat((char *)buffer, String(u32).c_str());
    u32 = u64 % (1ull * 1000 * 1000 * 1000);
    strcat((char *)buffer, String(u32).c_str());
    strcat((char *)buffer, ")%/LI%");
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int addFileName(HtmlPrint * htmlPrint, uint8_t * buffer) {
    uint8_t * bufferStart;

    bufferStart = buffer;
    switch (sdListingState) {
    case LS_HEADER:
        strcpy_P((char *)buffer, sdHeader);
        sdListingState = LS_DISPLAY_FILES;
        return strlen((char *)buffer);

    case LS_DISPLAY_FILES:
        sdFile = SdFile();
        if (!sdFile.openNext(&sdRootDir, O_RDONLY)) {
            sdListingState = LS_TRAILER;
            if (!sdCardEmpty)
                // No more files, at least one file displayed
                return addFileName(htmlPrint, buffer);

            // No files on the SD card
            strcpy_P ((char *)buffer, sdNoFiles);
            return strlen((char *)buffer);
        }

        // Add the anchor if another file exists
        buildHtmlAnchor (htmlPrint, buffer);

        // Close the file
        sdFile.close();
        return strlen((char *)buffer);

    case LS_TRAILER:
        if (!sdCardEmpty)
            strcat_P((char*)buffer, htmlUlListEnd);
        strcat_P((char *)buffer, htmlBodyEnd);
        sdListingState = LS_DONE;
        return strlen((char *)buffer);
    }

    // The listing is complete
    return 0;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int sdCardListing(uint8_t * buffer, size_t maxLen) {
    uint8_t * bufferStart;
    int bytesWritten;
    int dataBytes;
    HtmlPrint * htmlPrint;

    htmlPrint = new HtmlPrint();
    bufferStart = buffer;
    do {
        // Determine if the buffer is full enough
        bytesWritten = buffer - bufferStart;
        if (maxLen < MAX_FILE_NAME_SIZE) {
#ifdef  MARK_BOUNDARY
            if (bytesWritten) {
                Serial.println("Next buffer");
                strcat ((char *)buffer, "--------------------<br>\n");
                buffer += strlen((char *)buffer);
                bytesWritten = buffer - bufferStart;
            }
#endif  // MARK_BOUNDARY
            break;
        }

        // Add the next file name
        *buffer = 0;
        dataBytes = addFileName (htmlPrint, buffer);
        if (!dataBytes)
            break;
        buffer += dataBytes;
        maxLen -= dataBytes;
    } while (1);

    // Close the root directory when done
    if (!bytesWritten)
        sdRootDir.close();

    delete htmlPrint;
    return bytesWritten;
}
*/

//------------------------------------------------------------------------------
// sdCardSize
//      Get the SD card size in bytes
//
//  Inputs:
//      buffer: Address of a buffer to receive the next portion of the HTML response
//      maxLen: Maximum length of the next portion of the HTML response
//
//  Returns:
//      The number of characters written to the response buffer
//------------------------------------------------------------------------------
static
int
returnFile(
    uint8_t * buffer,
    size_t maxLen
    )
{
    int bytesToRead;
    int bytesRead;

    // Read data from the file
    bytesToRead = maxLen;
    bytesRead = sdFile.read(buffer, bytesToRead);

    // Don't return any more bytes on error
    if (bytesRead < 0)
        bytesRead = 0;

    // Close the file when done
    if (!bytesRead)
        sdFile.close();

    // Return the number of bytes read
    return bytesRead;
}

//------------------------------------------------------------------------------
// fileDownload
//      Download the specified file to the browser
//
//  Inputs:
//      request: Address of the AsyncWebServerRequest object
//      filename: Zero terminated string containing the filename to download
//
//  Returns:
//      One (1) if the file is available, zero (0) if no file is found
//------------------------------------------------------------------------------
static
int
fileDownload (
    AsyncWebServerRequest * request,
    const char * filename
    )
{
    uint64_t fileSize;

    //  Download the file if requested
    // Attempt to open the root directory
    sdRootDir = SdFile();
    if (!sdRootDir.openRoot(sdFat->vol())) {
        Serial.println("ERROR - Failed to open root directory!");
        return 0;
    }

    // Attempt to open the file
    if (!sdFile.open(&sdRootDir, filename, O_RDONLY)) {
        // File not found
        Serial.println("ERROR - File not found!");
        sdRootDir.close();
        return 0;
    }

    // Close the root directory
    sdRootDir.close();

    // Return the file
    AsyncWebServerResponse *response = request->beginChunkedResponse("application/octet-stream", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        return returnFile(buffer, maxLen);
    });
    fileSize = sdFile.fileSize();
    response->addHeader("Content-Length", String((int)fileSize));
    request->send(response);
    return 1;
}

//------------------------------------------------------------------------------
// sdCardSize
//      Get the SD card size in bytes
//
//  Returns:
//      Returns the size of the SD card in MB, zero (0) if the SD card is not
//      present.
//------------------------------------------------------------------------------
uint64_t
SdCardServer::sdCardSize(
    void
    )
{
    csd_t csd;

    // Verify that the SD card is present
    if (!sdCardPresent())
{
Serial.println("No card present!");
        return 0;
}

    // Get the SD card size
    sdFat->card()->readCSD(&csd); //Card Specific Data
Serial.printf("SD card present, %lld\n", sdCardCapacity(&csd));
    sdCardSizeMB = 0.000512 * sdCardCapacity(&csd);
    return sdCardSizeMB;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// Library API
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//------------------------------------------------------------------------------
// SdCardServer
//      Initialize an SdCardServer object
//------------------------------------------------------------------------------
SdCardServer::SdCardServer (
    SdFat * sd,
    SD_CARD_PRESENT sdCardPresent,
    const char * server,
    const char * url
    )
{
    // Remember the SdFat object that will be used to access the SD card
    sdFat = sd;
    this->sdCardPresent = sdCardPresent;

    // Remember the server name to be added as an HTML header
    this->server = server;

    // Save the base URL
    webPage = url;
    webPageLength = strlen(webPage);
    webPageMissingSlash = (webPage[webPageLength - 1] == '/') ? 0 : 1;
}

//------------------------------------------------------------------------------
// sdCardWebPage
//      Display the SD card web page if the requested URL matches the
//      URL passed to the SdCardServer initializer
//------------------------------------------------------------------------------
int
SdCardServer::sdCardWebPage(
    AsyncWebServerRequest * request
    )
{
    const char * filename;
    const char * url;

    // Get the URL
    url = (const char*)(request->url().c_str());

    // Determine if this is one of the SD card's web pages
    if (strncmp(url, webPage, webPageLength)
        || (webPageMissingSlash && (url[webPageLength] != '/')))

        // This request does not match any of the SD card's web pages
        return 0;

    // This is one of the SD card's web pages
    // Determine the filename
    filename = &url[webPageLength + webPageMissingSlash];
Serial.printf ("filename: %s\n", filename);

    //  Determine which SD card web page was requested
    if (filename[0])
        return fileDownload(request, filename);

    //  Display the listing page if requested
    listingPage(request);
    return 1;
}
