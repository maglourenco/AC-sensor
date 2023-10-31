#include <Arduino.h>
#include "config.h"
#include "HX711.h"
#include "soc/rtc.h"
#include "ThingSpeak.h"
#include "twilio.hpp"
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN = 4;
const int touchPin = 15; // GPIO15 for touch sensor
const int threshold = 20; // Adjust this threshold as needed
// Weight value
int weightMapped = -1;
int previousWeightMapped = -1;
HX711 scale;

// Timer & other variables
unsigned long lastTime = 0;
unsigned long timerDelay = 10000;
bool touched = false;
bool smsSent = false;
unsigned long lastTouchTime = 0;
const unsigned long backlightDuration = 10000; // 10 seconds

WiFiClient  client;
Twilio *twilio;
LiquidCrystal_I2C lcd(0x27, 16, 2); 
TaskHandle_t ThingSpeakTask, SMSTask;

/**
 * sendDataToThingSpeakTask - send data to ThingSpeak every 16s (rate limited to once every 15s)
 */
void sendDataToThingSpeakTask( void * pvParameters){
  for(;;){
    connectWifi();
    ThingSpeak.writeField(THINGSPEAK_CHANNEL_NUMBER, 2, weightMapped, THINGSPEAK_API_KEY);
    delay(16000);
  } 
}

/**
 * sendSMSTask - send SMS through the Twilio API when the percentage is > 70%
 */
void sendSMSTask( void * pvParameters){
  String response;
  String message = "Nivel do AC elevado! " + String(weightMapped) + "%";
  twilio->send_message(TWILIO_TO_NUMBER, TWILIO_FROM_NUMBER, message, response);
  // Delete the task itself when done
  vTaskDelete(NULL);
}

void connectWifi() {
  // Connect or reconnect to WiFi
  if(WiFi.status() != WL_CONNECTED){
    while(WiFi.status() != WL_CONNECTED){
      WiFi.begin(WIFI_SSID, WIFI_PASS); 
      delay(5000);     
    } 
  }
}

/**
 * customMapping - maps a value of weight to a corresponding percentage considering...
 * 200g -> 0%
 * 2000 -> 100%
 */
int customMapping(int value) {
  if (value <= 200) {
        return 0;
  } else if (value >= 2000) {
        return 100;
  } else {
      return map(value, 200, 2000, 0, 100);
  }
}

void setup() {
  // Initialize the scale
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);       
  scale.set_scale(124793/303);
  scale.tare(); // reset the scale to 0
  
  // Initialize the LCD display 
  lcd.init();                      
  lcd.noBacklight();
  //lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Initializing");
  lcd.setCursor(0,1);
  lcd.print("the scale...");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Scale ready!");
  delay(1000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Weight (g): ");
  
  WiFi.mode(WIFI_STA); 
  connectWifi();

  // Initialize ThingSpeak connection
  ThingSpeak.begin(client);  
  delay(100);

  // Initialize Twilio connection
  twilio = new Twilio(TWILIO_ACCOUNT_SID, TWILIO_AUTH_TOKEN);  
  delay(100);
  
  //create a task that will be executed continuously in the sendDataToThingSpeakTask() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
                    sendDataToThingSpeakTask,   /* Task function. */
                    "sendDataToThingSpeakTask",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &ThingSpeakTask,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */  
}

void loop() {
  float weightRaw = scale.get_units();
  int weight = round(weightRaw);
  // Set it to the minimum value of 0
  if (weight < 0) {
    weight = 0; 
  }
  weightMapped = customMapping(weight);

  lcd.setCursor(0,1);
  String message = String(weight) + " " + String(weightMapped) + "%";
  lcd.print(message);
  delay(200);
  lcd.setCursor(0,1);
  lcd.print("          ");

  /*
   * If the weight is > 70 &
   * No SMS sent yet &
   * Not an outlier - if the previous weight measured is equal to the current
   */
  if (weightMapped > 70 && !smsSent && weightMapped == previousWeightMapped) {
    xTaskCreatePinnedToCore(sendSMSTask, "sendSMSTask", 10000, NULL, 1, &SMSTask, 0);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("SMS SENT!");
    delay(2000);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Weight (g): ");
    
    //delay(200);
    smsSent = true;
  } else if (weightMapped <= 70) {
    // Reset the flag when weightMapped is less than or equal to 70
    smsSent = false;
  }
  
  unsigned long currentTime = millis();
  
  // Read the state of the touch sensor
  int touchValue = touchRead(touchPin);

  // Check if the touchValue is below the threshold and the sensor was not previously touched
  if (touchValue < 50 && !touched) {
    // Small delay to account for false positives on touch sensor
    delay(500);
    // Read the state of the touch sensor - read again after the delay for a 'soft' software debounce
    touchValue = touchRead(touchPin);
    if (touchValue < 50 && !touched) {
      lcd.backlight(); // Turn on the backlight
      lastTouchTime = currentTime; // Record the time of touch
      touched = true; // Set the touched flag to true
    }
  }

  // Check if the backlight should be turned off after 5 seconds
  if (touched && currentTime - lastTouchTime >= backlightDuration) {
    lcd.noBacklight(); // Turn off the backlight
    touched = false; // Reset the touched flag
  }
  delay(50);

  previousWeightMapped = weightMapped;
}
