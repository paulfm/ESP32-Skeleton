#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ---------------------------------------------------------
// Wi-Fi credentials
// ---------------------------------------------------------

const char* WIFI_SSID = "Myers24";
const char* WIFI_PASSWORD = "ABBAC1024F";

// ---------------------------------------------------------
// Output definitions
// ---------------------------------------------------------

struct OutputControl
{
    const char* name;
    const char* route;
    uint8_t pin;
    bool isOn;
};

OutputControl outputs[] =
{
    {"Onboard LED", "onboard", 8, false},
    {"Skeleton Body - IO0", "io0", 0, false},
    {"Skeleton Eyes - IO1", "io1", 1, false},
    {"Skeleton Heart - IO3", "io3", 3, false}
};

// ---------------------------------------------------------
// Skeleton heartbeat settings
// ---------------------------------------------------------

constexpr uint8_t HEART_PIN = 3;
constexpr uint8_t HEART_PWM_CHANNEL = 0;
constexpr uint32_t HEART_PWM_FREQUENCY = 5000;
constexpr uint8_t HEART_PWM_RESOLUTION = 8;

unsigned long heartbeatStartTime = 0;

constexpr size_t OUTPUT_COUNT =
    sizeof(outputs) / sizeof(outputs[0]);

WebServer server(80);

// ---------------------------------------------------------
// Skeleton eye-fade settings
// ---------------------------------------------------------

constexpr uint8_t EYES_PIN = 1;
constexpr uint8_t EYES_PWM_CHANNEL = 1;
constexpr uint32_t EYES_PWM_FREQUENCY = 5000;
constexpr uint8_t EYES_PWM_RESOLUTION = 8;

constexpr unsigned long EYES_FADE_TIME = 5000;
constexpr unsigned long EYES_CYCLE_TIME =
    EYES_FADE_TIME * 2;

unsigned long eyesFadeStartTime = 0;

// ---------------------------------------------------------
// Output control
// ---------------------------------------------------------
uint8_t fadeBetween(
    unsigned long position,
    unsigned long segmentStart,
    unsigned long segmentEnd,
    uint8_t startingBrightness,
    uint8_t endingBrightness
)
{
    if (segmentEnd <= segmentStart)
    {
        return endingBrightness;
    }

    float progress =
        static_cast<float>(position - segmentStart) /
        static_cast<float>(segmentEnd - segmentStart);

    return startingBrightness +
        static_cast<int>(
            progress *
            (endingBrightness - startingBrightness)
        );
}

void updateHeartbeat()
{
    // outputs[3] is the IO3 skeleton-heart output.
    if (!outputs[3].isOn)
    {
        ledcWrite(HEART_PWM_CHANNEL, 0);
        return;
    }

    // One complete heartbeat cycle lasts 1.2 seconds.
    unsigned long cyclePosition =
        (millis() - heartbeatStartTime) % 1200;

    uint8_t brightness = 0;

    if (cyclePosition < 120)
    {
        // First strong pulse: rapidly brighten.
        brightness = fadeBetween(
            cyclePosition,
            0,
            120,
            0,
            255
        );
    }
    else if (cyclePosition < 260)
    {
        // First pulse fades.
        brightness = fadeBetween(
            cyclePosition,
            120,
            260,
            255,
            20
        );
    }
    else if (cyclePosition < 380)
    {
        // Second, slightly weaker pulse.
        brightness = fadeBetween(
            cyclePosition,
            260,
            380,
            20,
            180
        );
    }
    else if (cyclePosition < 540)
    {
        // Second pulse fades completely.
        brightness = fadeBetween(
            cyclePosition,
            380,
            540,
            180,
            0
        );
    }
    else
    {
        // Rest period before the next heartbeat.
        brightness = 0;
    }

    ledcWrite(HEART_PWM_CHANNEL, brightness);
}

