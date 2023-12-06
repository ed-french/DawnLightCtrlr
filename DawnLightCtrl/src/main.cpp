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

#define TIME_FETCH_INTERVAL 3600000  // Hourly time syncs

#define PWM_FREQ 5000
#define PWM_RESOLUTION_BITS 10

#define MAXLIGHT 35

char g_alarm_time[6]; // to save the alarm time in readable form
uint32_t g_alarm_time_seconds=30000; // Safely after 8am
uint32_t g_prelight_start=g_alarm_time_seconds-20*60; // minutes of light before the alarm time
uint32_t g_postlight_end=g_alarm_time_seconds+20*60; // minutes of light after the alarm time

uint32_t g_millis_at_sync=0;
uint32_t g_seconds_last_sync=0;

bool g_override_light=false;
uint32_t g_override_light_off_ms=0;
uint32_t g_override_light_level=int(MAXLIGHT/2);


AsyncWebServer  server(80);

char * hostname="DawnLightCtrl";

char * time_server_url="http://192.168.1.125/gettime?from=dawnlightctrl";

uint32_t next_time_update_ms=millis();
uint32_t time_through_day_s=0;

uint8_t bank_pios[]={PIN_CHANNEL_0,PIN_CHANNEL_1,PIN_CHANNEL_2,PIN_CHANNEL_3};


uint16_t bank_levels[]={0,110,115,120,140,160,190,240,300,1023};

uint16_t g_light_level=0;





uint32_t get_now_seconds()
{
  return int((millis()-g_millis_at_sync)/1000)+g_seconds_last_sync;
}

bool is_digit(char c)
{
  return (c>='0' and c<='9');
}

struct good_seconds {
  bool success;
  uint32_t time;
};

good_seconds calc_seconds_from_time(String waketime)
{
  //returns a seconds since midnight and success values
  struct good_seconds result;
  result.success=false;
  result.time=0;
  if (waketime.length()!=5)
  {
    Serial.println("Waketime must be 5 digits");
    return result;
  }
  for (uint8_t i=0;i<5;i++)
  {
    if (i!=2 && !(is_digit(waketime.charAt(i))))
    {
      Serial.printf("Invalid character at position %d of waketime\n",i);
      return result;
    }
  }
  // At this point both pairs of digits must be valid numbers!
  uint8_t hours=waketime.substring(0,2).toInt();
  Serial.printf("Found hours value of : %d\n",hours);
  if (hours>23)
  {
    Serial.printf("Invalid number of hours: %d\n",hours);
    return result;
  }
  uint8_t mins=waketime.substring(3,5).toInt();
  Serial.printf("Found mins value of : %d\n",mins);
  if (mins>59)
  {
    Serial.printf("Invalid number of mins: %d\n",mins);
    return result;
  }

  // Now we have a good time, we can return it as a seconds since midnight

  result.success=true;
  result.time=hours*3600+60*mins;

  return result;
}


good_seconds seconds_from_time_sync(String raw)
{
  // raw will be like: 2023-12-06T11:26:08
  //                   01234567890123456789
  uint8_t hours=raw.substring(11,13).toInt();
  uint8_t mins=raw.substring(14,16).toInt();
  uint8_t seconds=raw.substring(17,19).toInt();
  Serial.printf("Found time as:\n\thours: %d\tmins: %d\tseconds: %d\n",hours,mins,seconds);
  struct good_seconds result;
  result.success=true;
  result.time=hours*3600+mins*60+seconds;
  return result;

}


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

    //Serial.printf("\tSet bank : %d\tlevel : %d\tRaw : %d\n",bank_no,level,bank_levels[level]);
  }
  //delay(100);

}




uint16_t get_required_light_level(uint32_t now)
{
  if (g_override_light)
  {
    if (millis()<g_override_light_off_ms)
    {
      Serial.println("Override light on");
      return g_override_light_level;
    } else {
      Serial.println("Override light now finished");
      g_override_light=false; // Cancel the override light
      // Conventional formula will return the required light level
    }
  }

  Serial.printf("now: %d\tprelight:%d\talarmtime:%d\tpostlight:%d\n",now,g_prelight_start,g_alarm_time_seconds,g_postlight_end);

  //uint32_t now=get_now_seconds();
  if (now<g_prelight_start)
  {
    Serial.println("Pre-prelight, so dark");
    return 0;
  }
  if (now>g_postlight_end)
  {
    Serial.println("Post-postlight, so dark");
    return 0;
  }

  if (now>g_alarm_time_seconds) return MAXLIGHT; // fully lit during postlight
  // During the prelight period, light is proportional...
  float proportion=float(now-g_prelight_start)/(g_alarm_time_seconds-g_prelight_start);
  Serial.printf("Proportion: %f\n",proportion);
  
  return int(proportion*MAXLIGHT+0.5);


}



