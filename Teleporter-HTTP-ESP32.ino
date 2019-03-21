const char VERSIONSTRING[] =
"Teleporter-HTTP-ESP32 version 1.0\r\n"
"<https://github.com/idzuna/Teleporter-HTTP-ESP32>\r\n";

/* Network Configurations */
const char BASESSID[] = "BASESSID";
const char PASSWORD[] = "PASSWORD";
const IPAddress BASEIPADDRESS(192, 168, 4, 224);
const IPAddress SUBNETMASK(255, 255, 255, 0);
const IPAddress DEFAULTGATEWAY(192, 168, 4, 1);
const uint16_t WEBAPIPORT = 80;
const uint16_t SERVERPORT = 8080;
const unsigned int MINIMUMINTERVAL = 200;

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

#define INPUT0      32
#define INPUT1      33
#define INPUT2      23
#define INPUT3      22
#define OUTPUT0     25
#define OUTPUT1     26
#define OUTPUT2     21
#define OUTPUT3     19
#define LED0        13
#define LED1        18
#define OUTINV01    27
#define OUTINV23    16
#define WIFIMODE    12
#define SSIDSEL     4
#define ROLESEL0    14
#define ROLESEL1    17
#define GROUPSEL    2

enum Role
{
    ROLE_CLIENT0,
    ROLE_SERVER,
    ROLE_CLIENT1,
    ROLE_SERVERWITHCLIENT1,
};
enum WifiMode{
    WIFIMODE_AP,
    WIFIMODE_STA,
};

Role g_role;
const char* g_roleString;
String g_serverUrl;
WebServer g_server(SERVERPORT);
WebServer g_webapi(WEBAPIPORT);
bool g_outputinv[4];
WifiMode g_wifiMode;
volatile bool g_falling[4];
bool g_clearFalling[4];
volatile unsigned int g_lastUpdate[2];
volatile unsigned int g_now;

void IRAM_ATTR onTimer()
{
    const unsigned int now = g_now + 1;
    g_now = now;
    digitalWrite(LED0, ((now - g_lastUpdate[0]) < 100) ? HIGH : LOW);
    if (g_role == ROLE_SERVERWITHCLIENT1) {
        digitalWrite(LED1, ((now - g_lastUpdate[1]) < 100) ? HIGH : LOW);
    }
}

void IRAM_ATTR onFall(void* arg)
{
    g_falling[(int)arg] = true;
}

String getStatusString()
{
    bool falling[4];
    for (int i = 0; i < 4; ++i) {
        falling[i] = g_falling[i];
        if (falling[i]) {
            g_clearFalling[i] = true;
        }
    }
    String status;
    status += "NAME=";
    status += g_roleString;
    status += "&INPUT0=";
    status += falling[0] ? 0 : digitalRead(INPUT0);
    status += "&INPUT1=";
    status += falling[1] ? 0 : digitalRead(INPUT1);
    status += "&INPUT2=";
    status += falling[2] ? 0 : digitalRead(INPUT2);
    status += "&INPUT3=";
    status += falling[3] ? 0 : digitalRead(INPUT3);
    status += "&TEMPERATURE=";
    status += String(temprature_sens_read(), 16);
    status += "&A0=";
    status += String(analogRead(0), 16);
    status += "&A3=";
    status += String(analogRead(3), 16);
    status += "&A6=";
    status += String(analogRead(6), 16);
    status += "&A7=";
    status += String(analogRead(7), 16);
    status += "&TIMESTAMP=";
    status += String(g_now, 16);
    status += "&LASTUPDATE0=";
    status += String(g_lastUpdate[0], 16);
    if (g_role == ROLE_SERVERWITHCLIENT1) {
        status += "&LASTUPDATE1=";
        status += String(g_lastUpdate[1], 16);
    }
    return status;
}

