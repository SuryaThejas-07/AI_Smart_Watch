#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MPU6050.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <DHT.h>

// OLED SPI pin mapping (for ESP8266)
#define OLED_MOSI D7  // D1 on OLED → SDA
#define OLED_CLK D5   // D0 on OLED → SCL
#define OLED_DC D3    // DC pin
#define OLED_CS D8    // CS pin
#define OLED_RESET D4 // RES pin

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// --- Define DHT Sensor Pin and Type ---
#define DHTPIN D6      // The pin you connected the DHT sensor to
#define DHTTYPE DHT11  // Or DHT22 if you are using that one

// --- NEW: Define IR Sensor Pin ---
#define IR_SENSOR_PIN A0  // The analog pin you connected the IR sensor to

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);

PulseOximeter pox;
MPU6050 mpu;
DHT dht(DHTPIN, DHTTYPE); // Initialize the DHT sensor object

// WiFi credentials
const char *ssid = "Denis Wifi";
const char *password = "Dru143Mee";

// Backend server
const char *serverDataEndpoint = "http://10.97.207.133:3000/data";
// Backend server for AI queries
const char *aiServer = "http://10.97.207.133:3000/ask-ai";

// NTP settings
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

uint32_t tsLastReport = 0;
bool beatDetected = false;
unsigned long lastStepTime = 0;
const unsigned long stepDebounceTime = 300;
int stepCount = 0;
String lastAIResponse = "";

// --- NEW: Global variables for new hydration logic ---
String hydrationAdvice = "Checking...";
int hydrationScore = 0;
float effectiveBPM = 0.0;

// --- MODIFIED: Replaced 'obstacleDetected' with stability logic ---
bool isWorn = true; // Start by assuming it's worn
int notWornCounter = 0;
const int NOT_WORN_THRESHOLD = 5; // Needs 5 'not worn' readings in a row

void onBeatDetected()
{
  Serial.println("Beat Detected!");
  beatDetected = true;
}

const unsigned char heartBitmap[] PROGMEM = {
    // ... (bitmap data unchanged) ...
    0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x18, 0x00, 0x0f, 0x00, 0x7f, 0x00, 0x3f, 0xf9, 0xff, 0xc0,
    0x7f, 0xf9, 0xff, 0xc0, 0x7f, 0xff, 0xff, 0xe0, 0x7f, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xf0,
    0xff, 0xf7, 0xff, 0xf0, 0xff, 0xe7, 0xff, 0xf0, 0xff, 0xe7, 0xff, 0xf0, 0x7f, 0xdb, 0xff, 0xe0,
    0x7f, 0x9b, 0xff, 0xe0, 0x00, 0x3b, 0xc0, 0x00, 0x3f, 0xf9, 0x9f, 0xc0, 0x3f, 0xfd, 0xbf, 0xc0,
    0x1f, 0xfd, 0xbf, 0x80, 0x0f, 0xfd, 0x7f, 0x00, 0x07, 0xfe, 0x7e, 0x00, 0x03, 0xfe, 0xfc, 0x00,
    0x01, 0xff, 0xf8, 0x00, 0x00, 0xff, 0xf0, 0x00, 0x00, 0x7f, 0xe0, 0x00, 0x00, 0x3f, 0xc0, 0x00,
    0x00, 0x0f, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  dht.begin(); // Initialize the DHT sensor
  
  // Initialize the IR Sensor pin
  pinMode(IR_SENSOR_PIN, INPUT);

  // --- THIS IS THE CORRECTED LINE ---
  if (!oled.begin(SSD1306_SWITCHCAPVCC))
  {
    Serial.println("SSD1306 allocation failed");
    for (;;)
      ;
  }
  
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed!");
    while (1);
  }

  // Added the missing initialization for the MAX30100
  if (!pox.begin()) {
    Serial.println("MAX30100 failed to initialize!");
    while(1);
  }
  pox.setOnBeatDetectedCallback(onBeatDetected);

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.println("Connecting WiFi...");
  oled.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void detectStep()
{
  // ... (function unchanged) ...
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  float ax_g = ax / 16384.0;
  float ay_g = ay / 16384.0;
  float az_g = az / 16384.0;
  float accMagnitude = sqrt(ax_g * ax_g + ay_g * ay_g + az_g * az_g);
  static float prevMagnitude = 1.0;
  static bool stepPossible = false;
  if ((accMagnitude - prevMagnitude) > 0.25)
  {
    stepPossible = true;
  }
  if (stepPossible && (prevMagnitude - accMagnitude) > 0.25)
  {
    if (millis() - lastStepTime > stepDebounceTime)
    {
      stepCount++;
      lastStepTime = millis();
    }
    stepPossible = false;
  }
  prevMagnitude = accMagnitude;
}

