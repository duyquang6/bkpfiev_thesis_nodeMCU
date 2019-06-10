#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include "DHT.h"

// WiFi Parameters
#define ssid "1 + 1 = password"
#define password "oneplusone"
WiFiClient espClient;
String node_ip;
long lastMsg = 0;
long now = 0;
long then = 0;
// MQTT Parameters
#define mqtt_server "192.168.43.153"
#define mqtt_topic_pub "data"
#define mqtt_topic_sub "data"
#define mqtt_user "esp8266"
#define mqtt_pwd "blank"
const uint16_t mqtt_port = 1883 ; //Port của CloudMQTT
PubSubClient client(espClient);
WiFiServer server(80);
// GPS Sensors Parameters
SoftwareSerial ss(4, 5); // The serial connection to the GPS device
int year , month , date, hour , minute , second;
String date_str , time_str , lat_str , lng_str;
TinyGPSPlus gps;
float latitude , longitude;
int pm;

// DHT Sensor Parameters
#define DHTPIN D4      // Chân DATA nối với chân D4
#define DHTTYPE DHT11   // DHT 11
DHT dht(DHTPIN, DHTTYPE);
float h, t;

// Pump value
uint8_t pump1_pin = D5;
boolean pump1_status = false;
uint8_t pump2_pin = D6;
boolean pump2_status = false;

// Initiate sensor values
int sensor = 1;
int sensorPin = 13;

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  server.begin();
  Serial.println("Server started");
  client.setCallback(callback);
  randomSeed(100);
  pinMode(pump1_pin, OUTPUT);
  pinMode(pump2_pin, OUTPUT);
}

/**
   Setting up WiFi connection
*/
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  //Serial.println(WiFi.localIP());
  node_ip = ipToString(WiFi.localIP());
  Serial.print(node_ip);
  Serial.print("\n");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("On message");

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';
  char pump1_on[] = "pump1_on";
  char pump1_off[] = "pump1_off";
  char pump2_on[] = "pump2_on";
  char pump2_off[] = "pump2_on";
  // Check pump 1
  if (strcmp(json, pump1_off) == 0) {
    digitalWrite(pump1_pin, LOW);
    pump1_status = false;
  }
  if (strcmp(json, pump1_on) == 0) {
    digitalWrite(pump1_pin, HIGH);
    pump1_status = true;
  }
  // Check pump 2
  if (strcmp(json, pump2_off) == 0) {
    digitalWrite(pump2_pin, LOW);
    pump2_status = false;
  }
  if (strcmp(json, pump2_on) == 0) {
    digitalWrite(pump2_pin, HIGH);
    pump2_status = true;
  }
}

String ipToString(IPAddress ip) {
  String s = "";
  for (int i = 0; i < 4; i++) {
    s += i ? "x" + String(ip[i]) : String(ip[i]);
  }
  return s;
}


/**
   Reconnect to MQTT Broker when connection is down
*/
void reconnect() {
  // Chờ tới khi kết nối
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Thực hiện kết nối với mqtt user và pass
    if (client.connect("ESP8266Client", mqtt_user, mqtt_pwd)) {
      Serial.println("connected");
      // Khi kết nối sẽ publish thông báo
      //client.publish(mqtt_topic_pub, "ESP_reconnected");
      // ... và nhận lại thông tin này
      client.subscribe("control");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Đợi 5s
      delay(5000);
    }
  }
}


