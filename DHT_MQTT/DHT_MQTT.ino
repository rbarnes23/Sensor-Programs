#include "DHTesp.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "definitions.h"

#ifdef DISPLAYAVAIL
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`
#endif

WiFiClient espClient;
PubSubClient client(espClient);
DHTesp dht;  //DHT22
long lastMsg = 0;
char msg[120];


// Initialize the OLED display using Wire library
#ifdef DISPLAYAVAIL
SSD1306  display(0x3C, D2, D1); // (I2C Address, SCL Pin, SDA Pin)
#endif

// Initialising the display.
void setupDisplay()
{
  // Initialising the display.
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  //display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  // init done

#ifdef DISPLAYAVAIL
  display.init();
  display.clear();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.display();
  drawSplashImage();
  delay(1000);
#endif
}

// Setup Mqtt
void setupMqtt()
{
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);
  Serial.println();
  Serial.println("Status\tHumidity (%)\tTemperature (C)\t(F)\tHeatIndex (C)\t(F)");
}

// Print Debug Messages
void printStatusMessages(float T, float H)
{
  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();

  Serial.print(dht.getStatusString());
  Serial.print("\t");
  Serial.print(humidity, 1);
  Serial.print("\t\t");
  Serial.print(temperature, 1);
  Serial.print("\t\t");
  Serial.print(dht.toFahrenheit(temperature), 1);
  Serial.print("\t\t");
  Serial.print(dht.computeHeatIndex(temperature, humidity, false), 1);
  Serial.print("\t\t");
  Serial.println(dht.computeHeatIndex(dht.toFahrenheit(temperature), humidity, true), 1);
}

//Main Setup Routine
void setup()
{
  Serial.begin(115200);
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  dht.setup(14); // data pin 14
  setup_wifi();
  setupMqtt();
  setupDisplay();


  //ESP.deepSleep(FREQUENCYOFREADING);
}

void getReading()
{
  //Get Current Epoch Time
  time_t currentTime = time(nullptr);
  //Get Temp and Humitiy
  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();

}
void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > FREQUENCYOFREADING) {
    // Get the current Time
    time_t currentTime = time(nullptr);
    //  Get the humidity and temp
    float humidity = dht.getHumidity();
    float temperature = dht.getTemperature();
    displayData(temperature, humidity,1);

    //snprintf (msg, 50, "Temperature %s Humidity %s", tempf, humid);
    //    snprintf (msg, 50, "Temperature %s °F  %s%sH", tempf, humid, "%");
    //Serial.print("Publish message: ");
    //Serial.println(msg);
    sendData(temperature, humidity, currentTime);
    lastMsg = now;


  }
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SSID);

  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("TT", "Temperature and Humidity");
      // ... and resubscribe
      client.subscribe("TT");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void displayData(double lT, double lH, int tempType) {
#ifdef DISPLAYAVAIL
  //Format to string
  char tempToDisplay[10];
  char humid[10];
  char TMessage[20];
  char HMessage[20];
  //  Change this to go fahrenheit or Centrigrade
  if (tempType == 1) {
    dtostrf(dht.toFahrenheit(lT), 7, 2, tempToDisplay);
  } else {
    dtostrf(lT, 7, 2, tempToDisplay);
  }

  dtostrf(lH, 3, 0, humid);
  snprintf (TMessage, 20, "%s °F", tempToDisplay);
  snprintf (HMessage, 20, "%s%sH", humid, "%");

  //Display Message on screen
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(66, 0, "Troy Telematics");
  display.drawString(66, 16, "Temp/Humidty");
  display.setFont(ArialMT_Plain_16);
  display.drawString(66, 32, TMessage);
  display.drawString(66, 48, HMessage);
  display.display();
#endif
}

void sendData(float lT, float lH, time_t currentTime) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();
  data["sensor"] = "dht22";
  data["chipid"] = ESP.getChipId();//WiFi.macAddress();
  data["time"] = currentTime;
  data["temp"] = dht.toFahrenheit(lT);
  data["humid"] = lH;
  data.printTo(msg);
  char toTopic[100];
  const char* sensor = data["sensor"];
  const char* chipid = data["chipid"];
  //snprintf (toTopic, sizeof(toTopic), "TT/00000001/001/001/001/%s/%s",sensor,chipid );
  //Serial.println(toTopic);
  //client.publish(toTopic, msg);
  client.publish("TT/00000001/001/001/001/001", msg);
}

void drawSplashImage() {
#ifdef DISPLAYAVAIL
  //display.drawXbm(032, 14, reb_Logo_width, reb_Logo_height, reb_Logo_bits);
  display.drawXbm(32, 14, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  display.display();
#endif
}

