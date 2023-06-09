#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <DHTesp.h>

// Uncomment according to your hardware type
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define TEXT_SPEED 100 // speed of text movement on display
// #define HARDWARE_TYPE MD_MAX72XX::GENERIC_HW

// Function prototypes
DynamicJsonDocument getWeather(DynamicJsonDocument doc);
void printWeather(DynamicJsonDocument doc);
void printTime(void);
void printTemeperature(DynamicJsonDocument doc);
void printHumidity(DynamicJsonDocument doc);
void printRoomTemperature(void);
void printRoomHumidity(void);
void handleInterrupt();
void switchCaseFunction();
void printSplashScreen(void);
void waitMilis(unsigned long milistowait);

// Constants and pin definitions
#define MAX_DEVICES 4
#define CS_PIN 15
#define DHT_PIN 5 // Define the DHT sensor pin

const int INTERRUPT_PIN = 4;
volatile bool interrupt_triggered = false;
// variables to keep track of the timing of recent interrupts
unsigned long button_time = 0;
unsigned long last_button_time = 0;
int screen = 1;

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";
char timeString[6]; // 6 characters: HH:MM + null terminator

// Initialize the DHT sensor
DHTesp dht;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

int hours;
int minutes;
bool firstRun = true;

// Initialize the LED matrix display and the JSON document for weather data
MD_Parola Display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
WiFiClient client;
DynamicJsonDocument doc(1024);

const char *host = "api.openweathermap.org";
const String city = "YOUR_CITY";
const String apiKey = "YOUR_API_KEY";

unsigned long previousMillis = 0;              // stores the last time the flag was raised
const unsigned long interval = 10 * 60 * 1000; // 10 minutes for update from openweather - interval in milliseconds
unsigned long currentMillis;

void setup()
{
  // Initialize the LED matrix display and the serial monitor
  Display.begin();
  Display.setIntensity(0);
  Display.displayClear();
  Serial.begin(115200);

  // Display a splash screen on the LED matrix display
  printSplashScreen();

  // Connect to the WiFi network
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());

  // Get the initial weather data
  doc = getWeather(doc);

  // Initialize the time client and set the time zone
  timeClient.begin();
  float timezone = doc["timezone"];
  timeClient.setTimeOffset(timezone);

  // Initialize the DHT sensor and the interrupt pin
  dht.setup(DHT_PIN, DHTesp::DHT11);
  pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), handleInterrupt, FALLING);



}

void loop()
{
  // Display the current time on the LED matrix display
  printTime();
  
  // Check if button is pressed
  if (interrupt_triggered)
  {
    interrupt_triggered = false;
    switchCaseFunction();
  }

}

DynamicJsonDocument getWeather(DynamicJsonDocument doc)
{
  // get current millis for checking if required interval has passed since last update
  currentMillis = millis();

  // check if the interval has elapsed
  if (currentMillis - previousMillis >= interval || firstRun)
  {
    Serial.println("Interval has passed, getting weather data from server");
    previousMillis = currentMillis;

    Serial.println("it worked");
    if (!client.connect(host, 80))
    {
      Serial.println("Connection failed");
      waitMilis(1000);
      return doc;
    }
    Serial.println("Connected to server");

    // Send request to server
    client.print(String("GET /data/2.5/weather?q=") + city + "&appid=" + apiKey + "&units=metric" + "\r\n");

    // Wait for server response
    while (!client.available())
    {
      waitMilis(1000);
      Serial.println("Waiting for server response...");
    }

    // Read server response
    String response = client.readString();
    Serial.println("Server response:");
    Serial.println(response);

    DeserializationError error = deserializeJson(doc, response);

    if (error)
    {
      Serial.print(F("JSON parsing failed: "));
      Serial.println(error.f_str());
      return doc;
    }

    // Get temperature and weather description
    float temperature = doc["main"]["temp"];
    float temperature_min = doc["main"]["temp_min"];
    float temperature_max = doc["main"]["temp_max"];
    String weather = doc["weather"][0]["main"];
    String weather_desc = doc["weather"][0]["description"];
    int humidity = doc["main"]["humidity"];

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println("°C");
    Serial.print("Min temperature: ");
    Serial.print(temperature_min);
    Serial.println("°C");
    Serial.print("Max temperature: ");
    Serial.print(temperature_max);
    Serial.println("°C");
    Serial.print("The humidity is ");
    Serial.print(humidity);
    Serial.println("%");
    Serial.print("The forecast for today is ");
    Serial.println(weather);
    Serial.print("With ");
    Serial.println(weather_desc);

    firstRun = false;
  }
  return doc;
}

void printWeather(DynamicJsonDocument doc)
{

  getWeather(doc);
  String weather = doc["weather"][0]["main"];
  String weather_desc = doc["weather"][0]["description"];

  const char *weather_chr = weather.c_str();
  const char *weather_desc_chr = weather_desc.c_str();

  Display.displayText("Current weather - ", PA_LEFT, TEXT_SPEED, 0, PA_SCROLL_LEFT);
  while (!Display.displayAnimate() && !interrupt_triggered)
  {
    delay(10);
  }

  Display.displayText(weather_chr, PA_LEFT, TEXT_SPEED, 0, PA_SCROLL_LEFT);
  while (!Display.displayAnimate() && !interrupt_triggered)
  {
    delay(10);
  }
  waitMilis(5000);

  Display.displayText(weather_desc_chr, PA_LEFT, TEXT_SPEED, 0, PA_SCROLL_LEFT);
  while (!Display.displayAnimate() && !interrupt_triggered)
  {
    delay(100);
  }
  waitMilis(5000);

  Display.displayClear();

  return;
}

