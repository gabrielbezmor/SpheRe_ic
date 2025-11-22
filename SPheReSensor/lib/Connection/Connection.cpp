#include "Connection.h"

// Based on:
// https://iotdesignpro.com/articles/esp32-data-logging-to-google-sheets-with-google-scripts
// https://electropeak.com/learn/sending-data-from-esp32-or-esp8266-to-google-sheets-2-methods/
// https://how2electronics.com/how-to-send-esp32-cam-captured-image-to-google-drive/#google_vignette
// https://www.niraltek.com/blog/how-to-take-photos-and-upload-it-to-google-drive-using-esp32-cam/
// https://www.makerhero.com/blog/tire-fotos-com-esp32-cam-e-armazene-no-google-drive/

ConnectionHandler Connection;




bool ConnectionHandler::setup()
{
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println("Waiting connection to WiFi..");
  unsigned long begin = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    printf(" failed (status=%d)\n", WiFi.status());
    if ((millis() - begin) > timeout)
    {
      failure_count++;
      return connection_status = false;
    }
  }
  Serial.println("Connected!");
  return connection_status = true;
}

static String urlenc(const String& s){
  String o; const char* h="0123456789ABCDEF";
  for (char c: s){ if (isalnum((unsigned char)c)||c=='-'||c=='_'||c=='.'||c=='~') o+=c;
    else { o+='%'; o+=h[(c>>4)&15]; o+=h[c&15]; } }
  return o;
}

// https://script.google.com/macros/s/AKfycbxOGKDgw1GcC6rqENq3CvNopTFDyFMk2jO_zbDYlYmUhw4Y-7ell3xzcCS_2aS0D3qP/exec

void ConnectionHandler::logEvent(const String& stage, int code, int bytes, float moisture, const String& msg){
  String url = "https://script.google.com/macros/s/" + LOGS_WEB_APP_ID + "/exec";
  url += "?device="   + urlenc(DEVICE_NAME);
  url += "&stage="    + urlenc(stage);
  url += "&code="     + String(code);
  url += "&bytes="    + String(bytes);
  url += "&moisture=" + String(moisture,1);
  url += "&msg="      + urlenc(msg);
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); http.setTimeout(20000);
  if (http.begin(client, url)) { http.GET(); http.end(); }
}

void ConnectionHandler::sendData(float moisture, bool valve_state) {
  Serial.println("Sending data to Google Sheets.");

  const String string_humidity   = String(moisture, 1);
  const String string_valve_state = valve_state ? "100" : "0";
  const String url =
      "https://script.google.com/macros/s/" + GOOGLE_SHEETS_SCRIPT_ID +
      "/exec?moisture=" + string_humidity + "&valvestate=" + string_valve_state;

  Serial.print("[Sheets URL] ");
  Serial.println(url);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setReuse(true);
  http.setTimeout(60000);

  if (!http.begin(client, url)) {
    Serial.println("HTTP begin() failed (Sheets).");
    failure_count++;
    return;
  }

  const int httpCode = http.GET();
  const String body  = http.getString();
  Serial.printf("[Sheets] HTTP %d\n", httpCode);
  if (httpCode != 200) {
    Serial.println("[Sheets] Body:");
    Serial.println(body);
    failure_count++;
  }
  http.end();
}

void ConnectionHandler::sendImage(float moisture, camera_fb_t *fb) {
  Serial.println("Sending image to Google Drive.");

  const String string_humidity = String(moisture, 1);
  const String url =
      "https://script.google.com/macros/s/" + GOOGLE_DRIVE_SCRIPT_ID +
      "/exec?moisture=" + string_humidity;

  Serial.print("[Drive URL] ");
  Serial.println(url);

  // Verificação básica do JPEG
  if (!fb || fb->len < 64 || fb->buf[0] != 0xFF || fb->buf[1] != 0xD8) {
    Serial.println("Error: invalid/corrupt JPEG.");
    if (fb) esp_camera_fb_return(fb);
    failure_count++;
    return;
  }

  const int maxRetries = 5;
  for (int attempt = 1; attempt <= maxRetries; ++attempt) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(60);

    HTTPClient http;
    // http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // << importante
    http.setReuse(false);
    http.setTimeout(60000);

    if (!http.begin(client, url)) {
      Serial.println("HTTP begin() failed (Drive).");
      failure_count++;
    } else {
      http.addHeader("Content-Type", "image/jpeg");
      const int httpCode = http.POST(fb->buf, fb->len);
      const String body  = http.getString();
      Serial.printf("[Drive] Attempt %d -> HTTP %d\n", attempt, httpCode);

      if (httpCode > 0 && httpCode >= 200 && httpCode < 350) {
        Serial.println("[Drive] Image sent successfully.");
        http.end();
        esp_camera_fb_return(fb);
        return;
      }
      Serial.println("[Drive] Error Body:");
      Serial.println(body);
      http.end();
    }

    // backoff simples
    delay(10000 * attempt);
  }

  Serial.println("[Drive] Critical failure: all attempts failed.");
  esp_camera_fb_return(fb);
  failure_count++;
}


String ConnectionHandler::receiveData() {
  Serial.println("Read data from Google Sheets.");

  const String url =
      "https://script.google.com/macros/s/" + GOOGLE_SHEETS_SCRIPT_ID + "/exec?read=1";

  Serial.print("[Sheets READ URL] ");
  Serial.println(url);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(60000);

  if (!http.begin(client, url)) {
    Serial.println("HTTP begin() failed (Sheets read).");
    failure_count++;
    return "nowater,00,00";
  }

  const int httpCode = http.GET();
  const String payload = http.getString();
  Serial.printf("[Sheets READ] HTTP %d\n", httpCode);
  Serial.println("Payload: " + payload);

  http.end();
  if (httpCode == 200 && payload.length() > 0) return payload;

  failure_count++;
  return "nowater,00,00";
}
//teste:
void ConnectionHandler::sendImageBuffer(float moisture, const uint8_t* data, size_t len) {
  Serial.println("Sending image (TEST buffer) to Google Drive.");

  const String url =
      "https://script.google.com/macros/s/" + GOOGLE_DRIVE_SCRIPT_ID +
      "/exec?moisture=" + String(moisture, 1);

  Serial.print("[Drive URL] ");
  Serial.println(url);

  // Checagem mínima
  if (!data || len < 64 || data[0] != 0xFF || data[1] != 0xD8) {
    Serial.println("Error: invalid/corrupt JPEG buffer (len < 64 ou sem 0xFFD8).");
    failure_count++;
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(60);

  HTTPClient http;
  // http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setReuse(false);
  http.setTimeout(60000);

  if (!http.begin(client, url)) {
    Serial.println("HTTP begin() failed (Drive TEST).");
    failure_count++;
    return;
  }

  http.addHeader("Content-Type", "text/plain");
  int httpCode = http.POST("");
  String body = http.getString();
  Serial.printf("[Drive TEST] HTTP %d\n", httpCode);
  Serial.println(body);
  http.end();

  if (httpCode != 200) failure_count++;
}

void ConnectionHandler::close()
{
  WiFi.disconnect();
}
