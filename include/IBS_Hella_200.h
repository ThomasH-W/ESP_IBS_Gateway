/*
  File: IBS_Hella_200.h
  Date: 2019-09-12

  ToDo:
*/
#ifndef IBS_Hella_200_h
#define IBS_Hella_200_h

#define PIN_MPC_CS 2
#define PIN_MPC_TX D6
#define PIN_MPC_RX D7

#define LIN_SERIAL_SPEED 19200
#define LIN_BREAK_BITS 13

#define IBS_SENSOR 0

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

void IBS_LIN_Setup(int IBS_Sensor);
void IBS_LIN_Read(char *json_message);

#endif