String getFormattedTime()
{
  // ... (function unchanged) ...
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
    return "Time N/A";
  char buffer[16];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

String getFormattedDate()
{
  // ... (function unchanged) ...
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
    return "Date N/A";
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d %b %Y", &timeinfo);
  return String(buffer);
}

void askAI(String query)
{
  // ... (function unchanged) ...
}


// --- MODIFIED: Updated function to send new hydration score ---
void sendSensorDataToAI(float hr, float spo2, float temp, float humidity, float pressure, int steps, int hydraScore, String hydraAdvice, bool obstacle, String timeStr)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    WiFiClient client;
    http.begin(client, serverDataEndpoint);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<512> doc;
    doc["heartRate"] = hr;
    doc["spo2"] = spo2;
    doc["temperature"] = temp;
    doc["humidity"] = humidity;
    doc["pressure"] = pressure;
    doc["steps"] = steps;
    doc["hydrationScore"] = hydraScore; // NEW
    doc["hydrationAdvice"] = hydraAdvice; // MODIFIED
    doc["obstacle"] = obstacle;      // This now represents 'wear status'
    doc["time"] = timeStr;

    String requestBody;
    serializeJson(doc, requestBody);

    int code = http.POST(requestBody);
    Serial.print("Data POST status: ");
    Serial.println(code);

    http.end();
  }
  else
  {
    Serial.println("WiFi not connected, skipping POST");
  }
}

