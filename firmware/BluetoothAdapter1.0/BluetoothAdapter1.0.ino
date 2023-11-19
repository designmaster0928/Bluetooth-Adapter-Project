
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
#define STBY 0  // when the battery charign is completed, STBY is pulled low, otherwise, it will be High impedance.
#define CHRG 1  // when charger is charing the battery, CHRG is pulled low, otherwise, it will be High impedance.

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

const char* BleName = "KLEIN_PTT";

uint16_t powerOffCntDown = POWEROFF_COUNT;  // buttonCallbackTime = 10ms; set powerOfftime as 2s
bool powerOnOff = false;

bool bluetoothConnection = false;

uint16_t batLevel = 0;
bool batLedBlink = false;
uint8_t batLedBlinkCnt = 0;

void rgbLed(bool r, bool g, bool b) {
  digitalWrite(RED, !r);
  digitalWrite(GREEN, !g);
  digitalWrite(BLUE, !b);
}

void powerON() {
  digitalWrite(P_CON, HIGH);
  rgbLed(1, 1, 1);
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
  if (bt.btStatus()) {
    if (swMode) rgbLed(0, 0, 1);
    else rgbLed(1, 0, 1);
    bluetoothConnection = true;
  } else {
    rgbLed(1, 1, 1);
    bluetoothConnection = false;
  }
}
void cycleModesCallback() {
  if (!(digitalRead(STBY) ^ digitalRead(CHRG))) {
    batLevel = analogRead(BAT_IN) * 0.488; // 500 / 1024 = 0.488;
    if (batLevel < 313) {  // <10% : 3.12V is 10% of max 4.2V
      batLedBlinkCnt++;
      CycleModeTask.setInterval(BATTERY_SHOW_TIME);
      batLedBlink = !batLedBlink;
      rgbLed(batLedBlink, 0, 0);  // RED color blinking
      if (batLedBlinkCnt > 20) {
        batLedBlinkCnt = 0;
        normalLedStatus();
      }
    }
    else {
      normalLedStatus();
    }
  }
}
void bluetoothConnectionCalback() {
  if (bt.btStatus()) {
    bluetoothConnectionTask.disable();
    runner.deleteTask(bluetoothConnectionTask);
    rgbLed(0, 0, 1);
    bluetoothConnection = true;
  }
}
void buttonCallback() {
  KEY.loop();     // MUST call the loop() function first
  PTT_IN.loop();  // MUST call the loop() function first

  if (KEY.isPressed()) {
    powerOnOff = true;
    //powerOFF();
  }
  if (KEY.isReleased()) {
    powerOnOff = false;
    if (bluetoothConnection) {
      swMode = !swMode;
      if (swMode) rgbLed(0, 0, 1);
      else rgbLed(1, 0, 1);
    }
  }
  if (powerOnOff) {
    if (powerOffCntDown != 0)
      powerOffCntDown--;
    else
      powerOFF();
  } else {
    powerOffCntDown = POWEROFF_COUNT;
  }
  if (bluetoothConnection) {
    if (PTT_IN.isPressed()) {
      if (swMode)
        bt.sendData("+PTTS=P");
      else
        bt.playPause();
    }
    if (PTT_IN.isReleased()) {
      if (swMode)
        bt.sendData("+PTTS=R");
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
  delay(2000);
  while (!bt.begin(Serial)) {
    delay(1000);
  }
  bt.btName(BleName);
  delay(1000);
  bt.reset();
  delay(1000);
  bt.bleOnOff(0);  // turn off BLE
  bt.setVOl(30);
  bt.switchFunction(bt.eBluetooth);
  delay(1000);

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
