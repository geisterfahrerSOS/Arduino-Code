#include <Arduino.h>

#include "Wire.h" // This library allows you to communicate with I2C devices.
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <Servo.h>

#define SCHUSS1 0  //3 PINS irl
#define SCHUSS2 2  //4
#define SCHUSS3 14 //5
#define SCHUSS4 12 //6
#define SCHUSS5 13 //7
#define ACC 16     //0 //Fehler
#define SERVO 11
/////////////////Testing 
int befAcc = 0;
int aftAcc = 0;
bool buttonFlagAcc = false;
int pressTimeAcc = 0;
////////////////////////

//////////////////////////////////////// Schüsse 
int schussMap[5] = {5, 4, 0, 2, 3}; //12340
float treffer[5][2] = {{0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}}; //{Platte, Zeit}
int anzahl = 0; //anzahl der Schüsse

///////////////////Server Kommunikation
int startShooting = 0;
bool shotListener = false;
int athleteId = -1;
String art = "test";

String host = "192.168.178.61";

void sendShots(float shots[5][2])
{
    if (WiFi.status() == WL_CONNECTED)
    {                    //Check WiFi connection status
        HTTPClient http; //Declare object of class HTTPClient
        //GET Data
        String Link = "http://" + host + ":80/addShooting?mode=id&value=" + String(athleteId) + "&result=" +
                      String((int)shots[0][0]) + "," + String(shots[0][1]) + "," +
                      String((int)shots[1][0]) + "," + String(shots[1][1]) + "," +
                      String((int)shots[2][0]) + "," + String(shots[2][1]) + "," +
                      String((int)shots[3][0]) + "," + String(shots[3][1]) + "," +
                      String((int)shots[4][0]) + "," + String(shots[4][1]) + "&art=" + art;
        Serial.println(Link);
        http.begin(Link);                  //Specify request destination
        int httpCode = http.GET();         //Send the request
        String payload = http.getString(); //Get the response payload

        Serial.println(httpCode); //Print HTTP return code
        Serial.println(payload);  //Print request response payload

        http.end(); //Close connection
    }
}
String sendRequest(String request)
{
    if (WiFi.status() == WL_CONNECTED)
    {                    //Check WiFi connection status
        HTTPClient http; //Declare object of class HTTPClient
        //GET Data
        String Link = "http://" + host + ":80/" + request;
        //Serial.println(Link);
        http.begin(Link);                  //Specify request destination
        int httpCode = http.GET();         //Send the request
        String payload = http.getString(); //Get the response payload

        //Serial.println(httpCode); //Print HTTP return code
        Serial.println(payload); //Print request response payload

        http.end(); //Close connection
        if (httpCode == 200)
        {
            return payload;
        }
        else
        {
            return String(httpCode);
        }
    }
}
int sendRequestInt(String request)
{
    if (WiFi.status() == WL_CONNECTED)
    {                    //Check WiFi connection status
        HTTPClient http; //Declare object of class HTTPClient
        //GET Data
        String Link = "http://" + host + ":80/" + request;
        Serial.println(Link);
        http.begin(Link);                  //Specify request destination
        int httpCode = http.GET();         //Send the request
        String payload = http.getString(); //Get the response payload

        Serial.println(httpCode); //Print HTTP return code
        Serial.println(payload);  //Print request response payload

        http.end(); //Close connection
        if (httpCode == 200)
        {
            return payload.toInt();
        }
        else
        {
            return httpCode;
        }
    }
}
/////////////////////// Action
int actionId = -1;
int timeAction = 0;

#define WIFI_SSID "PG6WLAN"
#define WIFI_PASS "Turbolader24$$"

//#define WIFI_SSID "Vodafone-FBF4"
//#define WIFI_PASS "atqNqPcMgqAXgd2P"

//#define WIFI_SSID "moto g(6) 5401"
//#define WIFI_PASS "the1and0nli"
/////////////////////////
#define MPU_ADDR 0x67 //ACHTUNG!!! Adresser noch ändern I2C address of the MPU-6050. If AD0 pin is set to HIGH, the I2C address will be 0x69.

int16_t accelerometer_x, accelerometer_y, accelerometer_z; // variables for accelerometer raw data
int16_t accelerometer_x_old, accelerometer_y_old, accelerometer_z_old;
int16_t gyro_x, gyro_y, gyro_z; // variables for gyro raw data
int16_t temperature;            // variables for temperature data

char tmp_str[7]; // temporary variable used in convert function

char *convert_int16_to_str(int16_t i)
{ // converts int16 to string. Moreover, resulting strings will have the same length in the debug monitor.
    sprintf(tmp_str, "%6d", i);
    return tmp_str;
}
int schwelle = 800;
int accTime = 0;
int accDelay = 20;
int danebenTime = 0;
/////////////////////
int requestTime = 0;
int requestDelay = 3000;

