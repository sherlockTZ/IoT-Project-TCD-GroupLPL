#include <WiFi.h>
#include <MQ135.h>
#include "Secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GFX.h>
#include <BluetoothSerial.h>
#include <NimBLEDevice.h>
#define LED_MATRIX_ADDR 0x70
#define BUZZER_PIN 16
#define MQ135_PIN 34
#define MAX_PIN 32

#define AWS_IOT_PUBLISH_TOPIC "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"
// Add the necessary headers
#include <WiFiClient.h>
#include <WiFiServer.h>
const char *AP_SSID = "ESP32AP";
const char *AP_PASSWORD = "123456";
// Add a constant for the server port
const uint16_t serverPort = 80;

// Create a Wi-Fi server instance
WiFiServer tcpServer(serverPort);
MQ135 mq135(MQ135_PIN);
Adafruit_8x8matrix matrix = Adafruit_8x8matrix();
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);
float air_quality;
float sound_level;

static const char *SERVICE_UUID = "3a4e2ff2-c9fb-11ed-afa1-0242ac120002";
static const char *CHARACTERISTIC_UUID = "7214fb32-c9fb-11ed-afa1-0242ac120002";

BluetoothSerial SerialBT;

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();
    Serial.print("Received value: ");
    Serial.println(value.c_str());
  }
};

float readAndDisplayAirQuality()
{

  
  float air_quality = mq135.getPPM();
  /* digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW); */
  matrix.clear();
  matrix.setCursor(0, 0);
  matrix.print(air_quality);
  matrix.writeDisplay();

  delay(1000);
  return air_quality;
}

float readAndDisplaySoundLevel()
{

  float max9814_gain = 60.0;            // Set this to the gain value you have selected for the MAX9814 (40dB, 50dB, or 60dB)
  float adc_ref_voltage = 3.3;          // Set this to your ADC reference voltage (e.g., 3.3V or 5V)
  // float microphone_sensitivity = -44.0; // Set this to the sensitivity of your electret microphone (e.g., -44 dBV/Pa)

  int mic_value = analogRead(MAX_PIN);
  Serial.println(mic_value);
  float output_voltage = (float)mic_value / 4095.0 * adc_ref_voltage;

  // Calculate the input-referred voltage of the microphone
  // float input_voltage = output_voltage / pow(10, max9814_gain / 20);
  // float sensitivity_v_per_pa = pow(10, microphone_sensitivity / 20) * 0.001;
  // float sound_pressure = input_voltage / sensitivity_v_per_pa;

  // Calculate the sound pressure level in dB SPL
  // Calculate the sound pressure level in dB SPL
  float sound_level1 = 20 * log10(output_voltage / adc_ref_voltage);

  matrix.clear();
  matrix.setCursor(0, 0);
  matrix.print(sound_level1);
  matrix.writeDisplay();
  delay(1000);
  return sound_level1;
}

String get_wifi_status(int status)
{
  switch (status)
  {
  case WL_IDLE_STATUS:
    return "WL_IDLE_STATUS";
  case WL_SCAN_COMPLETED:
    return "WL_SCAN_COMPLETED";
  case WL_NO_SSID_AVAIL:
    return "WL_NO_SSID_AVAIL";
  case WL_CONNECT_FAILED:
    return "WL_CONNECT_FAILED";
  case WL_CONNECTION_LOST:
    return "WL_CONNECTION_LOST";
  case WL_CONNECTED:
    return "WL_CONNECTED";
  case WL_DISCONNECTED:
    return "WL_DISCONNECTED";
  }
}

void messageHandler(char *topic, byte *payload, unsigned int length)
{
  Serial.print("incoming: ");
  Serial.println(topic);

  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char *message = doc["message"];
  Serial.println(message);
  float air_quality = doc["air_quality"];
  float soung_level = doc["sound_level"];
  for (int i = 0; i < length; i++) 
  {
    Serial.print((char)payload[i]); // Pring payload content
  }
    char buzzer = (char)payload[62]; // Extracting the controlling command from the Payload to Controlling Buzzer from AWS
    Serial.print("Command: ");
    Serial.println(buzzer);
    if (air_quality > 500 && sound_level > -5)
    {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(1000);
      digitalWrite(BUZZER_PIN, LOW);
    }
    else
    {
      digitalWrite(BUZZER_PIN, LOW);
    }
}

