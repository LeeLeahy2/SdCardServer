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

#define LINE_BUFFER_SIZE        1024    // Buffer to hold line across packets

typedef enum {
    LS_HEADER = 0,
    LS_DISPLAY_FILES,
    LS_TRAILER,
    LS_DONE
} LISTING_STATE;

//------------------------------------------------------------------------------
// HTML header pieces
//------------------------------------------------------------------------------

static prog_char htmlHeaderStart[] PROGMEM = "<!DOCTYPE HTML>\n<html lang=\"en\">\n<head>\n";

static prog_char htmlContentType[] PROGMEM = "  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/>\n";

static prog_char htmlRedirect[] PROGMEM = "  <meta http-equiv=\"refresh\" content=\"0; url='http://";

static prog_char htmlRedirectEnd[] PROGMEM = "'\" />\n";

static prog_char htmlTitle[] PROGMEM = R"rawliteral(  <title>)rawliteral";

static prog_char htmlTitleEnd[] PROGMEM = R"rawliteral(</title>
)rawliteral";

static prog_char htmlHeaderEnd[] PROGMEM = R"rawliteral(</head>
)rawliteral";

static prog_char htmlHeaderEndBodyStart[] PROGMEM = R"rawliteral(</head>
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

static prog_char sdHeader[] PROGMEM = R"rawliteral(%H%%CT%%T%%H1%%/T%%/HB%
  <h1>%H1%</h1>
)rawliteral";

static prog_char sdNoCard[] PROGMEM = R"rawliteral(
  <p>MicroSD card socket is empty!</p>
)rawliteral";

static prog_char sdNoFiles[] PROGMEM = R"rawliteral(
  <p>No files found!</p>
)rawliteral";

//------------------------------------------------------------------------------
// index.html
//------------------------------------------------------------------------------

static const char titleName[] PROGMEM = "SD Card Server";

static const char index_html[] PROGMEM = R"rawliteral(%H%%CT%%T%%Title%%/T%%/HB%
  <h1>%Title%</h1>
  <p>%A%%SD%%Q%>%H1%</a></p>
%/B%
)rawliteral";

static const char redirect_html[] PROGMEM = R"rawliteral(%H%%R%%IP%%SD%%R/%%/H%%/HTML%
)rawliteral";

static const char no_sd_card_html[] PROGMEM = R"rawliteral(%H%%CT%%T%%Title%%/T%%/HB%
  <h1>%Title%</h1>
  <p>ERROR - SD card not present!</a></p>
%/B%
)rawliteral";

static const char invalid_SD_card_format_html[] PROGMEM = R"rawliteral(%H%%CT%%T%%Title%%/T%%/HB%
  <h1>%Title%</h1>
  <p>ERROR - SD card has invalid format!</a></p>
%/B%
)rawliteral";

static const char memory_allocation_failed[] PROGMEM = R"rawliteral(%H%%CT%%T%%Title%%/T%%/HB%
  <h1>%Title%</h1>
  <p>ERROR - Memory allocation failed!</a></p>
%/B%
)rawliteral";

static const char not_implemented_html[] PROGMEM = R"rawliteral(%H%%CT%%T%%Title%%/T%%/HB%
  <h1>%Title%</h1>
  <p>ERROR - Not implemented!</a></p>
%/B%
)rawliteral";

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// Locals
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