int time_daneben = 0;
/////////////////////
int bef[5];
int aft[5];
bool buttonFlag[5] = {false, false, false, false, false};
int pressTime[5] = {0, 0, 0, 0, 0};
///////////////////Servo stuff
Servo myServo;
void servoReset(){
    myServo.write(110);
    delay(1000);
    myServo.write(30);
    delay(1000);
}
void setup()
{
    Serial.begin(9600); // The baudrate of Serial monitor is set in 9600
    for (int i = 0; i < 5; i++)
    {
        pinMode(schussMap[i], INPUT_PULLUP);
    }
    myServo.attach(SERVO);
    myServo.write(30);
    //Begin I2C
    Wire.begin();
    Wire.beginTransmission(MPU_ADDR); // Begins a transmission to the I2C slave (GY-521 board)
    Wire.write(0x6B);                 // PWR_MGMT_1 register
    Wire.write(0);                    // set to zero (wakes up the MPU-6050)
    Wire.endTransmission(true);
    // Begin WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Connecting to WiFi...
    Serial.print("Connecting to ");
    Serial.print(WIFI_SSID);
    // Loop continuously while WiFi is not connected
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(100);
        Serial.print(".");
    }

    // Connected to WiFi
    Serial.println();
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
}

void loop()
{
    if (millis() - timeAction > 1000) //ping server for shooting start
    {
        if (sendRequest("getAction?mode=action") == "start" && !shotListener)
        {
            //Anfagen Schießen aufzunehmen
            Serial.println("Operations have been started...");
            //Schussdaten Speicherung
            athleteId = sendRequestInt("getAction?mode=athleteId");
            art = sendRequest("getAction?mode=art");
            shotListener = true; //Höre auf Schüsse
            startShooting = millis();//merke dir die Startzeit
        }
        else if (sendRequest("getAction?mode=action") == "stop")
        {
            //stop shooting collection
            shotListener = false;
            servoReset(); //Platten zurückstellen
        }
        timeAction = millis(); //Zeit für erneute Server Abfrage
    }
    if (shotListener)
    {
        if (millis() - startShooting > 60000) //Nach 60 sekunden inaktivität stoppe das Schießen
        {
            sendRequest("setAction?mode=id&value=" + String(athleteId) + "&action=stop"); //sende an den Servo "stop"
            shotListener = false; //Höre nicht mehr auf das Schießen
        }
        ////////////////////////////////Zum testen der Fehler
        befAcc = digitalRead(ACC);
        if ((befAcc != aftAcc) && (befAcc == 0)) //Wenn ungleich zuvor und gedrückt
        {
            buttonFlagAcc = true;
            pressTimeAcc = millis();
        }
        if (buttonFlagAcc)
        {
            if (millis() - pressTimeAcc < 15)
            {
                if (digitalRead(ACC) == 1) //Abfrage nicht gedrückt
                {
                    buttonFlagAcc = false; //Stotter gewesen
                }
            }
            else //wenn 15 ms gedrückt
            {
                //Fehler ausgelöst
                treffer[anzahl][0] = 0; //Fehler in das Array reinschreiben
                treffer[anzahl][1] = (millis() - startShooting) / 1000.0; //Zeit in sekunden seit Anfang schießen
                //Ausgabe des Treffer arrays
                for (int j = 0; j < 5; j++)
                {
                    Serial.print(treffer[j][0]);
                    Serial.print(" time: ");
                    Serial.println(treffer[j][1]);
                }
                Serial.println("\n");
                /////////////////////////////////
                if (anzahl >= 4)
                {
                    //Schicke Schießdatena an den Server
                    sendShots(treffer);
                    sendRequest("setAction?mode=id&value=" + String(athleteId) + "&action=stop");
                    anzahl = 0;
                    //Zurückgesetzt
                    for (int j = 0; j < 5; j++)
                    {
                        treffer[j][0] = 0;
                        treffer[j][1] = -1;
                    }
                    buttonFlagAcc = false;
                }
                else
                {
                    //Schusszahl erhöhen
                    anzahl++;
                }
                buttonFlagAcc = false;
            }
        }
        aftAcc = befAcc;
        //////////////////////////////////////////Trefferabfragen
        for (int i = 0; i < 5; i++)
        {
            bef[i] = digitalRead(schussMap[i]);
            if ((bef[i] != aft[i]) && (bef[i] == 0))
            {
                buttonFlag[i] = true;
                pressTime[i] = millis();
            }
            if (buttonFlag[i])
            {
                if (millis() - pressTime[i] < 15)
                {
                    if (digitalRead(schussMap[i]) == 1)
                    {
                        buttonFlag[i] = false;
                    }
                }
                else
                {
                    if (anzahl > 0)
                    {
                        if (treffer[anzahl - 1][1] > (millis() - startShooting) / 1000.0 - 1.2)
                        {
                            Serial.println("Fehler revidiert!");
                            anzahl--;
                        }
                    }
                    //treffer ausgelöst
                    treffer[anzahl][0] = i + 1; //Schreibe plattennummer in das Array
                    treffer[anzahl][1] = (millis() - startShooting) / 1000.0; //Zeiteintrag
                    //Ausgabe
                    for (int j = 0; j < 5; j++)
                    {
                        Serial.print(treffer[j][0]);
                        Serial.print(" time: ");
                        Serial.println(treffer[j][1]);
                    }
                    Serial.println("\n");
                    if (anzahl >= 4)
                    {
                        sendShots(treffer);
                        sendRequest("setAction?mode=id&value=" + String(athleteId) + "&action=stop");
                        anzahl = 0;
                        for (int j = 0; j < 5; j++)
                        {
                            treffer[j][0] = 0;
                            treffer[j][1] = -1;
                        }
                        buttonFlag[i] = false;
                        anzahl = 0;
                    }
                    else
                    {
                        anzahl++;
                    }
                    buttonFlag[i] = false;
                }
            }
            aft[i] = bef[i];
        }
    }
}