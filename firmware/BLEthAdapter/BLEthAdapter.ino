/*
  1. while chargning, RED color is ON
  2. while full charged, GREEN color is ON
  3. while  discharged below 10%, RED color is blinking 20times quickly
  4. while connected to phone, BLUE is ON
  5. while is on youtube, PINK color is ON
  6. while is on zello, BLUE color is ON
  switching between youtube and zello will be occurred by quick single click of boards switch
*/
#include <TaskScheduler.h>
#include "DFRobot_BT401.h"
#include <ezButton.h>

#define RED 12
#define GREEN 13
#define BLUE 14
#define P_CON 19
#define PTT_OUT 10
#define INT 7
#define BAT_IN 6
#define STBY 0  // when the battery charging is completed, STBY is pulled low, otherwise, it will be High impedance.
#define CHRG 1  // when charger is charging the battery, CHRG is pulled low, otherwise, it will be High impedance.

#define CYCLE_MODE_TIME 60000          // 60000 milliseconds
#define BATTERY_SHOW_TIME 200          // 200 milliseconds
#define BUTTON_TIME 10                 // 10 milliseconds
#define BLUETOOTH_CONNECTION_TIME 500  // 500 miliseconds

#define POWEROFF_COUNT 200  // buttonCallbackTime = 10ms; set powerOfftime as 2s


//typedef enum {
//  ZELLO, YOUTUBE
//} switchingMode;
//switchingMode swMode = ZELLO;
bool swMode = true;

ezButton KEY(18);     // create ezButton object that attach to pin 18;
ezButton PTT_IN(11);  // create ezButton object that attach to pin 11;

Scheduler runner;
DFRobot_BT401 bt;

const char* BleName = "KLEIN_PTT_BLE";
const char* SppName = "KLEIN_DFD_SPP";
const char* ServiceUUID = "8925D23D-03E4-4447-826C-418DADC7F483";
const char* CharacteristicUUID = "14C56166-6888-4089-B0CE-269D098FE528";

uint16_t powerOffCntDown = POWEROFF_COUNT;  // buttonCallbackTime = 10ms; set powerOfftime as 2s
bool powerOff = false;                      // Show the power status of board.

uint8_t BleSpp = 0;  // None 0, Spp 1, BLE 2

uint16_t batLevel = 0;       // level of battery
bool batLedBlink = false;    // flag of blinking battery led
uint8_t batLedBlinkCnt = 0;  // countdown to show battery status with led blink.

void rgbLed(bool r, bool g, bool b) {
  digitalWrite(RED, !r);
  digitalWrite(GREEN, !g);
  digitalWrite(BLUE, !b);
}

void powerON() {
  digitalWrite(P_CON, HIGH);
  rgbLed(1, 1, 1);  // White color
}
void powerOFF() {
  digitalWrite(P_CON, LOW);
  rgbLed(0, 0, 0);
}

// Callback methods prototypes
void cycleModesCallback();
void buttonCallback();
void bluetoothConnectionCalback();