static SD_CARD_PRESENT cardPresent;    // Routine to determine if SD card is present
static char htmlBuffer[256];           // Buffer for HTML token replacement
static char * lineBuffer;              // Temporary buffer to hold the next line
static char * lineBufferData;
static char * lineBufferDataEnd;
static int sdCardEmpty;                // No files found in the FAT file system
static float sdCardSizeMB;             // Size of the SD card in MB (1000 * 1000 bytes)
static SdFat * sdFat;                  // Address of a SdFat object
static SdFile * sdFile;                // File on the SD card
static SdFile * sdRootDir;             // Root directory file on the SD card
static const char * serverHdrText;     // Zero terminated string for web server name
static LISTING_STATE state;            // Listing state
static const char * webPage;           // Zero terminated string for SD card's web pages
static int webPageMissingSlash;        // Non zero if last character is a not a slash
static int webPageLength;              // Length of the webPage string

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
    if (var == "CT")
        return String(htmlContentType);
    if (var == "H")
        return String(htmlHeaderStart);
    if (var == "/H")
        return String(htmlHeaderEnd);
    if (var == "H1")
        return String(sdFilesH1);
    if (var == "/HB")
        return String(htmlHeaderEndBodyStart);
    if (var == "IP") {
        IPAddress ip = WiFi.localIP();
        sprintf (htmlBuffer, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        return String(htmlBuffer);
    }
    if (var == "LI")
        return String(htmlListItemStart);
    if (var == "/LI")
        return String(htmlListItemEnd);
    if (var == "SD")
        return String(webPage);
    if (var == "SZ") {
        sprintf (htmlBuffer, "%3.0f %s",
                 sdCardSizeMB < 1000. ? sdCardSizeMB : sdCardSizeMB / 1000.,
                 sdCardSizeMB < 1000. ? "MB" : "GB");
        return String(htmlBuffer);
    }
    if (var == "Q")
        return String("\"");
    if (var == "R")
        return String(htmlRedirect);
    if (var == "R/")
        return String(htmlRedirectEnd);
    if (var == "T")
        return String(htmlTitle);
    if (var == "Title")
        return String(titleName);
    if (var == "/T")
        return String(htmlTitleEnd);
    if (var == "UL")
        return String(htmlUlListStart);
    if (var == "/UL")
        return String(htmlUlListEnd);
    return String();
}

