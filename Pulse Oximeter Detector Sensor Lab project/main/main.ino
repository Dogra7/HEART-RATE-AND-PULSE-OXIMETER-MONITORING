#include <Wire.h>
#include <WiFi.h> 
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "heartRate.h"
#include <ESPAsyncWebServer.h>

MAX30105 particleSensor;
#define MAX_BRIGHTNESS 255

int temp = 0;
uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data
int32_t bufferLength; //data length
int32_t spo2; //SPO2 value
int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
int32_t heartRate; //heart rate value calcualated as per Maxim's algorithm
int8_t validHeartRate; //indicator to show if the heart rate calculation is valid

byte pulseLED = 2; //onboard led on esp32 nodemcu
byte readLED = 19; //Blinks with each data read 

long lastBeat = 0; //Time at which the last beat occurred

float beatsPerMinute; //stores the BPM as per custom algorithm
int beatAvg = 0, sp02Avg = 0; //stores the average BPM and SPO2 
float ledBlinkFreq; //stores the frequency to blink the pulseLED

const char* ssid = "Mhatre";
const char* password = "Shubham@8765";

AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Heart Rate and SpO2 Monitoring</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 0;
            background-image: url('h.jpg'); /* Replace 'your_background_image.jpg' with the path to your background image */
            background-size: cover;
            background-position: center;
            color: #333;
        }
        .container {
            width: 80%;
            margin: 50px auto;
            text-align: center;
            background-color: rgba(106, 255, 255, 0.7); 
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
        }
        h1, h2 {
            margin-top: 0;
        }
        .graph {
            margin-top: 50px;
            display: flex;
            justify-content: center;
            align-items: center;
        }
        .graph img {
            max-width: 100%;
            height: auto;
            border-radius: 5px;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
        }
        .data-container {
            display: flex;
            justify-content: space-around;
            margin-top: 30px;
        }
        .data-box {
            background-color: #c4ffff;
            padding: 15px 20px;
            border-radius: 5px;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Heart Rate and SpO2 Monitoring</h1>
        <div class="data-container">
            <div class="data-box">
                <h2>SpO2</h2>
                <span id="spo2">-</span>
            </div>
            <div class="data-box">
                <h2>Heart Rate</h2>
                <span id="avgHeartRate">-</span>
            </div>
        </div>
        <div class="graph">
            <canvas id="myChart"></canvas>
        </div>
    </div>

    <script>
        // Initialize empty chart
        let myChart;

function fetchData() {
    fetch('/data') // Fetch data from '/data' endpoint
    .then(response => response.json()) // Parse JSON response
    .then(data => {
        console.log('Received data:', data); // Log received data
        // Update heart rate, SpO2, and average values
        document.getElementById('spo2').innerText = data.spo2 + "%";
        document.getElementById('avgHeartRate').innerText = data.beatAvg + " BPM";
        // Update or create chart
        updateChart(data);
    })
    .catch(error => console.error('Error fetching data:', error));
}

// Function to update or create chart
function updateChart(data) {
    if (!myChart) {
        const ctx = document.getElementById('myChart').getContext('2d');
        myChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [...Array(100).keys()],
                datasets: [{
                    label: 'BPM',
                    data: [],
                    borderColor: 'blue',
                    borderWidth: 1,
                    fill: false
                }, {
                    label: 'SPO2',
                    data: [],
                    borderColor: 'red',
                    borderWidth: 1,
                    fill: false
                }]
            },
            options: {
                scales: {
                    y: {
                        beginAtZero: true
                    }
                }
            }
        });
    }

    // Update existing chart data
    myChart.data.datasets[0].data.push(data.beatAvg);
    myChart.data.datasets[1].data.push(data.spo2);
    myChart.update();
}


        // Fetch data every 2 seconds
        setInterval(fetchData, 2000);
    </script>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

</body>
</html>


)rawliteral";