void setOutput(int port, int value)
{
    int pin;
    switch (port) {
    case 0:
        pin = OUTPUT0;
        break;
    case 1:
        pin = OUTPUT1;
        break;
    case 2:
        pin = OUTPUT2;
        break;
    case 3:
    default:
        pin = OUTPUT3;
        break;
    }
    digitalWrite(pin, ((value != 0) == g_outputinv[port]) ? LOW : HIGH);
}

void initializeServer()
{
    g_server.on("/", [](){
        /* read input, then write output */
        const String statusString = getStatusString();
        if (g_role == ROLE_SERVER) {
            const String name = g_server.arg("NAME");
            if (name == "CLIENT0") {
                for (int i = 0; i < 4; ++i) {
                    if (g_clearFalling[i]) {
                        g_clearFalling[i] = false;
                        g_falling[i] = false;
                    }
                }
                for (int i = 0; i < 4; ++i) {
                    const String input = g_server.arg(String("INPUT") + String(i));
                    if (input.length() > 0) {
                        setOutput(i, input.toInt());
                    }
                }
                g_lastUpdate[0] = g_now;
            }
        }
        else if (g_role == ROLE_SERVERWITHCLIENT1) {
            const String name = g_server.arg("NAME");
            if (name == "CLIENT0") {
                for (int i = 0; i < 2; ++i) {
                    if (g_clearFalling[i]) {
                        g_clearFalling[i] = false;
                        g_falling[i] = false;
                    }
                }
                for (int i = 0; i < 2; ++i) {
                    const String input = g_server.arg(String("INPUT") + String(i));
                    if (input.length() > 0) {
                        setOutput(i, input.toInt());
                    }
                }
                g_lastUpdate[0] = g_now;
            }
            else if (name == "CLIENT1") {
                for (int i = 2; i < 4; ++i) {
                    if (g_clearFalling[i]) {
                        g_clearFalling[i] = false;
                        g_falling[i] = false;
                    }
                }
                for (int i = 0; i < 2; ++i) {
                    const String input = g_server.arg(String("INPUT") + String(i));
                    if (input.length() > 0) {
                        setOutput(i + 2, input.toInt());
                    }
                }
                g_lastUpdate[1] = g_now;
            }
        }
        g_server.send(200, "text/plain", statusString);
    });
    g_server.begin();
}

void webapiTask(void* pvParameters)
{
    while (true) {
        g_webapi.handleClient();
        vTaskDelay(100);
    }
}

void initializeWebapi()
{
    g_webapi.on("/", [](){
        g_webapi.send(200, "text/plain", getStatusString());
    });
    g_webapi.begin();
    xTaskCreatePinnedToCore(webapiTask, "webapiTask", 8192, NULL, 1, NULL, 0);
}

void initializeTimer()
{
    hw_timer_t* timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 1000, true);
    timerAlarmEnable(timer);
}

