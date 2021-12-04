#include <Wire.h>
#include "SparkFun_VL53L1X.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>

#include <WiFiClient.h>
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);
//#include <LiquidCrystal.h>
//LiquidCrystal lcd(D3,D4,D5,D6,D7,D8);
void setup_wifi();
void zones_calibration();
void zones_calibration_boot();
void processPeopleCountingData(int16_t Distance, uint8_t zone);
void handleRoot();


const char* ssid = "biasalahhh";     //wi-fi netwrok name
const char* password = "zul12345";  //wi-fi network password

const int threshold_percentage = 80;

// if "true", the raw measurements are sent via MQTT during runtime (for debugging) - I'd recommend setting it to "false" to save traffic and system resources.
// in the calibration phase the raw measurements will still be sent through MQTT
static bool update_raw_measurements = false;

// this value has to be true if the sensor is oriented as in Duthdeffy's picture
static bool advised_orientation_of_the_sensor = true;

// this value has to be true if you don't need to compute the threshold every time the device is turned on
static bool save_calibration_result = false;

#define EEPROM_SIZE 8
#define DEFAULT_PEOPLE_LIMIT 3

SFEVL53L1X distanceSensor(Wire);//, SHUTDOWN_PIN, INTERRUPT_PIN);

static int NOBODY = 0;
static int SOMEONE = 1;
static int LEFT = 0;
static int RIGHT = 1;

static int DIST_THRESHOLD_MAX[] = {0, 0};   // treshold of the two zones
static int MIN_DISTANCE[] = {0, 0};

static int PathTrack[] = {0,0,0,0};
static int PathTrackFillingSize = 1; // init this to 1 as we start from state where nobody is any of the zones
static int LeftPreviousStatus = NOBODY;
static int RightPreviousStatus = NOBODY;

static int center[2] = {0,0}; /* center of the two zones */  
static int Zone = 0;
static int PplCounter = 0;

static int ROI_height = 0;
static int ROI_width = 0;


void setup(void)
{
  //lcd.begin(16, 2);
  Wire.begin();
  // initialize the EEPROM memory
  EEPROM.begin(EEPROM_SIZE);
  
  Serial.begin(9600);
  Serial.println("PEOPLE COUNTING SYSTEM - EMBEDDED SYSTEM");

  if (distanceSensor.init() == false)
    Serial.println("Sensor online!");
  distanceSensor.setIntermeasurementPeriod(50);
  distanceSensor.setDistanceModeLong();

  Serial.setTimeout(500);// Set time out for setup_wifi();
  setup_wifi();
  delay(1000);
  zones_calibration_boot();
  
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  
  server.begin();
  Serial.print("HTTP server started");
}


void loop(void)
{
  server.handleClient();
  uint16_t distance;
  distanceSensor.setROI(ROI_height, ROI_width, center[Zone]);  // first value: height of the zone, second value: width of the zone
  delay(52);
  distanceSensor.setTimingBudgetInMs(50);
  distanceSensor.startRanging(); //Write configuration bytes to initiate measurement
  distance = distanceSensor.getDistance(); //Get the result of the measurement from the sensor
  distanceSensor.stopRanging();

  //PRINT NUMBER OF PEOPLE INSIDE/OUTSIDE
  Serial.println(PplCounter);
  // lcd.setCursor(0,0);
  // lcd.print("Number of users");
  // lcd.setCursor(0,1);
  // lcd.print(PplCounter);

   // inject the new ranged distance in the people counting algorithm
  processPeopleCountingData(distance, Zone);

  Zone++;
  Zone = Zone%2;


}

// NOBODY = 0, SOMEONE = 1, LEFT = 0, RIGHT = 1

