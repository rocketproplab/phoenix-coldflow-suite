#include "w5500.h"

// timing
const unsigned long OPEN_MILLIS = 500;

// bit‑masks for each valve in the 7‑bit rocket state
const uint8_t GN2_LNG_FLOW_MASK = 0b1000000;  // new “arm_lng”
const uint8_t GN2_LOX_FLOW_MASK = 0b0100000;  // new “arm_lox”
const uint8_t GN2_VENT_MASK     = 0b0010000;
const uint8_t LNG_FLOW_MASK     = 0b0001000;
const uint8_t LNG_VENT_MASK     = 0b0000100;
const uint8_t LOX_FLOW_MASK     = 0b0000010;
const uint8_t LOX_VENT_MASK     = 0b0000001;

// assign each valve its GPIO pin; feel free to adjust as you wire it up
const uint8_t GN2_LNG_FLOW_PIN = 6; // confirmed
const uint8_t GN2_LOX_FLOW_PIN = 5; // confirmed
const uint8_t GN2_VENT_PIN     = 4;
const uint8_t LNG_FLOW_PIN     = 3; // confirmed
const uint8_t LNG_VENT_PIN     = 7;
const uint8_t LOX_FLOW_PIN     = 9; // confirmed
const uint8_t LOX_VENT_PIN     = 8;

// first byte 0x02 = locally‑administered, unicast
const uint8_t MAC_GROUND_STATION[6] = { 0x02, 0x47, 0x53, 0x00, 0x00, 0x01 }; // GS:  ground station
const uint8_t MAC_RELIEF_VALVE[6]   = { 0x02, 0x52, 0x56, 0x00, 0x00, 0x02 }; // RV:  relief valve
const uint8_t MAC_FLOW_VALVE[6]     = { 0x02, 0x46, 0x4C, 0x00, 0x00, 0x03 }; // FL:  flow valve
const uint8_t MAC_SENSOR_GIGA[6]    = { 0x02, 0x53, 0x49, 0x00, 0x00, 0x04 }; // SI:  sensor interface

// a single struct covers all valves
struct Valve {
  const char* name;
  uint8_t     mask;
  uint8_t     pin;
  bool        state;
  unsigned long lastOpened;
};

// list all 7 valves here
// comment out ones unused on this UNO
Valve valves[] = {
  { "GN2_LNG_FLOW", GN2_LNG_FLOW_MASK, GN2_LNG_FLOW_PIN, false, 0 },
  { "GN2_LOX_FLOW", GN2_LOX_FLOW_MASK, GN2_LOX_FLOW_PIN, false, 0 },
  { "GN2_VENT",     GN2_VENT_MASK,     GN2_VENT_PIN,     false, 0 },
  { "LNG_FLOW",     LNG_FLOW_MASK,     LNG_FLOW_PIN,     false, 0 },
  { "LNG_VENT",     LNG_VENT_MASK,     LNG_VENT_PIN,     false, 0 },
  { "LOX_FLOW",     LOX_FLOW_MASK,     LOX_FLOW_PIN,     false, 0 },
  { "LOX_VENT",     LOX_VENT_MASK,     LOX_VENT_PIN,     false, 0 }
};
const size_t NUM_VALVES = sizeof(valves)/sizeof(valves[0]);

Wiznet5500 w5500;
uint8_t buffer[500];
uint8_t rocketState = 0b0000000; // default null

struct TelemetryFrame
{
    //–––– Ethernet header (14 B) ––––
    uint8_t  dstMac[6];
    uint8_t  srcMac[6];
    uint16_t ethType;          // always 0x8889

    //–––– Payload ––––
    uint8_t payload[200]; // 100 bytes worth
};
TelemetryFrame f;

//——— simple hex printer for MAC debug ———
void printPaddedHex(uint8_t byte) {
  char str[2] = { (char)((byte>>4)&0x0f), (char)(byte&0x0f) };
  for(int i=0;i<2;i++){
    if(str[i]>9) str[i]+=39;
    str[i] += 48;
    // Serial.print(str[i]);
  }
}
void printMACAddress(const uint8_t addr[6]) {
  for(int i=0;i<6;i++){
    printPaddedHex(addr[i]);
    // if(i<5) Serial.print(':');
  }
  // Serial.println();
}

