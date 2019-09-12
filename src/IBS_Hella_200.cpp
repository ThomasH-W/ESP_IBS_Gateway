#include <arduino.h>
#include <SoftwareSerial.h>

// LIN serial Interface
int serSpeed = 19200;   // speed LIN
int breakDuration = 13; // number of bits break signal

int linCSPin = 2; // CS Pin
int txPin1 = D6;  // TX Pin LIN serial
int rxPin1 = D7;  // RX Pin LIN serial

SoftwareSerial linSerial(rxPin1, txPin1); // RX, TX
//SoftwareSerial linSerial(rxPin1, txPin1, false, 128);

// Configuration
boolean outputSerial = true; // true if json output to serial, false if only display
boolean debug = true;        // Debug output UART

// Global Variables
byte LinMessage[9] = {0};
byte LinMessageA[200] = {0};
boolean linSerialOn = 0;

/*
  Der Code nutzt Sensor 1. Für Sensor 2 müssen einfach andere Frame ID´s genutzt werden. 
  Hier ist die entsprechende Übersetzungsliste:

  Frame ID Typ 1   | 0x11 |   | 0x21 |   | 0x22 |   | 0x23 |   | 0x24 |   | 0x25 |   | 0x26
  Frame ID Typ 2   | 0x12 |   | 0x27 |   | 0x28 |   | 0x29 |   | 0x2A |   | 0x2B |   | 0x2C
  */

#define IBS_FRM_tb1 0 // 0x12
#define IBS_FRM_tb2 1 // 0x27
#define IBS_FRM_CUR 2 // 0x28
#define IBS_FRM_ERR 3 // 0x29
#define IBS_FRM_tb3 4 // 0x2A
#define IBS_FRM_SOx 5 // 0x2B
#define IBS_FRM_CAP 6 // 0x2C

#define IBS_SENSOR 0
//                   0     1     2     3     4     5     6     7
byte FrameID1[2][7] = {{0x11, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26},
                       {0x12, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C}};

void readFrame(byte mID);
void serialBreak();
void sendMessage(byte mID, int nByte);
byte LINChecksum(int nByte);
byte addIDParity(byte linID);

//-----------------------------------------------------------------------------------------------------------------
void IBS_LIN_Setup()
{
    pinMode(linCSPin, OUTPUT); // CS Signal LIN Tranceiver
    digitalWrite(linCSPin, HIGH);

    Serial.begin(115200);
    Serial.println("-------------------------------------------------------------");
    Serial.println("> Starting module");
    Serial.flush();
    Serial.end();
    delay(50);

    linSerial.begin(serSpeed);
    linSerialOn = 1;
}

//-----------------------------------------------------------------------------------------------------------------
void IBS_LIN_Read()
{
    int soc;
    int soh;
    float Ubatt;
    float Ibatt;
    float Btemp;
    float AvCap;
    int remTime = 0;

    LinMessageA[0] = 0x01;
    while (bitRead(LinMessageA[0], 0) > 0)
    {
        // readFrame(0x27);
        readFrame(FrameID1[IBS_SENSOR][IBS_FRM_tb2]);
    }

    // Read Frame IBS_FRM 2 - Current Values

    // readFrame(0x28);
    readFrame(FrameID1[IBS_SENSOR][IBS_FRM_CUR]);

    Ubatt = (float((LinMessageA[4] << 8) + LinMessageA[3])) / 1000;
    Ibatt = (float((long(LinMessageA[2]) << 16) + (long(LinMessageA[1]) << 8) + long(LinMessageA[0]) - 2000000L)) / 1000;
    Btemp = long(LinMessageA[5]) / 2 - 40;

    // Read Frame IBS_FRM 3 - Error Values
    // readFrame(0x29);
    readFrame(FrameID1[IBS_SENSOR][IBS_FRM_ERR]);

    // Read Frame IBS_FRM 5
    // readFrame(0x2B);
    readFrame(FrameID1[IBS_SENSOR][IBS_FRM_SOx]);

    soc = int(LinMessageA[0]) / 2;
    soh = int(LinMessageA[1]) / 2;

    // Read Frame IBS_FRM 6
    // readFrame(0x2C);
    readFrame(FrameID1[IBS_SENSOR][IBS_FRM_CAP]);

    AvCap = (float((LinMessageA[3] << 8) + LinMessageA[2])) / 10; //Dischargeable Capacity
    int Calib = bitRead(LinMessageA[5], 0);

    // Read Frame 4
    // readFrame(0x2A);
    readFrame(FrameID1[IBS_SENSOR][IBS_FRM_tb3]);

    //soc = (AvCap/(80*soh/100))*100;         // Anzeige der eigentlichen Restkapazität im Battsymbol

    // Output json String to serial

    if (outputSerial)
    {
        Serial.begin(115200);
        delay(100);
        Serial.print("{\"current\":{\"ubat\":\"");
        Serial.print(Ubatt, 2);
        Serial.print("\",\"icurr\":\"");
        Serial.print(Ibatt, 3);
        Serial.print("\",\"soc\":\"");
        Serial.print(soc);
        Serial.print("\",\"time\":\"");
        Serial.print(remTime);
        Serial.print("\",\"avcap\":\"");
        Serial.print(AvCap, 1);
        Serial.print("\"},\"Akku\":{\"soh\":\"");
        Serial.print(soh);
        Serial.print("\",\"temp\":\"");
        Serial.print(Btemp);
        Serial.println("\"}}");
        Serial.flush();
        Serial.end();
        delay(100);
    }

    delay(2000);
} // end of function loop