void processPeopleCountingData(int16_t Distance, uint8_t zone) {

    int CurrentZoneStatus = NOBODY;
    int AllZonesCurrentStatus = 0;
    int AnEventHasOccured = 0;

  if (Distance < DIST_THRESHOLD_MAX[Zone] && Distance > MIN_DISTANCE[Zone]) {
    // Someone is in !
    CurrentZoneStatus = SOMEONE;
  }

  // left zone
  if (zone == LEFT) {

    if (CurrentZoneStatus != LeftPreviousStatus) {
      // event in left zone has occured
      AnEventHasOccured = 1;

      if (CurrentZoneStatus == SOMEONE) {
        AllZonesCurrentStatus += 1;
      }
      // need to check right zone as well ...
      if (RightPreviousStatus == SOMEONE) {
        // event in left zone has occured
        AllZonesCurrentStatus += 2;
      }
      // remember for next time
      LeftPreviousStatus = CurrentZoneStatus;
    }
  }
  // right zone
  else {

    if (CurrentZoneStatus != RightPreviousStatus) {

      // event in left zone has occured
      AnEventHasOccured = 1;
      if (CurrentZoneStatus == SOMEONE) {
        AllZonesCurrentStatus += 2;
      }
      // need to left right zone as well ...
      if (LeftPreviousStatus == SOMEONE) {
        // event in left zone has occured
        AllZonesCurrentStatus += 1;
      }
      // remember for next time
      RightPreviousStatus = CurrentZoneStatus;
    }
  }

  // if an event has occured
  if (AnEventHasOccured) {
    if (PathTrackFillingSize < 4) {
      PathTrackFillingSize ++;
    }

    // if nobody anywhere lets check if an exit or entry has happened
    if ((LeftPreviousStatus == NOBODY) && (RightPreviousStatus == NOBODY)) {

      // check exit or entry only if PathTrackFillingSize is 4 (for example 0 1 3 2) and last event is 0 (nobobdy anywhere)
      if (PathTrackFillingSize == 4) {
        // check exit or entry. no need to check PathTrack[0] == 0 , it is always the case
        Serial.println();
        if ((PathTrack[1] == 1)  && (PathTrack[2] == 3) && (PathTrack[3] == 2)) {
          //////////////////////////////-EXIT-//////////////////////////////////
          PplCounter--;
        } else if ((PathTrack[1] == 2)  && (PathTrack[2] == 3) && (PathTrack[3] == 1)) {
          //////////////////////////////-ENTRY-///////////////////////////////////
          PplCounter++;
          }
      }
      for (int i=0; i<4; i++){
        PathTrack[i] = 0;
      }
      PathTrackFillingSize = 1;
    }
    else {
      // update PathTrack
      // example of PathTrack update
      // 0
      // 0 1
      // 0 1 3
      // 0 1 3 1
      // 0 1 3 3
      // 0 1 3 2 ==> if next is 0 : check if exit
      PathTrack[PathTrackFillingSize-1] = AllZonesCurrentStatus;
    }
  }
}


void zones_calibration_boot(){
  if (save_calibration_result){
    // if possible, we take the old values of the zones contained in the EEPROM memory
    if (EEPROM.read(0) == 1){
      // we have data in the EEPROM
      center[0] = EEPROM.read(1);
      center[1] = EEPROM.read(2);
      ROI_height = EEPROM.read(3);
      ROI_width = EEPROM.read(3);
      DIST_THRESHOLD_MAX[0] = EEPROM.read(4)*100 + EEPROM.read(5);;
      DIST_THRESHOLD_MAX[1] = EEPROM.read(6)*100 + EEPROM.read(7);
    }
    else{
      // there are no data in the EEPROM memory
      zones_calibration();
    }
  }
  else
    zones_calibration();
}