//------------------------------------------------------------------------------
// sdCardSize
//      Get the SD card size in bytes
//
//  Returns:
//      Returns the size of the SD card in bytes, zero (0) if the SD card is not
//      present.
//------------------------------------------------------------------------------
static
uint64_t
sdCardSize(
    void
    )
{
    csd_t csd;
    uint64_t sdCardBytes;

    // Verify that the SD card is present
    if (!cardPresent())
        return 0;

    // Get the SD card size
    sdFat->card()->readCSD(&csd); //Card Specific Data
    sdCardBytes = sd->card()->sectorCount() * 512;
    sdCardBytes <<= 9;
    sdCardSizeMB = 0.000001 * sdCardBytes;
    return sdCardBytes;
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
buildHtmlAnchor(HtmlPrint * htmlPrint, char * buffer) {
    uint64_t u64;
    uint32_t u32;

    // Start the list item
    strcpy(buffer, "%LI%");

    // Display the file date
    htmlPrint->setBufferAddress((uint8_t *)(buffer + strlen(buffer)));
    sdFile->printModifyDateTime(htmlPrint);
    strcat(buffer, ", ");

    // Build the HTML anchor
    strcat(buffer, "%A%%SD%");
    htmlPrint->setBufferAddress((uint8_t *)(buffer + strlen(buffer)));
    sdFile->printName(htmlPrint);
    strcat(buffer, "\">");
    htmlPrint->setBufferAddress((uint8_t *)(buffer + strlen(buffer)));
    sdFile->printName(htmlPrint);
    strcat(buffer, "%/A%, ");

    // Display the file size
    u64 = sdFile->fileSize();
    u32 = u64 / (1ull * 1000 * 1000 * 1000);
    if (u32)
        strcat(buffer, String(u32).c_str());
    u32 = u64 % (1ull * 1000 * 1000 * 1000);
    strcat(buffer, String(u32).c_str());
    strcat(buffer, " bytes%/LI%");
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
    int bytesWritten;
    HtmlPrint * htmlPrint;
    int length;

    bytesWritten = 0;
    if (maxLen && lineBuffer) {
        // Redirect the output to the web page
        htmlPrint = new HtmlPrint();
        *buffer = 0;
        do {
            // Determine if the previous buffer was too small for all of the data
            if (lineBufferData >= lineBufferDataEnd) {
                // Add the next file name
                lineBuffer[0] = 0;
                switch (state) {
                case LS_HEADER:
                    // Add the header, start the body and add the heading
                    strcpy_P(lineBuffer, sdHeader);
                    state = LS_DISPLAY_FILES;
                    break;

                case LS_DISPLAY_FILES:
                    // Add the next file name
                    sdFile = new SdFile();
                    if ((!sdRootDir) || (!sdFile) || (!sdFile->openNext(sdRootDir, O_RDONLY))) {
                        state = LS_TRAILER;
                        if (sdFile) {
                            delete sdFile;
                            sdFile = NULL;
                        }
                        if (!sdCardEmpty) {
                            // No more files, at least one file displayed
                            break;
                        }

                        // No files on the SD card
                        strcpy_P (lineBuffer, sdNoFiles);
                        break;
                    }

                    // Start the list if necessary
                    if (sdCardEmpty) {
                        sdCardEmpty = 0;
                        strcpy_P(lineBuffer, htmlUlListStart);
                    }

                    // Add the anchor if another file exists
                    buildHtmlAnchor (htmlPrint, &lineBuffer[strlen(lineBuffer)]);

                    // Close the file
                    sdFile->close();
                    delete sdFile;
                    sdFile = NULL;
                    break;

                case LS_TRAILER:
                    // Finish the list if there are files listed
                    if (!sdCardEmpty)
                        strcat_P(lineBuffer, htmlUlListEnd);

                    // Finish the page body
                    strcat_P(lineBuffer, htmlBodyEnd);
                    state = LS_DONE;
                    break;
                }

                // Set the temporary buffer length
                lineBufferData = lineBuffer;
                lineBufferDataEnd = &lineBuffer[strlen(lineBuffer)];
            }

            // Determine how much data will fit in the buffer
            length = lineBufferDataEnd - lineBufferData;
            if (length > maxLen)
                length = maxLen;

            // Move more data into the buffer
            memcpy(&buffer[bytesWritten], lineBufferData, length);
            lineBufferData += length;
            bytesWritten += length;

        // Determine if the listing is complete, end of the page reached
        } while (((maxLen - bytesWritten) > NEXT_ENTRY_SIZE) && (state != LS_DONE));

        // The listing is now complete.  Access to the SD card file system is no
        // longer necessary.  Close the root directory which was opened in
        // ListingPage below.
        if (!bytesWritten) {
            // Done with the root directory
            sdRootDir->close();
            delete sdRootDir;
            sdRootDir = NULL;

            // Done with the line buffer
            free(lineBuffer);
            lineBuffer = NULL;
        }

        // Done redirecting the output
        delete htmlPrint;
    }

    // Return this portion of the page to the web server for transmission
    return bytesWritten;
}

//------------------------------------------------------------------------------
// listingPage
//      The requested URL matches the SD card listing page
//
//  Inputs:
//      request: Address of the AsyncWebServerRequest object
//------------------------------------------------------------------------------
static
void
listingPage (
    AsyncWebServerRequest * request
    )
{
    AsyncWebServerResponse * response;

    if (!sdCardSize())
        // SD card not present
        request->send_P(200, "text/html", no_sd_card_html, processor);
    else {
        // Allocate a temporary buffer to hold data across packets.
        lineBuffer = (char *)malloc(LINE_BUFFER_SIZE);
        lineBufferData = lineBuffer;
        lineBufferDataEnd = lineBuffer;
        if (!lineBuffer) {
            // Invalid SD card format
            request->send_P(200, "text/html", memory_allocation_failed, processor);
        } else {
            // Open the root directory.  This entry must remain open after this
            // function exits to allow the code above to access the SD card file
            // system and send more data as buffers become available in the web
            // server.
            sdCardEmpty = 1;
            sdRootDir = new SdFile();
            if ((!sdRootDir) || (!sdRootDir->openRoot(sdFat->vol()))) {
                // Done with the root directory
                if (sdRootDir) {
                    delete sdRootDir;
                    sdRootDir = NULL;
                }

                // Done with the line buffer
                free(lineBuffer);
                lineBuffer = NULL;

                // Invalid SD card format
                request->send_P(200, "text/html", invalid_SD_card_format_html, processor);
            } else {
                state = LS_HEADER;
                response = request->beginChunkedResponse("text/html", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
                    return cardListing(buffer, maxLen);
                }, processor);

                // Send the response
                if (serverHdrText)
                    response->addHeader("Server", serverHdrText);
                request->send(response);
            }
        }
    }
}

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
    bytesRead = sdFile->read(buffer, bytesToRead);

    // Don't return any more bytes on error
    if (bytesRead < 0)
        bytesRead = 0;

    // Close the file when done
    if (!bytesRead) {
        sdFile->close();
        delete sdFile;
        sdFile = NULL;
    }

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

    // Download the file if requested
    // Attempt to open the root directory
    sdRootDir = new SdFile();
    if ((!sdRootDir) || (!sdRootDir->openRoot(sdFat->vol()))) {
        Serial.println("ERROR - Failed to open root directory!");
        return 0;
    }

    // Attempt to open the file
    sdFile = new SdFile();
    if ((!sdFile) || (!sdFile->open(sdRootDir, filename, O_RDONLY))) {
        // File not found
        Serial.println("ERROR - File not found!");
        if (sdFile) {
            delete sdFile;
            sdFile = NULL;
        }
        sdRootDir->close();
        delete sdRootDir;
        sdRootDir = NULL;
        return 0;
    }

    // Close the root directory
    sdRootDir->close();
    delete sdRootDir;
    sdRootDir = NULL;

    // Return the file
    AsyncWebServerResponse *response = request->beginChunkedResponse("application/octet-stream", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        return returnFile(buffer, maxLen);
    });
    fileSize = sdFile->fileSize();
    response->addHeader("Content-Length", String((int)fileSize));
    request->send(response);
    return 1;
}

