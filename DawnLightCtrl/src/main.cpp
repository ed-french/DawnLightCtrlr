#include <Arduino.h>
#include <AsyncTCP.h>
#include <WiFi.h>
#include <HTTPClient.h> // To fetch the time and alarm time as required

#include "credentials.h"
#include "pins.h"

// #include <AsyncTCP.h>


// // #define ELEGANTOTA_USE_ASYNC_WEBSERVER 1

#include <ESPAsyncWebServer.h>
// #include <ElegantOTA.h>

#include "light_control.h"

#define WIFI_AT_BOOT_TIMEOUT_MS 45000

#define TIME_FETCH_INTERVAL 3600000  // Hourly time syncs

#define PWM_FREQ 5000
#define PWM_RESOLUTION_BITS 10

// #define MAXLIGHT 35

// char g_alarm_time[6]; // to save the alarm time in readable form
// uint32_t g_alarm_time_seconds=30000; // Safely after 8am
// uint32_t g_prelight_start=g_alarm_time_seconds-20*60; // minutes of light before the alarm time
// uint32_t g_postlight_end=g_alarm_time_seconds+20*60; // minutes of light after the alarm time

// uint32_t g_millis_at_sync=0;
// uint32_t g_seconds_last_sync=0;

// bool g_override_light=false;
// uint32_t g_override_light_off_ms=0;
// uint32_t g_override_light_level=int(MAXLIGHT/2);

static LightControl light(PWM_RESOLUTION_BITS, \
                          PWM_FREQ, \
                          PIN_CHANNEL_0, \
                          PIN_CHANNEL_1, \
                          PIN_CHANNEL_2, \
                          PIN_CHANNEL_3);


static AsyncWebServer server(80);




static const char *htmlContent  = R"(
<!DOCTYPE html>
<html>
<body>
    <h1>Flic dawn light level controller</h1>
    <pre>
      Use /set_level?level=0.nnnn to set the light level
    </pre>
</body>
</html>
)";

static const size_t htmlContentLength = strlen_P(htmlContent);



const char * hostname="DawnLightingctrl";

const char * time_server_url="http://192.168.1.125/gettime?from=dawnlightctrl";

uint32_t next_time_update_ms=millis();
uint32_t time_through_day_s=0;

uint8_t bank_pios[]={PIN_CHANNEL_0,PIN_CHANNEL_1,PIN_CHANNEL_2,PIN_CHANNEL_3};


//uint16_t bank_levels[]={0,110,115,120,140,160,190,240,300,1023};
uint16_t bank_levels[]=  {0,112,116,124,132,150,210,350,600,1023};

uint16_t g_light_level=0;


uint32_t last_light_set_ms=0;




void setup_server(void)
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", htmlContent);
  });

server.on("/set_level", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("level")) {
      last_light_set_ms=millis();
      Serial.printf("Level? %s\n", request->getParam("level")->value().c_str());
      // Convert the level, which is a float from 0-1 to a float
      float raw_level = request->getParam("level")->value().toFloat();
      if (raw_level < 0.0 || raw_level > 1.0) {
        Serial.println("Ignoring out of range level");
        request->send(405, "text/plain", "Ignoring out of range level");
        return;
      }
      Serial.printf("Setting light level to %f\n", raw_level);
      // Set the light level
      light.setLightLevel(raw_level);
    }

    request->send(200, "text/plain", "OK");
  });


  server.begin();

}





void connect_wifi()
{


  
  WiFi.setHostname(hostname);

  /*                    

  ################################################################                                      
  
      JUST FOR THE 1.0.0 ESP32C3 Mini Lolin boards due to RF error
  
  
  */
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // May be required to connect

   /*                    

  ################################################################                                      
 
  
  */



  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  delay(1000);
   /*                    

  ################################################################                                      
  
      JUST FOR THE 1.0.0 ESP32C3 Mini Lolin boards due to RF error
  
  
  */
  // WiFi.setTxPower(WIFI_POWER_8_5dBm); // May be required to connect

   /*                    

  ################################################################                                      
 
  
  */
  WiFi.begin(ssid, password);
  delay(1000);
  Serial.printf("Connecting to: %s\n",ssid);
  Serial.println("");

  uint32_t wifi_timeout=millis()+WIFI_AT_BOOT_TIMEOUT_MS;

  // Wait for connection
  while (true)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\nWifi now connected");
      break;
    }
    if (millis()>wifi_timeout)
    {
      Serial.println("Pointless message saying we are restarting to have another go at connecting");
      esp_restart();
    }
    
    delay(200);
    Serial.print(".");
  }

  
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}

void startup_countdown(uint8_t seconds)
{
  Serial.begin(115200);
  for (uint8_t i=seconds;i>0;i--)
  {
    Serial.println(i);
    delay(1000);
  }
}
// void setup_pwm_channels()
// {
//   for (uint8_t i=0;i<4;i++)
//   {
//     pinMode(bank_pios[i],OUTPUT);
//     // ledcSetup(bank_pios[i],PWM_FREQ,PWM_RESOLUTION_BITS);
//     ledcSetup(i,PWM_FREQ,PWM_RESOLUTION_BITS);
//     // ledcAttach(bank_pios[i],PWM_FREQ,PWM_RESOLUTION_BITS);
//     ledcWrite(i,0);
//   }
// }

void setup() {

  startup_countdown(3);
  light.begin();
  connect_wifi();
  setup_server();
  
  // setup_pwm_channels();



}

void loop() {
  // while (true)
  // {
  //   check_key(&parse_light_level_serial_input);
  //   delay(200);
  //   Serial.print(".");
  // }
  // // update_time_through_day(); // Does nothing if not yet due
  // // g_light_level=get_required_light_level(get_now_seconds());
  // set_light_level(g_light_level);

  // Async will set up the 


  delay(1000);
  uint32_t now_ms=millis();
  if (now_ms>last_light_set_ms+30000)
  {
    Serial.println("No light level set command received in last 60 seconds, setting light level to 0");
    light.setLightLevel(0.0);
    last_light_set_ms=now_ms;
  }
}