// --- NEW 'void loop()' FUNCTION WITH UPDATED DISPLAY LAYOUT ---
void loop()
{
  pox.update();
  detectStep();

  static unsigned long lastDisplayUpdate = 0;
  static unsigned long lastPostUpdate = 0;
  static int lastStepCount = 0; // To check if user is moving
  unsigned long currentMillis = millis();
  bool moving = false; // --- NEW: 'moving' is now visible to display logic ---

  float bpm = pox.getHeartRate();
  float spo2 = pox.getSpO2();

  // --- Read from DHT Sensor ---
  float temp = dht.readTemperature(); // Reads temperature in Celsius
  float humidity = dht.readHumidity();

  if (isnan(temp) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    temp = 25.0;     // Use a default value if read fails
    humidity = 50.0; // Use a default value if read fails
  }
  
  // --- MODIFIED: Read from IR Sensor (with stability logic) ---
  int irValue = analogRead(IR_SENSOR_PIN);
  if (irValue < 500) { 
    // Device is worn
    isWorn = true;
    notWornCounter = 0; // Reset the counter
  } else {
    // Device might not be worn
    notWornCounter++; // Increment the counter
    if (notWornCounter > NOT_WORN_THRESHOLD) {
      isWorn = false; // Only set to 'not worn' after 5 bad readings
    }
  }

  // fallback for sensors you don't have
  float pressure = random(950, 1050); // Kept your random pressure
  
  // --- MODIFIED: Use new 'isWorn' variable ---
  // Use default values only if the device is worn and sensors fail
  if (isWorn) {
    if (bpm == 0.0)
      bpm = random(60, 100);
    if (spo2 == 0.0)
      spo2 = random(95, 100);
  }


  // =======================================================
  // --- NEW HYDRATION LOGIC (Implementations #1, 2, 3, 4) ---
  // =======================================================

  // --- MODIFIED: Use new 'isWorn' variable ---
  // 4. Integrate IR Sensor for Context Awareness
  if (!isWorn) {
    hydrationAdvice = "Hydration: --"; // --- MODIFIED: Advice no longer shows "not worn"
    hydrationScore = 0; // Reset score
    bpm = 0.0;          // Clear stale data
    spo2 = 0.0;         // Clear stale data
  } 
  else {
    // --- Device IS worn, proceed with logic ---

    moving = (stepCount != lastStepCount); // --- MODIFIED: Assign to loop-scoped variable
    lastStepCount = stepCount;

    // 1. Integrate Motion Filtering
    effectiveBPM = bpm; // Using raw BPM for this example
    // This logic ensures only *resting* high heart rate is flagged
    bool highRestingHR = (effectiveBPM > 100 && !moving);

    // 2. Add Environmental Load Index
    float heatStressIndex = (0.8 * temp) + (humidity * 0.1) - ((pressure - 1013) * 0.05);
    int envRiskLevel = 0;
    
    if (heatStressIndex > 35) envRiskLevel = 2; // High risk
    else if (heatStressIndex > 30) envRiskLevel = 1; // Mild risk
    else envRiskLevel = 0; // Normal

    // 3. Combine All Factors — Weighted Risk Model
    hydrationScore = 0; // Reset score
    if (envRiskLevel == 2) hydrationScore += 2;
    if (highRestingHR) hydrationScore += 2;
    if (moving && temp > 32) hydrationScore += 1; // Sweating loss factor

    // Convert to advice (Shortened strings to fit OLED)
    if (hydrationScore >= 3)
      hydrationAdvice = "High Risk: Drink!";
    else if (hydrationScore == 2)
      hydrationAdvice = "Mild Risk: Hydrate";
    else
      hydrationAdvice = "Hydration OK";
  }
  // --- End of New Hydration Logic ---


  // --- Display Update Logic ---
  if (currentMillis - lastDisplayUpdate > 1000)
  {
    lastDisplayUpdate = currentMillis;

    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    
    oled.setCursor(0, 0);
    oled.print("HR: ");
    // --- MODIFIED: Use new 'isWorn' variable ---
    // Only print HR/SpO2 if device is worn
    if (isWorn) {
      oled.print(bpm, 1);
      oled.print(" SpO2: ");
      oled.print(spo2, 1);
      oled.print("%");
    } else {
      oled.print("-- SpO2: --");
    }

    oled.setCursor(0, 10);
    oled.print("Temp: ");
    oled.print(temp, 1);
    oled.print("C Hum: ");
    oled.print(humidity, 0);
    oled.print("%");
    
    // --- MODIFIED: y=20 (Steps + Score) ---
    oled.setCursor(0, 20);
    oled.print("Steps: ");
    oled.print(stepCount);
    oled.print(" Score: ");
    if (isWorn) {
        oled.print(hydrationScore);
    } else {
        oled.print("--");
    }
    
    // --- MODIFIED: y=32 (Hydration Advice) ---
    oled.setCursor(0, 32);
    oled.setTextSize(1); 
    if (hydrationScore >= 3) { // Special formatting for high risk
       // You could add oled.invertDisplay(true) here for alerts
    }
    oled.print(hydrationAdvice);
    // oled.invertDisplay(false); // Turn off inversion if you used it
    
    // --- MODIFIED: y=42 (Device Status) ---
    oled.setCursor(0, 42);
    if (isWorn) {
        if (moving) {
            oled.print("Motion: Detected");
        } else {
            oled.print("Motion: At Rest");
        }
    } else {
        oled.print("Status: Not Worn");
    }

    oled.setCursor(0, 53);
    oled.print(getFormattedTime());
    
    oled.display();
  }

  // --- Data POST Logic ---
  if (currentMillis - lastPostUpdate > 1000)
  {
    lastPostUpdate = currentMillis;
    // --- MODIFIED: Send all new data (including score and new 'isWorn' variable) to the server ---
    sendSensorDataToAI(bpm, spo2, temp, humidity, pressure, stepCount, hydrationScore, hydrationAdvice, isWorn, getFormattedTime());
  }
}