void connectAWS()
{
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress IP = WiFi.softAPIP();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }

  //
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);

  // Create a message handler
  client.setCallback(messageHandler);

  Serial.println("Connecting to AWS IOT");

  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(100);
  }

  if (!client.connected())
  {
    Serial.println("AWS IoT Timeout!");
    return;
  }

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

  Serial.println("AWS IoT Connected!");
}
void publishMessage(float air_quality, float sound_level, int result) {
  if (!client.connected()) {
    connectAWS();
  }

  // Create a JSON formatted message to send
  StaticJsonDocument<256> jsonDoc;
  jsonDoc["air_quality"] = air_quality;
  jsonDoc["sound_level"] = sound_level;
  jsonDoc["result"] = result;

  char messageBuffer[256];
  serializeJson(jsonDoc, messageBuffer);
  Serial.println("Sending message to AWS IoT:");
  Serial.println(messageBuffer);

  // Publish the message to AWS IoT
  client.publish(AWS_IOT_PUBLISH_TOPIC, messageBuffer);
}

/* void processBluetoothData()
{
  if (SerialBT.available())
  {
    String data = SerialBT.readStringUntil('\n');
    if (data == "1")
    { // Turn on the LED matrix display
      matrix.fillScreen(1);
      matrix.writeDisplay();
    }
    else if (data == "0")
    { // Turn off the LED matrix display
      matrix.fillScreen(0);
      matrix.writeDisplay();
    }
  }
} */

void setup()
{
  // initialize local devices
  // Initialize I2C communication
  Wire.begin();
  // Initialize the Wi-Fi server
  WiFiServer tcpServer(serverPort);
  tcpServer.begin();
  Serial.print("Server started on port ");
  Serial.println(serverPort);
  
  // Initialize HT16K33 LED matrix
  matrix.begin(LED_MATRIX_ADDR);
  matrix.setBrightness(10);

  // Initialize MAX9814 microphone
  pinMode(MAX_PIN, INPUT);

  // Initialize MQ135 air quality sensor and passive buzzer
  pinMode(MQ135_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // initialize wifi connection and cloud connection
  Serial.begin(115200);
  delay(1000);

  connectAWS();
  Serial.println("Starting Arduino BLE Server application...");

  NimBLEDevice::init("Receiver");

  NimBLEServer *pServer = NimBLEDevice::createServer();
  NimBLEService *pService = pServer->createService(SERVICE_UUID);
  NimBLECharacteristic *pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::NOTIFY);

  pCharacteristic->setCallbacks(new CharacteristicCallbacks());
  pCharacteristic->setValue("Arduino Peripheral");

  pService->start();

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  int status = WL_IDLE_STATUS;
  Serial.println("\nConnecting");
  Serial.println(get_wifi_status(status));
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    status = WiFi.status();
    Serial.println(get_wifi_status(status));
  }

  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

void loop()
{
  // Add code to handle incoming TCP client connections and receive data
  WiFiClient wclient = tcpServer.available();

  if (wclient) {
    Serial.println("Wifi-Client connected");
    
    while (wclient.connected()) {
      if (wclient.available()) {
        int result = wclient.parseInt();
        Serial.print("Received result: ");
        Serial.println(result);
        air_quality = readAndDisplayAirQuality();
        sound_level = readAndDisplaySoundLevel();
        if (isnan(air_quality) || isnan(sound_level)) // Check if any reads failed and exit early (to try again).
        {
          Serial.println(F("Failed to read from sensors!"));
          return;
        }
        //print out serial values and publish message if air quality is above 0 and smaller than 2000 and sound level is above 0 and smaller than 100
        Serial.print(F("Air_quality: "));
        Serial.print(air_quality);
        Serial.print(F(" Sound_level: "));
        Serial.print(sound_level);
        Serial.print(" dB");
        /* processBluetoothData(); // Process incoming Bluetooth data */
        publishMessage(air_quality, sound_level, result);
      }
    }

    wclient.stop();
    Serial.println("WifiClient disconnected");
  }
  
    
  client.loop();
  delay(2000);
}