void setup() {
  // serial + SPI for W5500
  Serial.begin(115200);
  // SPI.begin();
  // pinMode(10, OUTPUT); digitalWrite(10, HIGH);
  Serial.println("[W5500MacRaw]");
  printMACAddress(MAC_FLOW_VALVE);
  w5500.begin(MAC_FLOW_VALVE);

  // init all valve pins
  for(size_t i=0;i<NUM_VALVES;i++){
    pinMode(valves[i].pin, OUTPUT);
    digitalWrite(valves[i].pin, LOW);
    valves[i].state = false;
    valves[i].lastOpened = 0;
  }
  Serial.println("Finished setup");
  Serial.flush();
}

void loop() {
  receiveRocketState();
  updateValveStates();
  applyValveVoltages();
  sendSensor();
  Serial.println("completed loop");
  delay(50);
}

void receiveRocketState() {
  uint16_t len = w5500.readFrame(buffer, sizeof(buffer));
  if(len > 0) { 
    if (buffer[12] != 0x63 || buffer[13] != 0xe4) return;
    uint8_t newState = buffer[14];
    if(newState != rocketState) {
      rocketState = newState;
      Serial.println("----------");
      Serial.print("Received Rocket State: ");
      Serial.println(rocketState, BIN);
    }
  }
}

void updateValveStates() {
  // for each valve, compare mask bit → open/close transition
  for(size_t i=0;i<NUM_VALVES;i++) {
    bool shouldBeOpen = rocketState & valves[i].mask;
    if(shouldBeOpen && !valves[i].state) {
      Serial.print(valves[i].name);
      Serial.println(": OPENING");
      valves[i].state = true;
      valves[i].lastOpened = millis();
    }
    else if(!shouldBeOpen && valves[i].state) {
      Serial.print(valves[i].name);
      Serial.println(": CLOSING");
      valves[i].state = false;
    }
  }
}

void applyValveVoltages() {
  // full PWM for OPEN_MILLIS, then trickle
  for(size_t i=0;i<NUM_VALVES;i++) {
    if(valves[i].state) {
      unsigned long elapsed = millis() - valves[i].lastOpened;
      uint8_t pwm = (elapsed > OPEN_MILLIS) ? 77 : 255;
      analogWrite(valves[i].pin, pwm);
      // Serial.print("Writing ");
      // Serial.print(pwm);
      // Serial.print(" to ");
      // Serial.println(valves[i].pin);
    } else {
      analogWrite(valves[i].pin, 0);
    }
  }
}

void sendSensor() {
  if(Serial.available() > 0) {
    String str = Serial.readStringUntil('\n');
    if(str == NULL) {
      Serial.println("invalid read");
      return;
    }
    Serial.println(str);
    char* data=str.c_str();
    int len = strlen(data);
    memset(&f, 0, sizeof(f)); // set to 0
    //memcpy(f.dstMac, MAC_GROUND_STATION, 6);
    ((byte *)&f)[0] = 0xFF;
    ((byte *)&f)[1] = 0xFF; 
    ((byte *)&f)[2] = 0xFF; 
    ((byte *)&f)[3] = 0xFF; 
    ((byte *)&f)[4] = 0xFF; 
    ((byte *)&f)[5] = 0xFF; 
    memcpy(f.srcMac, MAC_FLOW_VALVE, 6);
    ((byte *)&f)[12] = 0x88;
    ((byte *)&f)[13] = 0x89;
    Serial.print("Data len: ");
    Serial.println(len);
    memcpy(f.payload, data, len);
    if(w5500.sendFrame((byte *) &f, sizeof(f)) <= 0){
      Serial.println("Ethernet send Failed"); 
    }
  }
  else {
    Serial.println("Serial unavailable");
  }
}