void setup()
{
    Serial.begin(115200);
    Serial.println(VERSIONSTRING);

    pinMode(LED0, OUTPUT);
    pinMode(OUTPUT0, OUTPUT);
    pinMode(OUTPUT1, OUTPUT);
    pinMode(OUTPUT2, OUTPUT);
    pinMode(OUTPUT3, OUTPUT);
    pinMode(INPUT0, INPUT_PULLUP);
    pinMode(INPUT1, INPUT_PULLUP);
    pinMode(INPUT2, INPUT_PULLUP);
    pinMode(INPUT3, INPUT_PULLUP);
    pinMode(WIFIMODE, INPUT_PULLUP);
    pinMode(ROLESEL0, INPUT_PULLUP);
    pinMode(ROLESEL1, INPUT_PULLUP);

    pinMode(OUTINV01, INPUT_PULLUP);
    pinMode(OUTINV23, INPUT_PULLUP);
    pinMode(SSIDSEL, INPUT_PULLUP);
    pinMode(GROUPSEL, INPUT_PULLUP);
    delay(100);
    const int outinv01_pu = digitalRead(OUTINV01);
    const int outinv23_pu = digitalRead(OUTINV23);
    const int ssidsel_pu = digitalRead(SSIDSEL);
    const int groupsel_pu = digitalRead(GROUPSEL);

    pinMode(OUTINV01, INPUT_PULLDOWN);
    pinMode(OUTINV23, INPUT_PULLDOWN);
    pinMode(SSIDSEL, INPUT_PULLDOWN);
    pinMode(GROUPSEL, INPUT_PULLDOWN);
    delay(100);
    const int outinv01_pd = digitalRead(OUTINV01);
    const int outinv23_pd = digitalRead(OUTINV23);
    const int ssidsel_pd = digitalRead(SSIDSEL);
    const int groupsel_pd = digitalRead(GROUPSEL);

    pinMode(OUTINV01, INPUT);
    pinMode(OUTINV23, INPUT);
    pinMode(SSIDSEL, INPUT);
    pinMode(GROUPSEL, INPUT);

    Serial.print("Wi-Fi mode: ");
    WifiMode wifiMode;
    if (digitalRead(WIFIMODE) == HIGH) {
        wifiMode = WIFIMODE_STA;
        Serial.println("station mode");
    }
    else {
        wifiMode = WIFIMODE_AP;
        Serial.println("access point mode");
    }

    Serial.print("SSID: ");
    char ssid[sizeof(BASESSID) + 1];
    strcpy(ssid, BASESSID);
    if ((ssidsel_pu == HIGH) && (ssidsel_pd == HIGH)) {
        ssid[sizeof(ssid) - 2] = '1';
        ssid[sizeof(ssid) - 1] = '\0';
    }
    else if ((ssidsel_pu == LOW) && (ssidsel_pd == LOW)) {
        ssid[sizeof(ssid) - 2] = '0';
        ssid[sizeof(ssid) - 1] = '\0';
    }
    Serial.println(ssid);

    Serial.print("Device group: ");
    int group = 0;
    if ((groupsel_pu == HIGH) && (groupsel_pd == HIGH)) {
        group = 2;
    }
    else if ((groupsel_pu == LOW) && (groupsel_pd == LOW)) {
        group = 1;
    }
    Serial.println(group);

    Serial.print("Device role: ");
    int roleValue = 0;
    if (digitalRead(ROLESEL1) == HIGH) {
        roleValue += 2;
    }
    if (digitalRead(ROLESEL0) == HIGH) {
        roleValue += 1;
    }
    Role role;
    const char* roleString;
    switch (roleValue) {
    case 0:
        role = ROLE_CLIENT1;
        g_roleString = "CLIENT1";
        Serial.println("client1");
        break;
    case 1:
        role = ROLE_SERVERWITHCLIENT1;
        g_roleString = "SERVER";
        Serial.println("server (with client1)");
        break;
    case 2:
        role = ROLE_CLIENT0;
        g_roleString = "CLIENT0";
        Serial.println("client0");
        break;
    case 3:
    default:
        role = ROLE_SERVER;
        g_roleString = "SERVER";
        Serial.println("server");
        break;
    }

    Serial.print("IP address: ");
    IPAddress ipAddress(BASEIPADDRESS);
    ipAddress[3] += (uint8_t)(group * 4);
    if (role == ROLE_CLIENT0) {
        ipAddress[3] += 1;
    }
    else if (role == ROLE_CLIENT1) {
        ipAddress[3] += 2;
    }
    Serial.println(ipAddress);

    Serial.print("WebAPI URL: http://");
    Serial.print(ipAddress);
    if (WEBAPIPORT != 80) {
        Serial.print(":");
        Serial.print(WEBAPIPORT);
    }
    Serial.println("/");

    Serial.print("Server URL (Do not access via web browser): ");
    IPAddress serverAddress(BASEIPADDRESS);
    serverAddress[3] += (uint8_t)(group * 4);
    String url("http://");
    url += serverAddress[0];
    url += ".";
    url += serverAddress[1];
    url += ".";
    url += serverAddress[2];
    url += ".";
    url += serverAddress[3];
    if (SERVERPORT != 80) {
        url += ":";
        url += SERVERPORT;
    }
    url += "/";
    g_serverUrl = url;
    Serial.println(url);

    bool outputinv[4];
    if ((outinv01_pu == HIGH) && (outinv01_pd == HIGH)) {
        outputinv[0] = true;
    }
    else if ((outinv01_pu == LOW) && (outinv01_pd == LOW)) {
        outputinv[0] = true;
        outputinv[1] = true;
    }
    if ((outinv23_pu == HIGH) && (outinv23_pd == HIGH)) {
        outputinv[2] = true;
    }
    else if ((outinv23_pu == LOW) && (outinv23_pd == LOW)) {
        outputinv[2] = true;
        outputinv[3] = true;
    }
    for (int i = 0; i < 4; ++i) {
        Serial.printf("OUTPUT%d inversion: ", i);
        Serial.println(outputinv[i] ? "enabled" : "disabled");
        g_outputinv[i] = outputinv[i];
        setOutput(i, HIGH);
    }

    if (wifiMode == WIFIMODE_AP) {
        WiFi.softAPConfig(ipAddress, DEFAULTGATEWAY, SUBNETMASK);
        WiFi.softAP(ssid, PASSWORD);
    }
    else {
        WiFi.config(ipAddress, DEFAULTGATEWAY, SUBNETMASK);
        WiFi.begin(ssid, PASSWORD);
    }

    if (role == ROLE_SERVERWITHCLIENT1) {
        pinMode(LED1, OUTPUT);
    }

    g_role = role;
    g_wifiMode = wifiMode;
    initializeServer();
    initializeWebapi();
    initializeTimer();
    attachInterruptArg(INPUT0, onFall, (void*)0, FALLING);
    attachInterruptArg(INPUT1, onFall, (void*)1, FALLING);
    attachInterruptArg(INPUT2, onFall, (void*)2, FALLING);
    attachInterruptArg(INPUT3, onFall, (void*)3, FALLING);
}

