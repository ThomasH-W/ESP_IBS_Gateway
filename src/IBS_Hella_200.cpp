/*
Basiert auf dem Code von Frank Schöniger
     https://github.com/frankschoeniger/LIN_Interface

Batterie Typ - code von breezer
    https://www.kastenwagenforum.de/forum/threads/diy-hella-ibs-batteriecomputer.31724/page-2

File: IBS_Hella_200.cpp
Date: 2019-09-18

LIN Interface

*/

#include <arduino.h>
#include <SoftwareSerial.h>
#include <IBS_Hella_200.h>

// LIN serial Interface
int serSpeed = LIN_SERIAL_SPEED;    // speed LIN
int breakDuration = LIN_BREAK_BITS; // number of bits break signal

int linCSPin = PIN_MPC_CS; // CS Pin
int txPin1 = PIN_MPC_TX;   // TX Pin LIN serial
int rxPin1 = PIN_MPC_RX;   // RX Pin LIN serial

int IBS_SENSOR = DEFAULT_IBS_SENSOR;

SoftwareSerial linSerial(rxPin1, txPin1); // RX, TX
//SoftwareSerial linSerial(rxPin1, txPin1, false, 128);

// Configuration
boolean outputSerial = true; // true if json output to serial, false if only display
boolean debug = true;        // Debug output UART

// Global Variables
byte LinMessage[9] = {0};
byte LinMessageA[200] = {0};
boolean linSerialOn = 0;

//                   0     1     2     3     4     5     6     7
byte FrameID1[2][7] = {{0x11, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26},
                       {0x12, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C}};

void readFrame(byte mID);
void serialBreak();
void sendMessage(byte mID, int nByte);
byte LINChecksum(int nByte);
byte addIDParity(byte linID);

//-----------------------------------------------------------------------------------------------------------------
void IBS_LIN_Setup(byte BatTyp, byte cap)
{
    pinMode(linCSPin, OUTPUT); // CS Signal LIN Tranceiver
    digitalWrite(linCSPin, HIGH);

    Serial.println("-------------------------------------------------------------");
    Serial.println("> Setting up serial LIN communication");
    Serial.flush();
    Serial.end();
    delay(50);

    IBS_LIN_setNomCap(cap);    //Normkapazität parametrieren (7AH)
    IBS_LIN_setBatTyp(BatTyp); //Batterietyp setzen

    linSerial.begin(serSpeed);
    linSerialOn = 1;
} // end of function

//-----------------------------------------------------------------------------------------------------------------
void IBS_LIN_setNomCap(byte cap)
{

    byte i = 0;

    LinMessage[0] = 0x00;
    LinMessage[1] = 0x02;
    LinMessage[2] = 0x03;
    LinMessage[3] = 0xb5;
    LinMessage[4] = 0x39;
    LinMessage[5] = cap;
    LinMessage[6] = 0xff;
    LinMessage[7] = 0xff;
    LinMessage[8] = 0xff;

    byte cksum = LINChecksum(9);

    LinMessage[0] = 0x3c;

    serialBreak();
    linSerial.write(0x55); // Sync
    while (i < 9)
    {
        linSerial.write(LinMessage, sizeof(LinMessage)); // Message (array from 1..8)
        i++;
    }
    linSerial.write(cksum);
    linSerial.flush();
} // end of function

//-----------------------------------------------------------------------------------------------------------------
void IBS_LIN_setBatTyp(byte BatTyp)
{

    byte i = 0;

    LinMessage[0] = 0x3c;
    LinMessage[1] = 0x02;
    LinMessage[2] = 0x03;
    LinMessage[3] = 0xb5;
    LinMessage[4] = 0x3a;

    LinMessage[6] = 0xff;
    LinMessage[7] = 0xff;
    LinMessage[8] = 0xff;

    switch (BatTyp)
    {

    case BAT_TYPE_STARTER:
        //3c 02 03 b5 3a 0a ff ff ff 01
        LinMessage[5] = 0x0a;
        serialBreak();
        linSerial.write(0x55); // Sync
        while (i < 9)
        {
            linSerial.write(LinMessage, sizeof(LinMessage)); // Message (array from 1..8)
            i++;
        }
        linSerial.write(0x01);
        linSerial.flush();
        break;

    case BAT_TYPE_GEL:
        //3c 02 03 b5 3a 14 ff ff ff f6
        LinMessage[5] = 0x14;
        serialBreak();
        linSerial.write(0x55); // Sync
        while (i < 9)
        {
            linSerial.write(LinMessage, sizeof(LinMessage)); // Message (array from 1..8)
            i++;
        }
        linSerial.write(0xf6);
        linSerial.flush();
        break;

    case BAT_TYPE_AGM:
        //3c 02 03 b5 3a 1e ff ff ff ec
        LinMessage[5] = 0x1e;
        serialBreak();
        linSerial.write(0x55); // Sync
        while (i < 9)
        {
            linSerial.write(LinMessage, sizeof(LinMessage)); // Message (array from 1..8)
            i++;
        }
        linSerial.write(0xec);
        linSerial.flush();
        break;
    }
} // end of function

//-----------------------------------------------------------------------------------------------------------------
void IBS_LIN_Read(char *json_message, int IBS_SensorNo)
{
    int soc;
    int soh;
    float Ubatt;
    float Ibatt;
    float Btemp;
    float AvCap;
    int remTime = 0;

    if (IBS_SensorNo == 1)
        IBS_SENSOR = 0;
    else
        IBS_SENSOR = 1;

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
    remTime = 0;

    // Read Frame 4
    // readFrame(0x2A);
    readFrame(FrameID1[IBS_SENSOR][IBS_FRM_tb3]);

    //soc = (AvCap/(80*soh/100))*100;         // Anzeige der eigentlichen Restkapazität im Battsymbol

    //    int soc soh remTime    float Ubatt Ibatt Btemp AvCap

    // Output json String to serial
    // %d signed int; %f float; %8.2f: 8 Stellen, 2 Nachkomma
    sprintf(json_message, "{\"current\":{\"ubat\":\"%4.2f\",\"icurr\":\"%4.3f\",\"soc\":\"%d\",\"time\":\"%d\",\"avcap\":\"%3.1f\"},\"Akku\":{\"soh\":\"%d\",\"temp\":\"%4.2f\"}}",
            Ubatt, Ibatt, soc, remTime, AvCap, soh, Btemp);

    if (outputSerial)
    {
        Serial.begin(115200);
        delay(100);

        Serial.println("json_message:");
        Serial.println(json_message);

        /*
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
    */
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