//-----------------------------------------------------------------------------------------------------------------
// Read answer from Lin bus
void readFrame(byte mID)
{
    memset(LinMessageA, 0, sizeof(LinMessageA));
    int ix = 0;
    byte linID = mID & 0x3F | addIDParity(mID);

    serialBreak();
    linSerial.flush();
    linSerial.write(0x55);  // Sync
    linSerial.write(linID); // ID
    linSerial.flush();
    delay(800);
    if (linSerial.available())
    { // read serial
        while (linSerial.available())
        {
            LinMessageA[ix] = linSerial.read();
            ix++;
            if (ix > 9)
            {
                break;
            }
        }
    }
    Serial.begin(115200);
    delay(50);
    Serial.print("ID: ");
    Serial.print(linID, HEX);
    Serial.print(" --> ");
    Serial.print(mID, HEX);
    Serial.print(": ");
    for (int ixx = 0; ixx < 9; ixx++)
    {
        Serial.print(LinMessageA[ixx], HEX);
        Serial.print(":");
    }
    Serial.println("  Lesen Ende");
    Serial.flush();
    Serial.end();
    delay(100);
}

//-----------------------------------------------------------------------------------------------------------------
// Generate Break signal LOW on LIN bus
void serialBreak_old()
{
    // delay(1000 / serSpeed * breakDuration); // duration break time pro bit in milli seconds * number of bit for break
    delayMicroseconds(1000000 / serSpeed * breakDuration);
}

//-----------------------------------------------------------------------------------------------------------------
void serialBreak()
{
    //  if (linSerialOn == 1) linSerial.end();
    pinMode(txPin1, OUTPUT);
    digitalWrite(txPin1, LOW);                             // send break
    delayMicroseconds(1000000 / serSpeed * breakDuration); // duration break time pro bit in milli seconds * number of bit for break
    digitalWrite(txPin1, HIGH);
    delayMicroseconds(1000000 / serSpeed); // wait 1 bit
    linSerial.begin(serSpeed);
    linSerialOn = 1;
}

//-----------------------------------------------------------------------------------------------------------------
void sendMessage(byte mID, int nByte)
{

    byte cksum = LINChecksum(nByte);
    byte linID = mID & 0x3F | addIDParity(mID);
    serialBreak();
    linSerial.write(0x55);  // Sync
    linSerial.write(linID); // ID
    while (nByte-- > 0)
        linSerial.write(LinMessage[nByte]); // Message (array from 1..8)
    linSerial.write(cksum);
    linSerial.flush();
}

//-----------------------------------------------------------------------------------------------------------------
byte LINChecksum(int nByte)
{
    uint16_t sum = 0;
    while (nByte-- > 0)
        sum += LinMessage[nByte];
    while (sum >> 8)
        sum = (sum & 255) + (sum >> 8);
    return (~sum);
}

//-----------------------------------------------------------------------------------------------------------------
byte addIDParity(byte linID)
{
    byte p0 = bitRead(linID, 0) ^ bitRead(linID, 1) ^ bitRead(linID, 2) ^ bitRead(linID, 4);
    byte p1 = ~(bitRead(linID, 1) ^ bitRead(linID, 3) ^ bitRead(linID, 4) ^ bitRead(linID, 5)); // evtl. Klammer rum ???
    return ((p0 | (p1 << 1)) << 6);
}