String getQueryValue(const String& string, const String& key)
{
    /* find start of value string */
    int valueStart;
    {
        String keyString('&');
        keyString += key;
        keyString += '=';
        const int pos = string.indexOf(keyString);
        if (pos >= 0) {
            valueStart = pos + keyString.length();
        }
        else {
            String keyString(key);
            keyString += '=';
            if (string.startsWith(keyString)) {
                valueStart = keyString.length();
            }
            else {
                return String();
            }
        }
    }
    /* find end of value string */
    const int valueEnd = string.indexOf('&', valueStart);
    if (valueEnd < 0) {
        return string.substring(valueStart);
    }
    else {
        return string.substring(valueStart, valueEnd);
    }
}

void loop()
{
    int i = 0;
    if (g_wifiMode == WIFIMODE_STA) {
        while (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected");
            if (i >= 10) {
                WiFi.reconnect();
                Serial.println("WiFi reconnecting");
                return;
            }
            delay(1000);
            ++i;
        }
    }
    if ((g_role == ROLE_CLIENT0) || (g_role == ROLE_CLIENT1)) {
        while (g_now - g_lastUpdate[0] < MINIMUMINTERVAL) {
            delay(1);
        }
        HTTPClient http;
        http.begin(g_serverUrl);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        int httpCode = http.POST(getStatusString());
        if (httpCode == HTTP_CODE_OK) {
            for (int i = 0; i < 4; ++i) {
                if (g_clearFalling[i]) {
                    g_clearFalling[i] = false;
                    g_falling[i] = false;
                }
            }
            String body = http.getString();
            for (int i = 0; i < 4; ++i) {
                const String input = getQueryValue(body, String("INPUT") + String(i));
                if (input.length() > 0) {
                    setOutput(i, input.toInt());
                }
            }
            g_lastUpdate[0] = g_now;
        }
    }
    else {
        g_server.handleClient();
    }
}
