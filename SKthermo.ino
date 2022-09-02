#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>

#define PIN_TEMP 4
#define PIN_RELAY 12
#define PIN_RESET 14
#define PIN_ENABLE 13

#define EEPROM_SIZE 4

OneWire oneWire(PIN_TEMP);
DallasTemperature sensors(&oneWire);
AsyncWebServer server(80);
DNSServer dns;
AsyncWiFiManager wm(&server, &dns);

bool isResetMode() {
  return !digitalRead(PIN_RESET);
}

bool isEnabled() {
  return !digitalRead(PIN_ENABLE);
}

#define TEMP_READ_PERIOD_MS 5 * 1000
uint32_t lastTempRead = 0;
float lastTemp = 0;
void updateTemp() {
  if (millis() - lastTempRead > TEMP_READ_PERIOD_MS) {
    sensors.requestTemperatures();
    lastTemp = sensors.getTempCByIndex(0);
    lastTempRead = millis();
  }
}

float targetTemp = 0;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta http-equiv="refresh" content="5">
    <title>SK thermo</title>
  </head>
  <body>
    <style>
      a.button {
        -webkit-appearance: button;
        -moz-appearance: button;
        appearance: button;
        text-decoration: none;
        color: initial;
      }  
    </style>
    <center>

    %ENABLED%
    
    <h2>
      <a href="/?target_down" class="button">-</a>
      &nbsp;
      %TMP_TARGET% °C
      &nbsp;
      <a href="/?target_up" class="button">+</a>
    </h2>
    
    Now: <b>%TMP_CURRENT%</b> °C
    <br/>

    <h3>%STATUS%</h3>
    </center>
  </body>
</html>)rawliteral";

String processor(const String& var)
{
  if (var == "ENABLED")
    return isEnabled() ? F("") : F("<b style='color:red'>DISABLED</b><br/><br/>");
  if (var == "TMP_TARGET")
    return String(targetTemp, 1);
  if (var == "TMP_CURRENT")
    return String(lastTemp, 1);
  if (var == "STATUS")
    return digitalRead(PIN_RELAY) ? F("<b style='color:orange'>Heating</b>") : F("Standby");
  return String();
}

void setup(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.hostname("SKthermo");

  Serial.begin(9600);

  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_RESET, INPUT_PULLUP);
  pinMode(PIN_ENABLE, INPUT_PULLUP);

  sensors.begin();

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0x00, targetTemp);
  if (targetTemp < 10)
    targetTemp = 15;

  if (isResetMode())
    wm.resetSettings();

  if (wm.autoConnect("SKthermo"))
    Serial.println("Connected");
  else
    Serial.println("Configportal running");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (request->hasParam("target_up")) {
      targetTemp += 0.5;
      if (targetTemp > 40)
        targetTemp = 40;
      EEPROM.put(0x00, targetTemp);
      EEPROM.commit();
      request->redirect("/");
    }
    else if (request->hasParam("target_down")) {
      targetTemp -= 0.5;
      if (targetTemp < 10)
        targetTemp = 10;
      EEPROM.put(0x00, targetTemp);
      EEPROM.commit();
      request->redirect("/");
    }
    else request->send_P(200, "text/html", index_html, processor);
  });

  server.begin();
}

bool relay = false;

void loop(void)
{
  updateTemp();

  if (!isEnabled())
    digitalWrite(PIN_RELAY, 0);
  else if (lastTemp > targetTemp + 0.2f)
    digitalWrite(PIN_RELAY, 0);
  else  if (lastTemp < targetTemp - 0.2f)
    digitalWrite(PIN_RELAY, 1);

  delay(500);
}
