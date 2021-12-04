//===================
//ESP8266 Web Server
//===================
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);

//---------------------------------
const char* ssid     = "biasalahhh";
const char* password = "zul12345";
//======================================================================
void setup()
{
  Serial.begin(115200);
  pinMode(D1, INPUT); pinMode(D2, OUTPUT); digitalWrite(D2, LOW);
  
  WiFi.begin(ssid, password);
  Serial.print("\n\r \n\rWorking to connect");
  while (WiFi.status() != WL_CONNECTED) {delay(500); Serial.print(".");}
  Serial.println("");
  Serial.println("ESP8266 Webpage");
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  server.on("/", handleRoot);
  server.on("/pot", pot);
  server.on("/button", button);
  server.on("/led", led);
  
  server.begin();
  Serial.println("HTTP server started");
}
//======================================================================
void loop()
{server.handleClient();}



//-----------------------------------
//functions executing client requests
//-----------------------------------
void handleRoot()
{
  server.send(200, "text/html", "<h1>ESP8266 Controller<br>Potentiometer: /pot<br>Button: /button<br>LED: /led</h1>");
}
//------------------------------------------------------------------------
void pot()
{
  String pot_val = String("Pot Value: ") + String(analogRead(0)) + "\n";
  pot_val = "<html><head><meta http-equiv=\"refresh\" content=\"2\"> <body bgcolor=\"#ffff00\">"
         + pot_val + "</body></html>";
  server.send(200, "text/html", pot_val);
}
//------------------------------------------------------------------------
void button()
{
  String button_val;
  if(digitalRead(D1)==LOW)
  {
    button_val = String("Button: ") + String("OFF") + "\n";
  }
  if(digitalRead(D1)==HIGH)
  {
    button_val = String("Button: ") + String("ON") + "\n";
  }
  button_val = "<html><head><meta http-equiv=\"refresh\" content=\"2\"> <body bgcolor=\"#add8e6\">"
                + button_val + "</body></html>";
  server.send(200, "text/html", button_val);
}
//------------------------------------------------------------------------
void led()
{
  digitalWrite(D2, digitalRead(D2)^1);
  server.send(200, "text/html", "<p><a href=/led>Toggle LED</a></p>");
}
