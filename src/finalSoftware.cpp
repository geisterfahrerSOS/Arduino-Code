#include <Arduino.h>

#include "Wire.h" // This library allows you to communicate with I2C devices.
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <Servo.h>

//////////////////////////////////////// Schüsse
int schussMap[5] = {12, 13, 0, 2, 14};                               //67345                           //12340
float treffer[5][2] = {{0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}}; //{Platte, Zeit}
int anzahl = 0;                                                      //anzahl der Schüsse
///////////////////Server Kommunikation
int requestTime = 0;
int startShooting = 0;
bool shotListener = false;
int athleteId = -1;
String art = "test";

String host = "192.168.43.237"; //Server Adress

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
        //Serial.println(payload); //Print request response payload

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

//////////////////////////////Wlan Verbindung
//#define WIFI_SSID "PG6WLAN"
//#define WIFI_PASS "Turbolader24$$"

//#define WIFI_SSID "Vodafone-FBF4"
//#define WIFI_PASS "atqNqPcMgqAXgd2P"

#define WIFI_SSID "moto g(6) 5401"
#define WIFI_PASS "the1and0nli"
/////////////////////////Accelerometer
#define MPU_ADDR 0x68 //ACHTUNG!!! Adresser noch ändern I2C address of the MPU-6050. If AD0 pin is set to HIGH, the I2C address will be 0x69.