void setup() {

  ledcSetup(0, 0, 8); // PWM Channel = 0, Initial PWM Frequency = 0Hz, Resolution = 8 bits
  ledcAttachPin(pulseLED, 0); //attach pulseLED pin to PWM Channel 0
  ledcWrite(0, 255); //set PWM Channel Duty Cycle to 255
  
  Serial.begin(115200);
  delay(1000);

    // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Print ESP32 IP address
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

    // Route to provide heart rate and SpO2 data
server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json =  "{\"spo2\":" + String(spo2) + ",\"beatAvg\":" + String(beatAvg) + "}";
    Serial.println("Sending data to client: " + json); // Print data being sent    
    request->send(200, "application/json", json);
});
  // Start server
  server.begin();
  Serial.println("HTTP server started");

    // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println(F("MAX30105 was not found. Please check wiring/power."));
    while (1);
  }

  /*The following parameters should be tuned to get the best readings for IR and RED LED. 
   *The perfect values varies depending on your power consumption required, accuracy, ambient light, sensor mounting, etc. 
   *Refer Maxim App Notes to understand how to change these values
   */
  byte ledBrightness = 50; //Options: 0=Off to 255=50mA
  byte sampleAverage = 1; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 69; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384
  
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings
}

void loop() {
  bufferLength = 100; //buffer length of 100 stores 4 seconds of samples running at 25sps

  //read the first 100 samples, and determine the signal range
  for (byte i = 0; i < bufferLength; i++) {
    while (particleSensor.available() == false) //do we have new data?
      particleSensor.check(); //Check the sensor for new data

    redBuffer[i] = particleSensor.getIR();
    irBuffer[i] = particleSensor.getRed();
    particleSensor.nextSample(); //We're finished with this sample so move to next sample
  }
  

  // Print headers for Serial Plotter
  Serial.println("BPM,SPO2");

  // Continuously taking samples from MAX30102.  Heart rate and SpO2 are calculated every 1 second
  while (1) {
    // dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
    for (byte i = 25; i < 100; i++) {
      redBuffer[i - 25] = redBuffer[i];
      irBuffer[i - 25] = irBuffer[i];
    }

    // take 25 sets of samples before calculating the heart rate.
    for (byte i = 75; i < 100; i++) {
      while (particleSensor.available() == false) //do we have new data?
        particleSensor.check(); //Check the sensor for new data

      digitalWrite(readLED, !digitalRead(readLED)); //Blink onboard LED with every data read

      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR();
      particleSensor.nextSample(); //We're finished with this sample so move to next sample

      long irValue = irBuffer[i];

      if (irValue < 50000) {
        // If irValue is less than 50000, set spo2 to 0 and continue to the next iteration
        spo2 = 0;
        sp02Avg = 0;
        beatAvg = 0;
        beatsPerMinute = 0;
        continue;
      }

      // Calculate BPM independent of Maxim Algorithm.
      if (checkForBeat(irValue) == true && irValue > 50000) {
        // We sensed a beat!
        long delta = millis() - lastBeat;
        lastBeat = millis();

        beatsPerMinute = 60 / (delta / 1000.0);
        beatAvg = (beatAvg + beatsPerMinute) / 2;

        if (beatAvg != 0)
          ledBlinkFreq = (float)(60.0 / beatAvg);
        else
          ledBlinkFreq = 0;
        ledcWriteTone(0, ledBlinkFreq);
      }
      if (millis() - lastBeat > 10000) {
        beatsPerMinute = 0;
        beatAvg = (beatAvg + beatsPerMinute) / 2;

        if (beatAvg != 0)
          ledBlinkFreq = (float)(60.0 / beatAvg);
        else
          ledBlinkFreq = 0;
        ledcWriteTone(0, ledBlinkFreq);
      }
      // if (irValue < 50000) {
      //   beatAvg = 0;
      //   beatsPerMinute = 0;
      // }
    
      if (irValue < 50000) {
        Serial.print(0);
      } else {
        Serial.print(1);       
      }      
      Serial.print(","); // Separate with comma

      // Output BPM and SPO2 for Serial Plotter
      Serial.print(beatAvg); // Print BPM
      Serial.print(","); // Separate with comma
      if (spo2 <= 80) {
        Serial.println(0); // Print SPO2
      } else {
        Serial.println(spo2); // Print SPO2 average
      }
    }

    //After gathering 25 new samples recalculate HR and SP02
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

    //Calculates average SPO2 to display smooth transitions on Blynk App
    if (validSPO2 == 1 && spo2 < 100 && spo2 > 0) {
      sp02Avg = (sp02Avg + spo2) / 2;
    } else {
      spo2 = 0;
      sp02Avg = (sp02Avg + spo2) / 2;
    }
  }
}
