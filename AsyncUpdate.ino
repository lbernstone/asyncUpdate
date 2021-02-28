#include <ESPAsyncWebServer.h>
#ifdef ESP8266
#include <Updater.h>
#include <ESP8266mDNS.h>
#define U_PART U_FS
#else
#include <Update.h>
#include <ESPmDNS.h>
#define U_PART U_SPIFFS
#endif

#define MYSSID "ssid"
#define PASSWD "passwd"

AsyncWebServer server(80);
size_t content_len;

boolean wifiConnect(char* host) {
#ifdef MYSSID
  WiFi.begin(MYSSID,PASSWD);
  WiFi.waitForConnectResult();
#else
  WiFi.begin();
  WiFi.waitForConnectResult();
#endif
  Serial.println(WiFi.localIP());
  return (WiFi.status() == WL_CONNECTED);
}

void handleRoot(AsyncWebServerRequest *request) {
  request->redirect("/update");
}

void handleUpdate(AsyncWebServerRequest *request) {
  char* html = "<form method='POST' action='/doUpdate' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  request->send(200, "text/html", html);
}

void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index){
    Serial.println("Update");
    content_len = request->contentLength();
    // if filename includes spiffs, update the spiffs partition
    int cmd = (filename.indexOf("spiffs") > -1) ? U_PART : U_FLASH;
#ifdef ESP8266
    Update.runAsync(true);
    if (!Update.begin(content_len, cmd)) {
#else
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
#endif
      Update.printError(Serial);
    }
    request->send(200, "text/html", "<head><meta http-equiv='refresh' content='10;URL=/'/></head><body>Upload complete! Please wait while the device reboots</body>");
  }

  if (Update.write(data, len) != len) {
    Update.printError(Serial);
#ifdef ESP8266
  } else {
    Serial.printf("Progress: %d%%\n", (Update.progress()*100)/Update.size());
#endif
  }

  if (final) {
    if (!Update.end(true)){
      Update.printError(Serial);
    } else {
      Serial.println("Update complete");
      Serial.flush();
      ESP.restart();
    }
  }
}

void printProgress(size_t prg, size_t sz) {
  Serial.printf("Progress: %d%%\n", (prg*100)/content_len);
}

boolean webInit() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {request->redirect("/update");});
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){handleUpdate(request);});
  server.on("/doUpdate", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {handleDoUpdate(request, filename, index, data, len, final);}
  );
  server.onNotFound([](AsyncWebServerRequest *request){request->send(404);});
  server.begin();
#ifdef ESP32
  Update.onProgress(printProgress);
#endif
}

void setup() {
  Serial.begin(115200);
  char host[16];
#ifdef ESP8266
  snprintf(host, 12, "ESP%08X", ESP.getChipId());
#else
  snprintf(host, 16, "ESP%012llX", ESP.getEfuseMac());
#endif
  if(!wifiConnect(host)) {
    Serial.println("Connection failed");
    return;
  }
  MDNS.begin(host);
  webInit();
  MDNS.addService("http", "tcp", 80);
  Serial.printf("Ready! Open http://%s.local in your browser\n", host);
}

void loop() {delay(0xFFFFFFFF);}