int16_t accelerometer_x, accelerometer_y, accelerometer_z; // variables for accelerometer raw data
int16_t accelerometer_x_old, accelerometer_y_old, accelerometer_z_old;
int stateFehler = 0; //Bereits einmal Accelerometer Werte beschrieben?
int schwelle = 4000; //Auslöseschwelle für einen Fehler
int accTime = 0;
/////////////////////Button Auslösen
int bef[5];
int aft[5];
int pressTime[5] = {0, 0, 0, 0, 0};
///////////////////Servo
#define SERVO 16
Servo myServo;
void servoReset()
{
    myServo.write(0);
    delay(1000);
    myServo.write(120);
    delay(1000);
}
void setup()
{
    Serial.begin(9600); // The baudrate of Serial monitor is set in 9600
    for (int i = 0; i < 5; i++)
    {
        pinMode(schussMap[i], INPUT_PULLUP);
    }
    /////////////////////Servo init
    myServo.attach(SERVO);
    myServo.write(120);
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
    if (millis() - requestTime > 1000) //ping server for shooting start
    {
        if (sendRequest("getAction?mode=action") == "start" && !shotListener)
        {
            //Anfagen Schießen aufzunehmen
            Serial.println("Operations have been started...");
            //Schussdaten Speicherung
            athleteId = sendRequestInt("getAction?mode=athleteId");
            art = sendRequest("getAction?mode=art");
            shotListener = true;      //Höre auf Schüsse
            startShooting = millis(); //merke dir die Startzeit
        }
        else if (sendRequest("getAction?mode=action") == "stop")
        {
            //stop shooting collection
            shotListener = false;
        }
        requestTime = millis(); //Zeit für erneute Server Abfrage
    }
    if (shotListener)
    {
        if (millis() - startShooting > 60000) //Nach 60 sekunden inaktivität stoppe das Schießen
        {
            sendShots(treffer);
            sendRequest("setAction?mode=id&value=" + String(athleteId) + "&action=stop");
            servoReset(); //Platten zurückstellen
            anzahl = 0;
            //Zurückgesetzt
            for (int j = 0; j < 5; j++)
            {
                treffer[j][0] = 0;
                treffer[j][1] = -1;
            } //Höre nicht mehr auf das Schießen
        }
        ///////////////////////////////Fehler
        if (millis() - accTime > 30) //Fehlerermittlung
        {
            Wire.beginTransmission(MPU_ADDR);
            Wire.write(0x3B);                                 // starting with register 0x3B (ACCEL_XOUT_H) [MPU-6000 and MPU-6050 Register Map and Descriptions Revision 4.2, p.40]
            Wire.endTransmission(false);                      // the parameter indicates that the Arduino will send a restart. As a result, the connection is kept active.
            Wire.requestFrom(MPU_ADDR, 7 * 2, true);          // request a total of 7*2=14 registers
            accelerometer_x = Wire.read() << 8 | Wire.read(); // reading registers: 0x3B (ACCEL_XOUT_H) and 0x3C (ACCEL_XOUT_L)
            accelerometer_y = Wire.read() << 8 | Wire.read(); // reading registers: 0x3D (ACCEL_YOUT_H) and 0x3E (ACCEL_YOUT_L)
            accelerometer_z = Wire.read() << 8 | Wire.read(); // reading registers: 0x3F (ACCEL_ZOUT_H) and 0x40 (ACCEL_ZOUT_L)
            if (abs(accelerometer_x - accelerometer_x_old) + abs(accelerometer_y - accelerometer_y_old) + abs(accelerometer_z - accelerometer_z_old) > schwelle && stateFehler != 0)
            {
                //Fehler ausgelöst
                if (anzahl > 0)
                {
                    if (treffer[anzahl - 1][1] > (millis() - startShooting) / 1000.0 - 1.0)
                    {
                        Serial.println("Schuss kurz zuvor gefallen, keinen Fehler gegeben!");//1
                    }
                    else //Fehler gegeben
                    { 
                        Serial.println("Fehler gegeben, weil genug Zeit zum vorherigen Schuss");
                        treffer[anzahl][0] = 0;                                   //Fehler in das Array reinschreiben
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
                            servoReset(); //Platten zurückstellen
                            anzahl = 0;
                            //Zurückgesetzt
                            for (int j = 0; j < 5; j++)
                            {
                                treffer[j][0] = 0;
                                treffer[j][1] = -1;
                            }
                        }
                        else
                        {
                            //Schusszahl erhöhen
                            anzahl++;
                        }
                    }
                }
                else //Fehler gegeben
                {
                    Serial.println("Fehler gegeben, weil noch kein vorheriger Schuss");
                    treffer[anzahl][0] = 0;                                   //Fehler in das Array reinschreiben
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
                        servoReset(); //Platten zurückstellen
                        anzahl = 0;
                        //Zurückgesetzt
                        for (int j = 0; j < 5; j++)
                        {
                            treffer[j][0] = 0;
                            treffer[j][1] = -1;
                        }
                    }
                    else
                    {
                        //Schusszahl erhöhen
                        anzahl++;
                    }
                }
            }
            accelerometer_x_old = accelerometer_x;
            accelerometer_y_old = accelerometer_y;
            accelerometer_z_old = accelerometer_z;
            accTime = millis();
            stateFehler = 1;
        }
        //////////////////////////////////////////Trefferabfragen
        for (int i = 0; i < 5; i++) //Trefferermittlung
        {
            bef[i] = digitalRead(schussMap[i]);
            if ((bef[i] != aft[i]) && (bef[i] == 0) && millis() - pressTime[i] > 1500)//Verhinderung von Prellen
            {
                //treffer ausgelöst
                if (anzahl > 0)//Kann auch zusammengefasst werden...
                {
                    if (treffer[anzahl - 1][1] > (millis() - startShooting) / 1000.0 - 1.0)
                    {
                        Serial.println("Fehler revidiert, weil zeitlich zu nah am Treffer");//2
                        anzahl--;
                    }
                }
                int trefferSchon = 0; //Untersuchung ob Treffer schon mal angekommen
                for (int j = 0; j < anzahl; j++)
                {
                    if (treffer[j][0] == i + 1)
                    {
                        trefferSchon = 1;
                        break;
                    }
                }
                if (trefferSchon == 0)
                {
                    //treffer gegeben
                    Serial.println("Treffer ausgelöst");
                    treffer[anzahl][0] = i + 1;                               //Schreibe plattennummer in das Array
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
                        servoReset(); //Platten zurückstellen
                        anzahl = 0;
                        for (int j = 0; j < 5; j++)
                        {
                            treffer[j][0] = 0;
                            treffer[j][1] = -1;
                        }
                        anzahl = 0;
                    }
                    else
                    {
                        anzahl++;
                    }
                }
                else
                {
                    Serial.println("Platte bereits umgefallen, Treffer nicht gewertet");
                }
                pressTime[i] = millis();
            }
            aft[i] = bef[i];
        }
    }
}