void zones_calibration(){
  // the sensor does 100 measurements for each zone (zones are predefined)
  // each measurements is done with a timing budget of 100 ms, to increase the precision
  center[0] = 167;
  center[1] = 231;
  ROI_height = 8;
  ROI_width = 8;
  delay(500);
  Zone = 0;
  float sum_zone_0 = 0;
  float sum_zone_1 = 0;
  uint16_t distance;
  int number_attempts = 20;
  for (int i=0; i<number_attempts; i++){
      // increase sum of values in Zone 0
      distanceSensor.setROI(ROI_height, ROI_width, center[Zone]);  // first value: height of the zone, second value: width of the zone
      delay(50);
      distanceSensor.setTimingBudgetInMs(50);
      distanceSensor.startRanging(); //Write configuration bytes to initiate measurement
      distance = distanceSensor.getDistance(); //Get the result of the measurement from the sensor
      distanceSensor.stopRanging();      
      sum_zone_0 = sum_zone_0 + distance;
      Zone++;
      Zone = Zone%2;

      // increase sum of values in Zone 1
      distanceSensor.setROI(ROI_height, ROI_width, center[Zone]);  // first value: height of the zone, second value: width of the zone
      delay(50);
      distanceSensor.setTimingBudgetInMs(50);
      distanceSensor.startRanging(); //Write configuration bytes to initiate measurement
      distance = distanceSensor.getDistance(); //Get the result of the measurement from the sensor
      distanceSensor.stopRanging();      
      sum_zone_1 = sum_zone_1 + distance;
      Zone++;
      Zone = Zone%2;
  }
  // after we have computed the sum for each zone, we can compute the average distance of each zone
  float average_zone_0 = sum_zone_0 / number_attempts;
  float average_zone_1 = sum_zone_1 / number_attempts;
  // the value of the average distance is used for computing the optimal size of the ROI and consequently also the center of the two zones
  int function_of_the_distance = 16*(1 - (0.22 * 2) / (0.34 * (min(average_zone_0, average_zone_1)/1000) ));
  delay(1000);
  int ROI_size = min(8, max(4, function_of_the_distance));
  ROI_width = ROI_size;
  ROI_height = ROI_size;
  if (advised_orientation_of_the_sensor){
    
    switch (ROI_size) {
        case 4:
          center[0] = 150;
          center[1] = 247;
          break;
        case 5:
          center[0] = 159;
          center[1] = 239;
          break;
        case 6:
          center[0] = 159;
          center[1] = 239;
          break;
        case 7:
          center[0] = 167;
          center[1] = 231;
          break;
        case 8:
          center[0] = 167;
          center[1] = 231;
          break;
      }
  }
  else{
    switch (ROI_size) {
        case 4:
          center[0] = 193;
          center[1] = 58;
           break;
        case 5:
          center[0] = 194;
          center[1] = 59;
          break;
        case 6:
          center[0] = 194;
          center[1] = 59;
          break;
        case 7:
          center[0] = 195;
          center[1] = 60;
          break;
        case 8:
          center[0] = 195;
          center[1] = 60;
          break;
      }
  }
  delay(2000);
  // we will now repeat the calculations necessary to define the thresholds with the updated zones
  Zone = 0;
  sum_zone_0 = 0;
  sum_zone_1 = 0;
  for (int i=0; i<number_attempts; i++){
      // increase sum of values in Zone 0
      distanceSensor.setROI(ROI_height, ROI_width, center[Zone]);  // first value: height of the zone, second value: width of the zone
      delay(50);
      distanceSensor.setTimingBudgetInMs(50);
      distanceSensor.startRanging(); //Write configuration bytes to initiate measurement
      distance = distanceSensor.getDistance(); //Get the result of the measurement from the sensor
      distanceSensor.stopRanging();      
      sum_zone_0 = sum_zone_0 + distance;
      Zone++;
      Zone = Zone%2;

      // increase sum of values in Zone 1
      distanceSensor.setROI(ROI_height, ROI_width, center[Zone]);  // first value: height of the zone, second value: width of the zone
      delay(50);
      distanceSensor.setTimingBudgetInMs(50);
      distanceSensor.startRanging(); //Write configuration bytes to initiate measurement
      distance = distanceSensor.getDistance(); //Get the result of the measurement from the sensor
      distanceSensor.stopRanging();      
      sum_zone_1 = sum_zone_1 + distance;
      Zone++;
      Zone = Zone%2;
  }
  average_zone_0 = sum_zone_0 / number_attempts;
  average_zone_1 = sum_zone_1 / number_attempts;
  float threshold_zone_0 = average_zone_0 * threshold_percentage/100; // they can be int values, as we are not interested in the decimal part when defining the threshold
  float threshold_zone_1 = average_zone_1 * threshold_percentage/100;
  
  DIST_THRESHOLD_MAX[0] = threshold_zone_0;
  DIST_THRESHOLD_MAX[1] = threshold_zone_1;

  delay(2000);

  // we now save the values into the EEPROM memory
  int hundred_threshold_zone_0 = threshold_zone_0 / 100;
  int hundred_threshold_zone_1 = threshold_zone_1 / 100;
  int unit_threshold_zone_0 = threshold_zone_0 - 100* hundred_threshold_zone_0;
  int unit_threshold_zone_1 = threshold_zone_1 - 100* hundred_threshold_zone_1;

  EEPROM.write(0, 1);
  EEPROM.write(1, center[0]);
  EEPROM.write(2, center[1]);
  EEPROM.write(3, ROI_size);
  EEPROM.write(4, hundred_threshold_zone_0);
  EEPROM.write(5, unit_threshold_zone_0);
  EEPROM.write(6, hundred_threshold_zone_1);
  EEPROM.write(7, unit_threshold_zone_1);
  EEPROM.commit();
  
}


void setup_wifi() 
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
//    WiFi.begin(ssid, password);
  }
}


//-----------------------------------
//functions executing client requests
//-----------------------------------
void handleRoot()
{
  //server.send(200, "text/html", "<h1>ESP8266 Controller<br>Potentiometer: /pot<br>Button: /button<br>LED: /led</h1>");
  //String msg = String("Number Of People: ") + String(PplCounter) + "\n";
  String msg = String(PplCounter) + "\n";
  msg = "<html><head><style>body{font-size: 500px;text-align: center;}</style></head><meta http-equiv=\"refresh\" content=\"0.1\"> <body bgcolor=\"#ffffff\">"
            + msg + "</body></html>";

  server.send(200, "text/html", msg);

}