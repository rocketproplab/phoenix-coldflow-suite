//LIBRARIES*****************************************************
//SPI Libs
#include <SPI.h>


//Ethernet Lib
#include "w5500.h"
// #include <SD.h>
// File myFile;

//Load Cell Lib
#include "HX711.h"
//Thermocouple Libs
#include <Wire.h>
#include <DallasTemperature.h>


#include <Adafruit_I2CDevice.h>
#include <Adafruit_I2CRegister.h>
#include "Adafruit_MCP9600.h"
#include <OneWire.h>

//Pin Definitions **********************************************
//Ethernet CS
#define EthernetCS 10
#define SDCS 40

//Load Cell Pins
#define DOUT1  30
#define CLK1  28
#define DOUT2  34
#define CLK2  32
#define DOUT3  38
#define CLK3  36
//ThermoCouple OneWire
#define ONE_WIRE_BUS 22
#define I2C_ADDRESS (0x67)

// ─────────────────────────────────────────────────────────────────────────────
//                           Ethernet (MAC‑RAW) Setup
// ─────────────────────────────────────────────────────────────────────────────
// first byte 0x02 = locally‑administered, unicast
const uint8_t MAC_GROUND_STATION[6] = {0x02, 0x47, 0x53,
                                       0x00, 0x00, 0x01}; // GS:  ground station
const uint8_t MAC_RELIEF_VALVE[6] = {0x02, 0x52, 0x56,
                                     0x00, 0x00, 0x02}; // RV:  relief valve
const uint8_t MAC_FLOW_VALVE[6] = {0x02, 0x46, 0x4C,
                                   0x00, 0x00, 0x03}; // FL:  flow valve
const uint8_t MAC_SENSOR_GIGA[6] = {0x02, 0x53, 0x49,
                                    0x00, 0x00, 0x04}; // SI:  sensor interface

struct TelemetryFrame
{
    //–––– Ethernet header (14 B) ––––
    uint8_t  dstMac[6];
    uint8_t  srcMac[6];
    uint16_t ethType;          // always 0x8889

    //–––– Payload ––––
    double  PT[5];            // raw ADC counts 0-4095
    double  load[3];          // HX711 “units” or N × 10-3 N
    double  tempC[2];         // centi-degree Celsius, e.g. 2534 ⇒ 25.34 °C
};

//object creation **********************************************
//Ethernet
Wiznet5500 w5500;
//Load Cells
HX711 loadCell1;
HX711 loadCell2;
HX711 loadCell3;
//Thermocouples
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature thermoCouples(&oneWire);
DeviceAddress addr;
//Adafruit_MCP9600 mcp;

//Other Consts ***************************************************
float calibration_factor1 = -7050; //-7050 worked for my 440lb max scale setup
float calibration_factor2 = -7050; //-7050 worked for my 440lb max scale setup
float calibration_factor3 = -7050; //-7050 worked for my 440lb max scale setup

Ambient_Resolution ambientRes = RES_ZERO_POINT_0625;

void setup() {
  Serial.begin(9600);
  
  Serial.println("test");
 

  // if (!SD.begin(SDCS)) {
  //   Serial.println("initialization failed!");
  //   //while (1);
  // }else{
  // Serial.println("SD Initlaized");
  // }

  //PT INPUTS
  pinMode(A0,INPUT);
  pinMode(A1,INPUT);
  pinMode(A2,INPUT);
  pinMode(A3,INPUT);
  pinMode(A4,INPUT);

  
  //ethernet setup
  // w5500.begin(MAC_SENSOR_GIGA);
  Serial.println("Ethernet Setup Complete");
  //load cell setup
  loadCell1.begin(DOUT1, CLK1);
  loadCell1.set_scale();
  loadCell1.tare(); //Reset the scale to 0
  Serial.println("Load 1 Done");
  
  loadCell2.begin(DOUT2, CLK2);
  loadCell2.set_scale();
  loadCell2.tare(); //Reset the scale to 0
  Serial.println("Load 2 Done");

  loadCell3.begin(DOUT3, CLK3);
  loadCell3.set_scale();
  loadCell3.tare(); //Reset the scale to 0
  Serial.println("Load 3 Done");

  //thermocouple setup
  
  thermoCouples.begin();
/*
  if (! mcp.begin(I2C_ADDRESS)) {
    Serial.println("Sensor not found. Check wiring!");
    while (1);
  }
  

  //mcp.setAmbientResolution(ambientRes);
  */

  Serial.println("Done Initialized");
}

