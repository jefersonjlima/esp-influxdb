/*
    This project establishes a TCP connection to a influxdb database.
*/


#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include <DHT.h>

//#define DEBUG
#ifdef DEBUG
  #include <passwords.h>
#endif

#ifndef PASSWORDS_H
  const char* ssid     = "SSID";
  const char* password = "PASSWORD";
  const char* host =     "INFLUX_HOST";
  const uint16_t port = 8086;
#else
  const char* ssid     = STA_SSID;
  const char* password = STA_PASS;
  const char* host =     INFLUX_HOST;
  const uint16_t port = 8086;
#endif

// Define NTP Client to get time: utc-3 
const long utcOffsetInSeconds = -10800;
const char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                   "Thursday", "Friday", "Saturday"};
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds, 60000);

// Use WiFiClient class to create TCP connections
WiFiClient client;

Ticker timeTicker, sensorTicker;
boolean getTime();
boolean postInflux();

DHT dht;

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
#endif
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // waiting connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
#ifdef DEBUG
    Serial.print(".");
#endif
  }
#ifdef DEBUG
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
#endif
  // get server time 
  timeClient.begin();
  timeClient.update();
  timeTicker.attach_scheduled(3600, getTime);
  sensorTicker.attach_scheduled(1, postInflux);
  dht.setup(2);
}

void loop() {
}

boolean getTime(){
  timeClient.update();
#ifdef DEBUG
  Serial.println("Get time ...");
#endif
  return 0;
}

boolean postInflux(){

  unsigned long timeout = millis();
  while (!client.connect(host, port)) {
#ifdef DEBUG
  Serial.println("trying to connect");
#endif
  delay(100);
  if (millis() - timeout > 500) {
#ifdef DEBUG
      Serial.println(">>> Client is not available!");
#endif
      return 1;
    }
  }
#ifdef DEBUG
  Serial.println();
  Serial.print("connected to ");
  Serial.print(host);
  Serial.print(':');
  Serial.print(port);
  Serial.println(" at " + timeClient.getFormattedDate());
#endif

  //Database connection
  uint8_t humidity = dht.getHumidity();
  uint8_t temperature = dht.getTemperature();
  String epoch = String(timeClient.getEpochTime())  + "000000000";
  // This will send a string to the server
  String post_qry = "/write?db=sensors&precision=ns";
  String body = "weather,location=pato-branco temperature=" +
                String(temperature) + "," + "humidity=" + String(humidity) + " " + epoch;

  uint8_t Len = body.length();
  
  if (client.connected()) {
    client.print(String("POST ") + post_qry + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: EspSensor\r\n" +
               "Content-Type: text/plain\r\n" + 
               "Connection: close\r\n" +
               "Content-Length: " + String(Len) + "\r\n\r\n" +   
               body + "\r\n\r\n");
  }

  // wait for data to be available
  while (client.available() == 0) {
    if (millis() - timeout > 1000) {
#ifdef DEBUG
      Serial.println(">>> Client Timeout !");
#endif
      client.stop();
      break;
    }
  }
  // Read all the lines of the reply from server and print them to Serial
  while (client.connected() || client.available())
  {
    if (client.available()){
      String line = client.readStringUntil('\n');
#ifdef DEBUG
      Serial.println(line);
#endif
    }
  }

  // Close the connection
#ifdef DEBUG
  Serial.println("closing connection");
  client.stop();
#endif
  // delay(1000);
  return 0;
}