#include <WebServer.h>
#include <Update.h>
#include <ArduinoOTA.h>

WebServer server(80);

const char *upload_html = R"rawliteral(
<form method='POST' action='/update' enctype='multipart/form-data'>
  <input type='file' name='update'>
  <input type='submit' value='Update'>
</form>
)rawliteral";

// HTML to redirect after successful update
const char *update_success_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta http-equiv="refresh" content="5; url=/" />
</head>
<body>
  <h1>Update Successful! Rebooting...</h1>
  <p>You will be redirected to Home page in 5 seconds.</p>
</body>
</html>
)rawliteral";

void ota_prov()
{

    server.on("/", HTTP_GET, []()
              { server.send(200, "text/html", upload_html); });

    server.on("/update", HTTP_POST, []()
              {
                  server.send(200, "text/html", update_success_html); // Return the redirect HTML after a successful update
                  delay(1000);                                        // Time to display the message
                  ESP.restart();                                      // Restart the ESP32
              },
              []()
              {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin()) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.println("Update complete");
      } else {
        Update.printError(Serial);
      }
    } });

    ArduinoOTA.setPassword("admin"); // <- Match this password with the one in platform.ini

    ArduinoOTA.begin();

    server.begin();
}