void loop() {

  
  double PT1_a=analogRead(A0)/1023.0 * 5.0;
  double PT1=((PT1_a - 0.5)/(4.5-0.5)) * 5000;
  double PT2_a = analogRead(A1)/1024.0 * 5.0;
  double PT2 = ((PT2_a - 1)/(5-1)) * 1000;
  // int PT3=analogRead(A2);
  // int PT4=analogRead(A3);
  // int PT5=analogRead(A4);
  double PT3_a = analogRead(A2)/1023.0 * 5.0;
  double PT3 = ((PT3_a - 1)/(5-1)) * 1000;
  double PT4_a = analogRead(A3)/1023.0 * 5.0;
  double PT4 = ((PT4_a - 1)/(5-1)) * 1000;
  double PT5_a = analogRead(A4)/1023.0 * 5.0;
  double PT5 = ((PT5_a - 1)/(5-1)) * 1000;
  

  //LOAD CELL CALIBRATION
  loadCell1.set_scale(calibration_factor1); //Adjust to this calibration factor
  loadCell2.set_scale(calibration_factor2); //Adjust to this calibration factor
  loadCell3.set_scale(calibration_factor3); //Adjust to this calibration factor

  //LOAD CELL DATA
  double loadOutput1=loadCell1.get_units();
  double loadOutput2=loadCell2.get_units();
  double loadOutput3=loadCell3.get_units();
  
  //THERMOCOUPLE DATA
  
  thermoCouples.requestTemperatures();
  double thermoCouple1=thermoCouples.getTempCByIndex(0);
  double thermoCouple2=thermoCouples.getTempCByIndex(1);
  //double thermoCouple3=thermoCouples.getTempCByIndex(2);

  
  //LIVE ETHERNET TRANSMISSION
  //Put all data into an output string to be sent over
  //String dataOut=String(PT1)+","+String(PT2)+","+String(PT3)+","+String(PT4)+","+String(PT5);
  //String dataOut=String(loadOutput1)+","+String(loadOutput2)+","+String(loadOutput3)+","+String(thermoCouple1)+","+String(thermoCouple2);
  
  String dataOut=String(PT1)+","+String(PT2)+","+String(PT3)+","+String(PT4)+","+String(PT5) + "," +String(loadOutput1)+","+String(loadOutput2)+","+String(loadOutput3)+","+String(thermoCouple1)+","+String(thermoCouple2);
  Serial.println(dataOut);

  //Send data

  // TelemetryFrame f;
  // memcpy(f.dstMac, MAC_GROUND_STATION, 6);
  // memcpy(f.srcMac, MAC_SENSOR_GIGA, 6);
  // ((byte *)&f)[12] = 0x88;
  // ((byte *)&f)[13] = 0x89; 
  // f.PT[0] = PT1;
  // f.PT[1] = PT2;
  // f.PT[2] = PT3;
  // f.PT[3] = PT4;
  // f.PT[4] = PT5;
  // f.load[0] = loadOutput1;
  // f.load[1] = loadOutput2;
  // f.load[2] = loadOutput3;
  // f.tempC[0] = thermoCouple1;
  // f.tempC[1] = thermoCouple2;

  // if(w5500.sendFrame((byte *) &f, sizeof(f)) <= 0){
  //   Serial.println("Ethernet Failed"); 
  // }
  // else{
  //   Serial.println("Ethernet Success");  
  // }

  // myFile = SD.open("StaticFire.txt", FILE_WRITE);
  // if (myFile) {
  //   Serial.print("Writing to file...");
  //   myFile.println(dataOut);
  //   // close the file:
  //   myFile.close();
  //   Serial.println("done.");
  // } else {
  //   // if the file didn't open, print an error:
  //   Serial.println("error opening file");
  // }
  
     delay(250);

     

}