void updateEyes()
{
    // outputs[2] is the GPIO1 skeleton-eyes output.
    if (!outputs[2].isOn)
    {
        ledcWrite(EYES_PWM_CHANNEL, 0);
        return;
    }

    unsigned long cyclePosition =
        (millis() - eyesFadeStartTime) %
        EYES_CYCLE_TIME;

    uint8_t brightness;

    if (cyclePosition < EYES_FADE_TIME)
    {
        // Slowly brighten from off to full brightness.
        brightness = fadeBetween(
            cyclePosition,
            0,
            EYES_FADE_TIME,
            0,
            255
        );
    }
    else
    {
        // Slowly fade from full brightness back to off.
        brightness = fadeBetween(
            cyclePosition,
            EYES_FADE_TIME,
            EYES_CYCLE_TIME,
            255,
            0
        );
    }

    ledcWrite(EYES_PWM_CHANNEL, brightness);
}

void setOutput(size_t index, bool turnOn)
{
    if (index >= OUTPUT_COUNT)
    {
        return;
    }

    outputs[index].isOn = turnOn;

    if (outputs[index].pin == HEART_PIN)
    {
        if (turnOn)
        {
            heartbeatStartTime = millis();
        }

        ledcWrite(HEART_PWM_CHANNEL, 0);
    }
    else if (outputs[index].pin == EYES_PIN)
    {
        if (turnOn)
        {
            eyesFadeStartTime = millis();
        }

        ledcWrite(EYES_PWM_CHANNEL, 0);
    }
    else
    {
        // GPIO8 and GPIO0 are ordinary active-high outputs.
        digitalWrite(
            outputs[index].pin,
            turnOn ? HIGH : LOW
        );
    }

    Serial.print(outputs[index].name);
    Serial.print(" changed to: ");
    Serial.println(turnOn ? "ON" : "OFF");
}

void toggleOutput(size_t index)
{
    if (index >= OUTPUT_COUNT)
    {
        return;
    }

    setOutput(index, !outputs[index].isOn);
}

// ---------------------------------------------------------
// Formatting functions
// ---------------------------------------------------------

String formatUptime()
{
    unsigned long totalSeconds = millis() / 1000;

    unsigned long days = totalSeconds / 86400;
    unsigned long hours = (totalSeconds % 86400) / 3600;
    unsigned long minutes = (totalSeconds % 3600) / 60;
    unsigned long seconds = totalSeconds % 60;

    String uptime;

    if (days > 0)
    {
        uptime += String(days) + " days, ";
    }

    uptime += String(hours) + " hours, ";
    uptime += String(minutes) + " minutes, ";
    uptime += String(seconds) + " seconds";

    return uptime;
}

String signalQuality(int rssi)
{
    if (rssi >= -50)
    {
        return "Excellent";
    }
    else if (rssi >= -60)
    {
        return "Very good";
    }
    else if (rssi >= -70)
    {
        return "Good";
    }
    else if (rssi >= -80)
    {
        return "Weak";
    }

    return "Very weak";
}

// ---------------------------------------------------------
// Browser responses
// ---------------------------------------------------------

void redirectHome()
{
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
}

void toggleOnboard()
{
    toggleOutput(0);
    redirectHome();
}

void toggleIO0()
{
    toggleOutput(1);
    redirectHome();
}

void toggleIO1()
{
    toggleOutput(2);
    redirectHome();
}

void toggleIO3()
{
    toggleOutput(3);
    redirectHome();
}

// ---------------------------------------------------------
// Main webpage
// ---------------------------------------------------------