void loop() {
  client.loop();
  now = millis();
  // ---------------------------------------
  // GPS Sensor
  // ---------------------------------------
  while (ss.available() > 0) {
    if (gps.encode(ss.read()))
    {
      if (gps.location.isValid())
      {
        latitude = gps.location.lat();
        lat_str = String(latitude , 6);
        longitude = gps.location.lng();
        lng_str = String(longitude , 6);
      } else {
      }
      if (gps.date.isValid())
      {
        date_str = "";
        date = gps.date.day();
        month = gps.date.month();
        year = gps.date.year();
        if (date < 10)
          date_str = '0';
        date_str += String(date);
        date_str += " / ";
        if (month < 10)
          date_str += '0';
        date_str += String(month);
        date_str += " / ";
        if (year < 10)
          date_str += '0';
        date_str += String(year);
      }
      if (gps.time.isValid())
      {
        time_str = "";
        hour = gps.time.hour();
        minute = gps.time.minute();
        second = gps.time.second();
        minute = (minute + 30);
        if (minute > 59)
        {
          minute = minute - 60;
          hour = hour + 1;
        }
        hour = (hour + 5) ;
        if (hour > 23)
          hour = hour - 24;
        if (hour >= 12)
          pm = 1;
        else
          pm = 0;
        hour = hour % 12;
        if (hour < 10)
          time_str = '0';
        time_str += String(hour);
        time_str += " : ";
        if (minute < 10)
          time_str += '0';
        time_str += String(minute);
        time_str += " : ";
        if (second < 10)
          time_str += '0';
        time_str += String(second);
        if (pm == 1)
          time_str += " PM ";
        else
          time_str += " AM ";
      }
    }
  }
  // ---------------------------------------
  // GPS Sensor
  // ---------------------------------------
  h = dht.readHumidity();
  t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
  }

  if (!client.connected()) {
    reconnect();
  }
  //  client.loop();
  if (now - lastMsg > 30000) {
    DynamicJsonDocument doc(1024);
    // ---------------------------------------
    // Data content
    // ---------------------------------------
    doc["nodeID"] = "test";
    doc["temperature"]   = isnan(t) ? 0 : t;
    doc["humidity"] = isnan(h) ? 0 : h;
    doc["lat"] = lat_str;
    doc["lng"] =  lng_str;
    doc["ripeness"] = 10;
    doc["pump1"] = pump1_status;
    doc["pump2"] = pump2_status;
    // ---------------------------------------
    // End data content
    // ---------------------------------------
    String pubString;
    serializeJson(doc, pubString);
    // Convert to CharArray
    unsigned int len =  pubString.length() + 1;
    char data[len];
    pubString.toCharArray(data, len);
    Serial.println(pubString);
    client.publish(mqtt_topic_pub, data);
    lastMsg = now;
  }

  WiFiClient httpClient = server.available();
  if (!httpClient)
  {
    return;
  }
  // ---------------------------------------
  // HTML Response
  // ---------------------------------------
  String s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n <!DOCTYPE html> <html> <head> <title>GPS Interfacing with NodeMCU</title> <style>";
  s += "a:link {background-color: YELLOW;text-decoration: none;}";
  s += "table, th, td {border: 1px solid black;} </style> </head> <body> <h1  style=";
  s += "font-size:300%;";
  s += " ALIGN=CENTER> GPS Interfacing with NodeMCU</h1>";
  s += "<p ALIGN=CENTER style=""font-size:150%;""";
  s += "> <b>Location Details</b></p> <table ALIGN=CENTER style=";
  s += "width:50%";
  s += "> <tr> <th>Latitude</th>";
  s += "<td ALIGN=CENTER >";
  s += lat_str;
  s += "</td> </tr> <tr> <th>Longitude</th> <td ALIGN=CENTER >";
  s += lng_str;
  s += "</td> </tr> <tr>  <th>Temperature</th> <td ALIGN=CENTER >";
  s += String(t);
  s += "</td> </tr> <tr>  <th>Humidity</th> <td ALIGN=CENTER >";
  s += String(h);
  s += "</td> </tr> <tr>  <th>Pump status</th> <td ALIGN=CENTER >";
  s += String(h);
  s += "</td> </tr> <tr>  <th>Date</th> <td ALIGN=CENTER >";
  s += date_str;
  s += "</td></tr> <tr> <th>Time</th> <td ALIGN=CENTER >";
  s += time_str;
  s += "</td>  </tr> </table> ";
  if (gps.location.isValid())
  {
    s += "<p align=center><a style=""color:RED;font-size:125%;"" href=""http://maps.google.com/maps?&z=15&mrt=yp&t=k&q=";
    s += lat_str;
    s += "+";
    s += lng_str;
    s += """ target=""_top"">Click here!</a> To check the location in Google maps.</p>";
  }
  s += "</body> </html> \n";
  httpClient.print(s);
  // ---------------------------------------
  // HTML Response
  // ---------------------------------------
  //  if (millis() -
  //  delay(10000);
}