//------------------------------------------------------------------------------
// indexPage
//      Main page for the SD card web site
//
//  Inputs:
//      request: Address of the AsyncWebServerRequest object
//------------------------------------------------------------------------------
static
void
indexPage (
    AsyncWebServerRequest * request
    )
{
    const char * page;

    // Determine which page to send
    page = sdCardSizeMB ? index_html : no_sd_card_html;

    // Send the response
    request->send_P(200, "text/html", page, processor);
}

//------------------------------------------------------------------------------
// isSdCardPage
//      Display the SD card listing web page if the requested URL matches
//      the URL passed to the SdCardServer constructor.  Start the SD card
//      file download if the requested URL starts with the URL passed to
//      the sdCardServer constructor and the file is found on the SD card.
//------------------------------------------------------------------------------
static
int
isSdCardPage(
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

    //  Determine which SD card web page was requested
    if (filename[0])
        return fileDownload(request, filename);

    //  Display the listing page if requested
    listingPage(request);
    return 1;
}

//------------------------------------------------------------------------------
// pageNotFound
//      Handle the page not found events.  Display the SD card listing or
//      download the specified SD card file.
//
//  Inputs:
//      request: Address of the AsyncWebServerRequest object
//------------------------------------------------------------------------------
static
void
pageNotFound (
    AsyncWebServerRequest * request
    )
{
    // Display the SD card page if necessary
    if (isSdCardPage(request))
        return;

    // URL not found
    request->send(404);
}

