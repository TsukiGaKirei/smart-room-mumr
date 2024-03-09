#include <WiFi.h>
#include <PubSubClient.h>

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Daikin.h>
#define SOUND_SPEED 0.034

// MQTT broker configuration
const char* mqttBroker = "34.101.245.2";
const int mqttPort = 1883;
const char* mqttUser = "user1";
const char* mqttPassword = "qweasd123";
const char* mqttTopic = "api-topic";
const char* deviceID = "1";  //device id === rid
// WiFi configuration
const char* ssid = "iot-gemil";
const char* password = "qweasd123";
int acTemp = 26;
bool lampState = false;
bool acStatus = false;
bool isDoorOpen = false;
String command;

int currentPerson = 0;
int sensor1[] = { 18, 5 };
int sensor2[] = { 12, 13 };
int sensor1Initial;
int sensor2Initial;
int doorTimer = 0;
int loopDelay = 100;
// Format MQTT message
// dev_id, ac_temp, room_status, door_status

// Doorlock purpose
const int doorPin = 23;  // Define the pin connected to the door control

WiFiClient espClient;
PubSubClient client(espClient);

String sequence = "";         // Assuming this is a global variable
int timeoutCounter = 0;       // Assuming this is a global variable

// AC related
const uint16_t kIrLed = 4;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).
IRDaikinESP ac(kIrLed);     // Set the GPIO to be used to sending the message

void setup() {
  Serial.begin(115200);
  ac.begin();
  pinMode(doorPin, OUTPUT);
  pinMode(22, OUTPUT);
  digitalWrite(doorPin, HIGH);
  digitalWrite(22, HIGH);


  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Connect to MQTT broker
  client.setServer(mqttBroker, mqttPort);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT broker...");
    if (client.connect("ESP32Client", mqttUser, mqttPassword)) {
      Serial.println("Connected to MQTT broker");
      client.subscribe(mqttTopic);
    } else {
      Serial.printf("Failed, rc=%d. Retrying in 5 seconds...\n", client.state());
      delay(5000);
    }
  }

  // Initialize sensor values for person counting
  sensor1Initial = measureDistance(sensor1);
  sensor2Initial = measureDistance(sensor2);
}

void loop() {
  client.loop();

  // Read ultrasonic sensors
  int sensor1Val = measureDistance(sensor1);
  int sensor2Val = measureDistance(sensor2);

  // Process the data
  // jarak awal 15
  // initial dikurangi jadi kalo terlalu jauh ga keitung
  if (sensor1Val < 8 && sequence.charAt(0) != '1') {  //initial 17
    sequence += "1";
    delay(500);
    doorTimer-=500;

  } else if (sensor2Val < 8 && sequence.charAt(0) != '2') {  //initial 17
    sequence += "2";
    delay(500);
    doorTimer-=500;
  }

  if (sequence.equals("12")) {
    personChange(currentPerson+1);
    sequence = "";
  } else if (sequence.equals("21") && currentPerson > 0) {
    sequence = "";
    personChange(currentPerson-1);
  }

  // Resets the sequence if it is invalid or timeouts
  if (sequence.length() > 2 || sequence.equals("11") || sequence.equals("22") || timeoutCounter > 200) {
    sequence = "";
  }

  if (sequence.length() == 1) {  //
    timeoutCounter++;
  } else {
    timeoutCounter = 0;
  }

  if (currentPerson >= 1 && !lampState) {
    lampState = true;
    turnOnLamp();
  } else if (currentPerson == 0 && lampState) {
    lampState = false;
    turnOffLamp();
  }

  if (doorTimer > 0 && isDoorOpen){
     doorTimer -= loopDelay;
    Serial.println(doorTimer);
    Serial.println("Door is open");

     }
  else if (doorTimer <= 0 && isDoorOpen) {
    closeDoor();
    Serial.println(doorTimer);
    Serial.println("doorClose");

    doorTimer = 0;
    
  }
    delay(loopDelay);
  

}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message received from topic: " + String(topic));

  // Convert payload to a string
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // Tokenize the message using space as a delimiter
  String deviceIDReceived = getValue(message, ';', 0);

  // Check if the received deviceID matches the deviceID of the ESP32
  if (!deviceIDReceived.equals(deviceID)) {
    Serial.println("Ignoring message. Device ID mismatch.");
    return;
  }
  Serial.println("Device ID: " + deviceIDReceived);
  // Ensure the message has the expected format



  command = getValue(message, ';', 1);
  if (command == "ac_on") {
    acTemp = (getValue(message, ';', 2).toInt());
    acStatus = true;
    turnOnAC(acTemp);
    Serial.println("Command Received: AC ON");
  } else if (command == "ac_off") {
    turnOffAC();
    acStatus = false;
    Serial.println("Command Received: AC OFF");

  } else if (command == "door_open") {
    openDoor();
    Serial.println("Command Received: Open Door");

  } else if (command == "lamp_on") {
    turnOnLamp();
  }
}
// Validate and convert room status and door status




// Save the values to variables or perform actions as needed
// For example:
// roomStatusVariable = roomStatus;
// roomTempVariable = ACTemp.toInt();
// doorStatusVariable = doorStatus;


// Function to get a specific value from a String based on a delimiter
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

int measureDistance(int a[]) {
  pinMode(a[1], OUTPUT);
  digitalWrite(a[1], LOW);
  delayMicroseconds(2);
  digitalWrite(a[1], HIGH);
  delayMicroseconds(10);
  digitalWrite(a[1], LOW);
  pinMode(a[0], INPUT);
  long duration = pulseIn(a[0], HIGH, 100000);
  long   distanceCm = duration * SOUND_SPEED/2;

  return distanceCm;
}

// Function to publish a message to the specified topic
void publishMessage(const char* message) {
  client.publish("esp32-topic", message);
}

void personChange(int x) {
  
  if(x<0)currentPerson = 0;
  else if(x>=0)currentPerson=x;
  Serial.println("ganti");
  char message[15];
  //send rid;count_person;lamp_status
  if (currentPerson >= 1) sprintf(message, "1;%d;true", currentPerson);
  else sprintf(message, "1;%d;false", currentPerson);
  publishMessage(message);
}

//type of mqtt publish esp32-topic
//rid;count_person;lamp_status
//rid;door_open;
//rid;door_close;

void turnOnAC(int temp) {
  ac.on();
  ac.setFan(3);
  ac.setMode(kDaikinCool);
  ac.setTemp(temp);

  Serial.println(ac.toString());
#if SEND_DAIKIN
  ac.send();
#endif  // SEND_DAIKIN
}

void turnOffAC() {
  ac.off();
#if SEND_DAIKIN
  ac.send();
#endif  // SEND_DAIKIN
  Serial.println(ac.toString());

}


void openDoor() {
    char message[15];

  digitalWrite(doorPin, LOW);  // Activate the door (assuming HIGH activates the door)
  isDoorOpen = true;
  doorTimer = 10000;
  Serial.println("door open");
}

void closeDoor() {

  digitalWrite(doorPin, HIGH);  // Deactivate the door after 10 seconds
  isDoorOpen = false;
  Serial.println("door close");
}


void turnOnLamp() {
  lampState = true;
  digitalWrite(22, LOW);
  
  Serial.println("LAMP ON");


}


void turnOffLamp() {
  lampState = false;
  digitalWrite(22, HIGH);
  Serial.println("LAMP OFF");
}