//Tasks
Task CycleModeTask(CYCLE_MODE_TIME, TASK_FOREVER, &cycleModesCallback);
Task buttonTask(BUTTON_TIME, TASK_FOREVER, &buttonCallback);
Task bluetoothConnectionTask(BLUETOOTH_CONNECTION_TIME, TASK_FOREVER, &bluetoothConnectionCalback);
void normalLedStatus() {
  CycleModeTask.setInterval(CYCLE_MODE_TIME);
  if (bt.SppStatus())
    BleSpp = 1;
  else BleSpp = 0;
  if (bt.BleStatus())
    BleSpp = 2;
  else BleSpp = 0;
  if (BleSpp > 0) {                 // when board is connected.
    if (swMode) rgbLed(0, 0, 1);  // BLUE when connected to zello app
    else rgbLed(1, 0, 1);         // PINK when connected to youtube
  } else {                        // when board is disconnected.
    rgbLed(1, 1, 1);              // white
  }
}
void cycleModesCallback() {
  if (!(digitalRead(STBY) ^ digitalRead(CHRG))) {
    batLevel = analogRead(BAT_IN) * 0.488;  // 500 / 1024 = 0.488;    10bit AD, VCC = 5V
    if (batLevel < 313) {                   // <10% : 3.12V is 10% of max 4.2V
      batLedBlinkCnt++;
      CycleModeTask.setInterval(BATTERY_SHOW_TIME);
      batLedBlink = !batLedBlink;
      rgbLed(batLedBlink, 0, 0);  // RED color blinking
      if (batLedBlinkCnt > 20) {
        batLedBlinkCnt = 0;
        normalLedStatus();
      }
    } else {
      normalLedStatus();
    }
  }
}
void bluetoothConnectionCalback() {
  if (bt.SppStatus())
    BleSpp = 1;
  else BleSpp = 0;
  if (bt.BleStatus())
    BleSpp = 2;
  else BleSpp = 0;
  if (BleSpp > 0) { // bluetooth connection is success
    bluetoothConnectionTask.disable();
    runner.deleteTask(bluetoothConnectionTask);
    rgbLed(0, 0, 1);  // BLUE
  }
}
void buttonCallback() {
  KEY.loop();     // MUST call the loop() function first
  PTT_IN.loop();  // MUST call the loop() function first

  if (KEY.isPressed()) {
    powerOff = true;
    //powerOFF();
  }
  if (KEY.isReleased()) {
    powerOff = false;
    if (BleSpp > 0) {
      swMode = !swMode;             // switch ZELLO and YOUTUBE
      if (swMode) rgbLed(0, 0, 1);  // BLUE when connected to zello app
      else rgbLed(1, 0, 1);         // PINK when connected to youtube
    }
  }
  if (powerOff) {
    if (powerOffCntDown != 0)
      powerOffCntDown--;
    else
      powerOFF();  // 2s countdown to power board off
  } else {
    powerOffCntDown = POWEROFF_COUNT;
  }
  if (BleSpp > 0) {
    if (PTT_IN.isPressed()) {
      if (swMode) {
        if (BleSpp == 1)
          bt.sendData("+PTTS=P");
        else if (BleSpp == 2)
          bt.sendData("1");
      } else {
        bt.playPause();
      }
    }
    if (PTT_IN.isReleased()) {
      if (swMode) {
        if (BleSpp == 1)
          bt.sendData("+PTTS=R");
        else if (BleSpp == 2)
          bt.sendData("0");
      }
    }
  }
  if (digitalRead(STBY) && !digitalRead(CHRG)) {
    // Charging battery
    rgbLed(1, 0, 0);  // RED color
  }
  if (digitalRead(CHRG) && !digitalRead(STBY)) {
    // Full Charged battery
    rgbLed(0, 1, 0);  // GREEN color
  }
}

void setup() {
  Serial.begin(115200);

  KEY.setDebounceTime(50);     // set debounce time to 50 milliseconds
  PTT_IN.setDebounceTime(50);  // set debounce time to 50 milliseconds

  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);
  pinMode(P_CON, OUTPUT);
  pinMode(STBY, INPUT);
  pinMode(CHRG, INPUT);

  powerON();
  /*Delay 2s for the BT401 to start*/  
  while (!bt.begin(Serial)) {
    delay(1000);
  }
  delay(2000);
  bt.SppName(SppName);
  delay(100);
  bt.BleName(BleName);
  delay(100);
  bt.bleOnOff(1);  // turn on BLE
  delay(100);
  bt.edrOnOff(1);  // turn off Bluetooth
  delay(100);  
  //bt.bleOnOff(1);  // turn on BLE
  //bt.edrOnOff(0);  // turn off Bluetooth
  //  bt.ServiceUUID(ServiceUUID);
  //  bt.CharacteristicUUID(CharacteristicUUID);
//  bt.ServiceUUID("F000");
//  bt.CharacteristicUUID("F002");
  bt.setVOl(30);
  delay(100);
  bt.switchFunction(bt.eBluetooth);
  delay(100);
  bt.reset();
  delay(1500);

  runner.init();
  runner.addTask(CycleModeTask);
  runner.addTask(buttonTask);
  runner.addTask(bluetoothConnectionTask);
  delay(100);
  buttonTask.enable();
  bluetoothConnectionTask.enable();
  CycleModeTask.enable();
}


void loop() {
  runner.execute();
}