void setup_server(void)
{
  

  server.on("/",HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "This is the dawn light controller.\n"
                        "\t* Use /set_level to temporarily set the light level\n"
                        "\t* Use /set_alarm?wake=06:30&prelight=20&postlight=20 to set the alarm time\n"
                        "\t* Use /now to get the seconds since midnight\n"
                        "\t* Use /test_time?seconds=30400 to see what the light level would be then\n"
                        "\t* Use /override?seconds=120&level=18"
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

  server.on("/override",HTTP_GET,[] (AsyncWebServerRequest *request)
  {
    uint32_t seconds=request->getParam("seconds")->value().toInt();
    uint32_t level=request->getParam("level")->value().toInt();
    g_override_light_off_ms=millis()+seconds*1000;
    g_override_light=true;
    g_override_light_level=level;
    Serial.printf("Override starting for %d seconds so ending at %d\n",seconds,g_override_light_off_ms);
    request->send(200, "text/plain", "OK");
  });

  server.on("/now",HTTP_GET, [] (AsyncWebServerRequest *request)
  {
    request->send(200,"text/plain",String(get_now_seconds()));
  });


  server.on("/test_time",HTTP_GET, [] (AsyncWebServerRequest *request)
  {
      // /test_time?seconds=30400
      uint32_t seconds=request->getParam("seconds")->value().toInt();
      String result=String(get_required_light_level(seconds));
      request->send(200,"text/plain",result);

  });
  

  server.on("/set_alarm",HTTP_GET, [] (AsyncWebServerRequest * request)
  {
    // e.g. /set_alarm?wake=06:30&prelight=20&postlight=20

    // First try and parse out the alarm time, which should always be present
    String response;

    if (!request->hasParam("wake"))
    {
      response=String("Missing a 'wake' url parameter, ignoring the request...");
      Serial.println(response);
      request->send(200,"text/plain",response);
      return;
    }

    // Does it make sense as an alarm time
    good_seconds result=calc_seconds_from_time(request->getParam("wake")->value());
    if (!result.success)
    {
      response=String("Invalid time, check logs for details.");
      Serial.println(response);
      request->send(200,"text/plain",response);
      return;
    }
    g_alarm_time_seconds=result.time;
    Serial.printf("New alarm time seconds: %d\n",g_alarm_time_seconds);
    strncpy(g_alarm_time,request->getParam("wake")->value().c_str(),5);


    response=String("New alarm time set to : "+request->getParam("wake")->value());
    Serial.println(response);
    request->send(200,"text/plain",response);

    if (request->hasParam("prelight"))
    {
      String prelight=request->getParam("prelight")->value();
      // Prelight should be a number of minutes
      uint32_t prelight_mins=prelight.toInt();
      if (prelight_mins==0 or prelight_mins>100)
      {
        String response=String("Prelight string must be in range 1-100 minutes");
        Serial.println(response);
        request->send(200,"text/plain",response);
      }
      Serial.printf("Prelight set to %d\n",prelight_mins);
      g_prelight_start=g_alarm_time_seconds-60*prelight_mins;
    }

    if (request->hasParam("postlight"))
    {
      String postlight=request->getParam("postlight")->value();
      // Prelight should be a number of minutes
      uint32_t postlight_mins=postlight.toInt();
      if (postlight_mins==0 or postlight_mins>100)
      {
        String response=String("Postlight string must be in range 1-100 minutes");
        Serial.println(response);
        request->send(200,"text/plain",response);
      }
      Serial.printf("Postlight set to %d\n",postlight_mins);
      g_postlight_end=g_alarm_time_seconds+60*postlight_mins;
    }

    request->send(200,"text/plain","OK");
  }
  
  
  );






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
        g_millis_at_sync=millis(); // Note the time of the last sync
        g_seconds_last_sync=seconds_from_time_sync(payload).time;
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
  g_light_level=get_required_light_level(get_now_seconds());
  set_light_level(g_light_level);
  delay(1000);
}
