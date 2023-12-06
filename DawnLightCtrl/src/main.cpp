#include <Arduino.h>

#include <WiFi.h>
#include <HTTPClient.h> // To fetch the time and alarm time as required

#include "credentials.h"
#include "pins.h"

#include <AsyncTCP.h>


// #define ELEGANTOTA_USE_ASYNC_WEBSERVER 1

#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>

#define WIFI_AT_BOOT_TIMEOUT_MS 45000

#define TIME_FETCH_INTERVAL 10000

#define PWM_FREQ 5000
#define PWM_RESOLUTION_BITS 10


AsyncWebServer  server(80);

char * hostname="DawnLightCtrl";

char * time_server_url="http://192.168.1.125/gettime?from=dawnlightctrl";

uint32_t next_time_update_ms=millis();
uint32_t time_through_day_s=0;

uint8_t bank_pios[]={PIN_CHANNEL_0,PIN_CHANNEL_1,PIN_CHANNEL_2,PIN_CHANNEL_3};


uint16_t bank_levels[]={0,110,115,120,140,160,190,240,300,1023};

uint16_t g_light_level=0;

void set_light_level(uint16_t value)
{
  // Value is 0-35 to set the light level output
  if (value>35)
  {
    Serial.println("Attempt to set light level beyond the maximum");
    return;
  }
  g_light_level=value;
  uint8_t max_bank=value/9; // 0-3 for the highest bank to activate
  uint16_t level=0;
  for (uint8_t bank_no=0;bank_no<4;bank_no++)
  {
    if (value==0)
    {
      level=0;
    } else {
      level=1+(value%9);
    }
    if (bank_no>max_bank)
    {
      level=0;
    }
    ledcWrite(bank_no,bank_levels[level]);

    Serial.printf("\tSet bank : %d\tlevel : %d\tRaw : %d\n",bank_no,level,bank_levels[level]);
  }
  delay(100);

}

void setup_server(void)
{
  

  server.on("/",HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "This is the dawn light controller.\n"
                        "\t* Use /set_level to temporarily set the light level\n"
                        "\tUse /update to install new firmware remotely (creds wificlassic)\n");
  });

  server.on("/set_level",HTTP_GET, [](AsyncWebServerRequest *request) {
        String raw_level;
        String response;
        if (!request->hasParam("level"))
        {
            response="Ignoring malformed set_level request, needs a level param";
            Serial.println(response);
            request->send(200,response);
            return;
        }
        raw_level = request->getParam("level")->value();
        // try to conver to a number

        g_light_level=raw_level.toInt();
        response="light level set to "+String(g_light_level);

        request->send(200, "text/plain", response);
        Serial.println(response);
  });






  ElegantOTA.begin(&server);    // Start AsyncElegantOTA
  ElegantOTA.setAuth(ota_username,ota_password);
  server.begin();
  Serial.println("HTTP server started");

  

}

void update_time_through_day()
{

  // Check the wifi is still up
  if (!WiFi.status()== WL_CONNECTED)
  {
    esp_restart();
    return; // never happens but gotta keep the compiler happy
  }

  // Just return if no update is due
  if (millis()<next_time_update_ms)
  {
    return;
  }
  


  HTTPClient http;


      
  // Your Domain name with URL path or IP address with path
  http.begin(time_server_url);
    
  int httpResponseCode = http.GET();
      
  if (httpResponseCode>0)
  {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
  } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();



  next_time_update_ms=millis()+TIME_FETCH_INTERVAL;

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
void setup_pwm_channels()
{
  for (uint8_t i=0;i<4;i++)
  {
    ledcSetup(i,PWM_FREQ,PWM_RESOLUTION_BITS);
    ledcAttachPin(bank_pios[i],i);
    ledcWrite(i,0);
  }
}

void setup() {

  startup_countdown(3);

  
  connect_wifi();
  setup_server();
  
  setup_pwm_channels();



}

void loop() {
  // put your main code here, to run repeatedly:
  update_time_through_day(); // Does nothing if not yet due
  set_light_level(g_light_level);
  delay(10);
}