void printTime()
{
  //// Get time and show on screen ////////////////
  while (!interrupt_triggered)
  {
    timeClient.update();

    hours = timeClient.getHours();
    minutes = timeClient.getMinutes();

    sprintf(timeString, "%02d:%02d", hours, minutes);
    // Serial.println(timeString);

    Display.setTextAlignment(PA_CENTER);
    Display.print(timeString);
    delay(20);
  }
  return;
}

void printTemeperature(DynamicJsonDocument doc)
{
  getWeather(doc);
  float temperature = doc["main"]["temp"];
  // float temperature_min = doc["main"]["temp_min"];
  // float temperature_max = doc["main"]["temp_max"];
  temperature = roundf(temperature * 10) / 10;
  // temperature_min = roundf(temperature_min * 10) / 10;
  // temperature_max = roundf(temperature_max * 10) / 10;

  // Print current temperature
  char current_temperature[10];
  sprintf(current_temperature, "%0.1f%cC", temperature, char(176));
  Display.displayClear();
  Display.displayText("Current temperature", PA_LEFT, TEXT_SPEED, 0, PA_SCROLL_LEFT);
  while (!Display.displayAnimate() && !interrupt_triggered)
  {
    delay(10);
  }

  waitMilis(1000);
  Serial.println(current_temperature);
  Display.setTextAlignment(PA_CENTER);
  Display.print(current_temperature);
  waitMilis(5000);
  Display.displayClear();
}

void printHumidity(DynamicJsonDocument doc)
{

  getWeather(doc);
  int humidity = doc["main"]["humidity"];
  char current_humidity[10];
  sprintf(current_humidity, "%d%%", humidity);
  Display.displayClear();
  Display.displayText("Current humidity", PA_LEFT, TEXT_SPEED, 0, PA_SCROLL_LEFT);
  while (!Display.displayAnimate() && !interrupt_triggered)
  {
    delay(10);
  }

  waitMilis(1000);
  Serial.println(current_humidity);
  Display.setTextAlignment(PA_CENTER);
  Display.print(current_humidity);
  waitMilis(5000);

  Display.displayClear();

  return;
}

void printRoomTemperature(void)
{

  // Read temperature from the DHT sensor
  float temperature = dht.getTemperature();

  // Check if reading failed
  if (isnan(temperature))
  {
    Serial.println("Failed to read temperature from DHT sensor!");
    return;
  }

  // Print the temperature value to the serial monitor
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print("°C");

  char room_temperature[10];
  sprintf(room_temperature, "%0.1f%cC", temperature, char(176));
  Display.displayClear();
  Display.displayText("room temperature", PA_LEFT, TEXT_SPEED, 0, PA_SCROLL_LEFT);
  while (!Display.displayAnimate() && !interrupt_triggered)
  {
    delay(100);
  }

  waitMilis(500);
  Serial.println(room_temperature);
  Display.setTextAlignment(PA_CENTER);
  Display.print(room_temperature);
  waitMilis(5000);
  Display.displayClear();

  delay(100);
}

void printRoomHumidity(void)
{
  // Read humidity from the DHT sensor

  int humidity = dht.getHumidity();

  // Check if reading failed
  if (isnan(humidity))
  {
    Serial.println("Failed to read humidity from DHT sensor!");
    return;
  }

  // Print the humidity value to the serial monitor
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println("%");

  char room_humidity[10];
  sprintf(room_humidity, "%d%%", humidity);
  Display.displayClear();
  Display.displayText("room humidity", PA_LEFT, TEXT_SPEED, 0, PA_SCROLL_LEFT);
  while (!Display.displayAnimate() && !interrupt_triggered)
  {
    delay(100);
  }

  waitMilis(500);
  Serial.println(room_humidity);
  Display.setTextAlignment(PA_CENTER);
  Display.print(room_humidity);
  waitMilis(5000);
  Display.displayClear();

  delay(100);
}

ICACHE_RAM_ATTR void handleInterrupt()
{
  button_time = millis();
  if (button_time - last_button_time > 500)
  {
    interrupt_triggered = true;
    Serial.println("Interrupt triggered!");
    last_button_time = button_time;
  }
}

void switchCaseFunction()
{
  screen = screen + 1;
  Serial.print("screen value is ");
  Serial.println(screen);
  switch (screen)
  {
  case 1:
    // Serial.println("Showing clock");
    printTime();

    break;
  case 2:
    // Serial.println("Showing local temperature and humidity");
    printTemeperature(doc);
    printHumidity(doc);

    break;
  case 3:
    // Serial.println("Showing room temperature and humidity");
    screen = 0;
    printRoomTemperature();
    printRoomHumidity();
    break;
  default:
    // Serial.println("Invalid input screen is deleted");
    screen = 0;
    break;
  }
}

void printSplashScreen()
{
  
  Display.displayText("Cool Cool Cool", PA_LEFT, 100, 500, PA_SCROLL_LEFT);
  while (!Display.displayAnimate() && !interrupt_triggered)
  {
    delay(10);
  }
  Display.displayClear();

  Display.displayText("Sery Designs", PA_LEFT, 100, 500, PA_SCROLL_LEFT);
  while (!Display.displayAnimate() && !interrupt_triggered)
  {
    delay(10);
  }
  delay(1000);
  Display.setTextAlignment(PA_CENTER);
  Display.print(".:.:.:.:.");

  return;
}

void waitMilis(unsigned long milistowait)
{
  milistowait = milistowait + millis();
  unsigned long currentMillisWaited = millis();
  // check if the interval has elapsed
  while (currentMillisWaited < milistowait && !interrupt_triggered)
  {
    // Serial.println("waiting");
    currentMillisWaited = millis();
    delay(100);
  }
}