void showHomePage()
{
    int rssi = WiFi.RSSI();

    String html;
    html.reserve(6500);

    html += R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">

    <meta name="viewport"
          content="width=device-width, initial-scale=1.0">

    <meta http-equiv="refresh" content="5">

    <title>ESP32-C3 Control Panel</title>

    <style>
        * {
            box-sizing: border-box;
        }

        body {
            margin: 0;
            padding: 25px;
            background: #eef2f6;
            color: #263238;
            font-family: Arial, Helvetica, sans-serif;
        }

        .card {
            width: 100%;
            max-width: 680px;
            margin: 35px auto;
            padding: 30px;
            background: white;
            border-radius: 14px;
            box-shadow: 0 8px 24px rgba(0, 0, 0, 0.12);
        }

        h1 {
            margin-top: 0;
            color: #00695c;
        }

        h2 {
            margin-top: 30px;
            color: #37474f;
        }

        .online-status {
            display: inline-block;
            margin-bottom: 20px;
            padding: 7px 12px;
            color: white;
            background: #2e7d32;
            border-radius: 20px;
            font-weight: bold;
        }

        .row {
            display: flex;
            justify-content: space-between;
            gap: 20px;
            padding: 12px 0;
            border-bottom: 1px solid #e0e0e0;
        }

        .label {
            font-weight: bold;
        }

        .value {
            text-align: right;
            overflow-wrap: anywhere;
        }

        .output-grid {
            display: grid;
            grid-template-columns:
                repeat(auto-fit, minmax(245px, 1fr));
            gap: 15px;
        }

        .output-panel {
            padding: 20px;
            background: #f5f7f9;
            border: 1px solid #dfe5e8;
            border-radius: 10px;
            text-align: center;
        }

        .output-name {
            margin-bottom: 8px;
            font-size: 1.05rem;
            font-weight: bold;
        }

        .pin-number {
            margin-bottom: 14px;
            color: #607d8b;
            font-size: 0.9rem;
        }

        .output-state {
            margin-bottom: 15px;
            font-size: 1.1rem;
            font-weight: bold;
        }

        .state-on {
            color: #2e7d32;
        }

        .state-off {
            color: #c62828;
        }

        button {
            width: 140px;
            padding: 12px 18px;
            border: none;
            border-radius: 7px;
            color: white;
            font-size: 1rem;
            font-weight: bold;
            cursor: pointer;
        }

        .turn-on {
            background: #2e7d32;
        }

        .turn-off {
            background: #c62828;
        }

        button:hover {
            opacity: 0.85;
        }

        .footer {
            margin-top: 24px;
            color: #607d8b;
            font-size: 0.9rem;
            text-align: center;
        }

        @media (max-width: 500px) {
            body {
                padding: 12px;
            }

            .card {
                margin: 12px auto;
                padding: 20px;
            }

            .row {
                flex-direction: column;
                gap: 4px;
            }

            .value {
                text-align: left;
            }
        }
    </style>
</head>

<body>
    <div class="card">
        <h1>ESP32-C3 SuperMini</h1>

        <div class="online-status">
            Online
        </div>
)rawliteral";

    html += "<div class='row'>";
    html += "<span class='label'>Network</span>";
    html += "<span class='value'>" +
            WiFi.SSID() +
            "</span></div>";

    html += "<div class='row'>";
    html += "<span class='label'>IP address</span>";
    html += "<span class='value'>" +
            WiFi.localIP().toString() +
            "</span></div>";

    html += "<div class='row'>";
    html += "<span class='label'>Signal</span>";
    html += "<span class='value'>" +
            String(rssi) +
            " dBm (" +
            signalQuality(rssi) +
            ")</span></div>";

    html += "<div class='row'>";
    html += "<span class='label'>Wi-Fi channel</span>";
    html += "<span class='value'>" +
            String(WiFi.channel()) +
            "</span></div>";

    html += "<div class='row'>";
    html += "<span class='label'>MAC address</span>";
    html += "<span class='value'>" +
            WiFi.macAddress() +
            "</span></div>";

    html += "<div class='row'>";
    html += "<span class='label'>Uptime</span>";
    html += "<span class='value'>" +
            formatUptime() +
            "</span></div>";

    html += "<h2>LED Controls</h2>";
    html += "<div class='output-grid'>";

    for (size_t i = 0; i < OUTPUT_COUNT; i++)
    {
        html += "<div class='output-panel'>";

        html += "<div class='output-name'>";
        html += outputs[i].name;
        html += "</div>";

        html += "<div class='pin-number'>GPIO ";
        html += String(outputs[i].pin);
        html += "</div>";

        if (outputs[i].isOn)
        {
            html +=
                "<div class='output-state state-on'>ON</div>";
        }
        else
        {
            html +=
                "<div class='output-state state-off'>OFF</div>";
        }

        html += "<form action='/toggle/";
        html += outputs[i].route;
        html += "' method='get'>";

        if (outputs[i].isOn)
        {
            html +=
                "<button class='turn-off' type='submit'>"
                "Turn Off"
                "</button>";
        }
        else
        {
            html +=
                "<button class='turn-on' type='submit'>"
                "Turn On"
                "</button>";
        }

        html += "</form>";
        html += "</div>";
    }

    html += R"rawliteral(
        </div>

        <div class="footer">
            This page refreshes every five seconds.
        </div>
    </div>