//------------------------------------------------------------------------------
// redirectPage
//      Main page for the SD card web site
//
//  Inputs:
//      request: Address of the AsyncWebServerRequest object
//------------------------------------------------------------------------------
static
void
redirectPage (
    AsyncWebServerRequest * request
    )
{
    const char * page;

    // Determine which page to send
    page = sdCardSizeMB ? redirect_html : no_sd_card_html;

    // Send the response
    request->send_P(200, "text/html", page, processor);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// Library API
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//------------------------------------------------------------------------------
// ~SdCardServer
//      SdCardServer object destructor
//------------------------------------------------------------------------------
SdCardServer::~SdCardServer (
    )
{
    if (server) {
        // Shutdown the SD card server website
        if (webSiteHandler)
            server->removeHandler(webSiteHandler);

        // Done with the server
        server = NULL;
    }
}

//------------------------------------------------------------------------------
// SdCardServer
//      Initialize an SdCardServer object
//------------------------------------------------------------------------------
SdCardServer::SdCardServer (
    SdFat * sd,
    SD_CARD_PRESENT sdCardPresent,
    const char * url,
    const char * serverHeaderText
    )
{
    // Remember the SdFat object that will be used to access the SD card
    sdFat = sd;
    cardPresent = sdCardPresent;

    // Remember the server name to be added as an HTML header
    serverHdrText = serverHeaderText;

    // Save the base URL
    webPage = url;
    webPageLength = strlen(webPage);
    webPageMissingSlash = (webPage[webPageLength - 1] == '/') ? 0 : 1;

    // No handlers are installed yet
    webSiteHandler = NULL;

    // Server not specified yet
    server = NULL;
}

//------------------------------------------------------------------------------
// isSdCardWebPage
//      Display the SD card listing web page if the requested URL matches
//      the URL passed to the SdCardServer constructor.  Start the SD card
//      file download if the requested URL starts with the URL passed to
//      the sdCardServer constructor and the file is found on the SD card.
//
//      This routine is designed to be called from the server.onNotFound
//      event routine.
//------------------------------------------------------------------------------
int
SdCardServer::isSdCardWebPage(
    AsyncWebServerRequest * request
    )
{
    return isSdCardPage(request);
}

//------------------------------------------------------------------------------
// sdCardListingWebPageLink
//      Add a link (HTML anchor) to an existing web page.  The link points
//      to the SD card listing web page.
//------------------------------------------------------------------------------
int
SdCardServer::sdCardListingWebPageLink(
    char * buffer,
    size_t maxLen,
    const char * linkText,
    const char * options
    )
{
    int initialLength;
    int sizeNeeded;

    // Determine the amount of space necessary to build the anchor link
    sizeNeeded = 2 + (options && *options ? ((*options != ' ') ? 1 : 0) : 0)
               + (options ? strlen(options) : 0) + 6 + strlen(webPage) + 2
               + strlen(linkText) + 4 + 1;

    // Add the link only if it fits
    initialLength = strlen(buffer);
    if (maxLen >= sizeNeeded) {
        strcat(buffer, "<a");
        if (options && *options) {
            if (*options != ' ')
                strcat(buffer, " ");
            strcat(buffer, options);
        }
        strcat(buffer, " href=\"");
        strcat(buffer, webPage);
        strcat(buffer, "\">");
        strcat(buffer, linkText);
        strcat(buffer, "</a>");
    }

    // Return the number of characters added to the buffer, NOT including the
    // zero termination!
    return strlen(buffer) - initialLength;
}

//------------------------------------------------------------------------------
// sdCardWebSite
//      Create a website for the SD card.  Call this routine if a website
//      does not already exist.  If a website already exists then use
//      sdCardListingWebPageLink to add a link (HTML anchor) to an existing
//      web page.
//------------------------------------------------------------------------------
void
SdCardServer::sdCardWebSite(
    AsyncWebServer * server,
    bool redirect
    )
{
    // Save the server address
    this->server = server;

    // Determine if the SD card is present
    sdCardSize();

    // Send the response
    if (redirect)
        webSiteHandler = &(server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            redirectPage (request);
        }));
    else
        webSiteHandler = &(server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            indexPage (request);
        }));
}

//------------------------------------------------------------------------------
// onNotFound
//      Add the request not found event.  This event handles the SD card
//      requests for listing and file download.
//
//      Call this routine if and only if the AsyncWebServer.onNotFound event
//      handler is not delared by the code calling SdCardServer.  The call
//      is made after the the AsyncWebServer is initialized.
//------------------------------------------------------------------------------
void
SdCardServer::onNotFound (
    AsyncWebServer * server
    )
{
    // Save the server address
    this->server = server;

    // Declare the page not found event handler
    server->onNotFound([](AsyncWebServerRequest *request) {
        pageNotFound(request);
    });
}