</body>
</html>
)rawliteral";

    server.send(200, "text/html", html);
}

void showNotFound()
{
    server.send(404, "text/plain", "Page not found");
}

// ---------------------------------------------------------
// Wi-Fi
// ---------------------------------------------------------

void connectToWiFi()
{
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);

    // Set the network hostname before connecting.
    WiFi.setHostname("skeleton-heart");

    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("Wi-Fi connected.");

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.println("Starting mDNS...");

    if (MDNS.begin("skeleton-heart"))
    {
        MDNS.addService("http", "tcp", 80);

        Serial.println("mDNS started successfully.");
        Serial.println(
            "Open: http://skeleton-heart.local/"
        );
    }
    else
    {
        Serial.println("Unable to start mDNS.");

        Serial.print("Use: http://");
        Serial.print(WiFi.localIP());
        Serial.println("/");
    }
}

// ---------------------------------------------------------
// Setup
// ---------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("Firmware: Skeleton Heart with mDNS - Version 2");
    Serial.println();
    Serial.println("ESP32-C3 Multi-LED Web Control");
    Serial.println("=============================");

// Configure the ordinary digital outputs.
// GPIO1 and GPIO3 are controlled by PWM.
for (size_t i = 0; i < OUTPUT_COUNT; i++)
{
    if (
        outputs[i].pin != HEART_PIN &&
        outputs[i].pin != EYES_PIN
    )
    {
        pinMode(outputs[i].pin, OUTPUT);
    }
}

// Configure GPIO3 for the heartbeat PWM animation.
ledcSetup(
    HEART_PWM_CHANNEL,
    HEART_PWM_FREQUENCY,
    HEART_PWM_RESOLUTION
);

ledcAttachPin(
    HEART_PIN,
    HEART_PWM_CHANNEL
);

// Configure GPIO1 for the eye-fade PWM animation.
ledcSetup(
    EYES_PWM_CHANNEL,
    EYES_PWM_FREQUENCY,
    EYES_PWM_RESOLUTION
);

ledcAttachPin(
    EYES_PIN,
    EYES_PWM_CHANNEL
);

// Establish the desired power-up states.
setOutput(0, false);  // Onboard LED off
setOutput(1, true);   // Skeleton body on steadily
setOutput(2, true);   // Eye fade starts automatically
setOutput(3, true);   // Heartbeat starts automatically

    connectToWiFi();

    server.on("/", HTTP_GET, showHomePage);

    server.on(
        "/toggle/onboard",
        HTTP_GET,
        toggleOnboard
    );

    server.on(
        "/toggle/io0",
        HTTP_GET,
        toggleIO0
    );

    server.on(
        "/toggle/io1",
        HTTP_GET,
        toggleIO1
    );

    server.on(
        "/toggle/io3",
        HTTP_GET,
        toggleIO3
    );

    server.onNotFound(showNotFound);

    server.begin();

    Serial.println("Web server started.");

    Serial.print("Open http://");
    Serial.print(WiFi.localIP());
    Serial.println("/");
}

// ---------------------------------------------------------
// Main loop
// ---------------------------------------------------------

void loop()
{
    server.handleClient();
    updateHeartbeat();
    updateEyes();
    
    static unsigned long lastStatusTime = 0;

    if (millis() - lastStatusTime >= 5000)
    {
        lastStatusTime = millis();

        Serial.println();
        Serial.println("Web server is running.");

        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("Wi-Fi status: Connected");

            Serial.print("IP address: http://");
            Serial.print(WiFi.localIP());
            Serial.println("/");

            Serial.print("Signal strength: ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");

            for (size_t i = 0; i < OUTPUT_COUNT; i++)
            {
                Serial.print(outputs[i].name);
                Serial.print(": ");
                Serial.println(
                    outputs[i].isOn ? "ON" : "OFF"
                );
            }
        }
        else
        {
            Serial.println("Wi-Fi status: Disconnected");
        }
    }

    delay(2);
}