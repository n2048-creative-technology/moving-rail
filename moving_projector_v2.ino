
//////////////////////////////////////////////
//        RemoteXY include library          //
//////////////////////////////////////////////

// RemoteXY select connection mode and include library
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <AccelStepper.h>


// COMMANDS:
struct {
  String help = "help";
  String clear = "clear";
  String home = "home";
  String rec = "rec";
  String debug = "debug";  // set debug mode [0|1]
  String save = "save";    // save [long tarrget ]
  String run = "run";      // sets run mode
  String stop = "stop";    // sets learning mode
  String mem = "mem";      // reads recorded EEPROM contents;
  String pos = "pos";      // reads curent count;
} Commands;

String inputString = "";      // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete

int rec = 0;
int clear = 0;
int debug = 0;
int mode = 0;


// ROTARY ENCODER
#define PinA 4  // D2
#define PinB 5  // D1

// STEPPER DRIVER
#define PinDir 12   // D6
#define PinStep 13  // D7
// #define PinEna 16   // D0
#define PinEna 15  // D8

#define PinLed 2  // D4

#define PinLimitSwitch1 14  // D5
// #define PinLimitSwitch2 15  // D8
#define PinLimitSwitch2 16  // D0

#define button 0  // D3 // connected to FLASH button, boot fails if pulled LOW (needs normal 5V)

AccelStepper stepper(AccelStepper::DRIVER, PinStep, PinDir);

volatile int flagA = 0;
volatile int flagB = 0;
volatile int counter = 0;
int last_counter = 0;
volatile int maxCount = 17000;
int target = 0;

int homed = 1;
int homing_step = 0;

long last_pressed = 0;
long press_start = 0;
int button_pressed = 0;
int action = 0;

int isRunning = 0;
int speed = 6000;

// for EEPROM
int memSize = 512;
int positionsStored = 0;
int addrRun = 0;
int addrLearn = 0;
int lastSavedTarget = -1;

String convert;

ICACHE_RAM_ATTR void handleInterruptA() {
  flagA = digitalRead(PinA);
  flagB = digitalRead(PinB);
  if (flagA && !flagB) {
    counter += 1;
  }
}

ICACHE_RAM_ATTR void handleInterruptB() {
  flagA = digitalRead(PinA);
  flagB = digitalRead(PinB);
  if (flagB && !flagA) {
    counter -= 1;
  }
}

void getNextTarget() {
  if (addrRun == positionsStored * 2) { addrRun = 0; }
  target = readIntFromEEPROM(addrRun);
  addrRun += 2;  // increment 2 because int uses 2 bytes
}

void setup() {
  EEPROM.begin(memSize);

  // TODO you setup code

  pinMode(PinA, INPUT_PULLUP);
  pinMode(PinB, INPUT_PULLUP);
  pinMode(PinLimitSwitch1, INPUT_PULLUP);
  pinMode(PinLimitSwitch2, INPUT_PULLDOWN_16);
  pinMode(button, INPUT_PULLUP);
  pinMode(PinLed, OUTPUT);
  digitalWrite(PinLed, HIGH);
  attachInterrupt(digitalPinToInterrupt(PinA), handleInterruptA, RISING);
  attachInterrupt(digitalPinToInterrupt(PinB), handleInterruptB, RISING);

  stepper.setMaxSpeed(10000);
  stepper.setAcceleration(1000);
  stepper.setEnablePin(PinEna);
  stepper.setPinsInverted(true, false, true);

  Serial.begin(115200);
}

// positive numbers from 0 to 60.000 wil be readd as positions
// nevative numbers wil be read as pause in milliseconds
void writeIntIntoEEPROM(int address, int number) {
  EEPROM.write(address, number >> 8);
  EEPROM.write(address + 1, number & 0xFF);
}

// positive numbers from 0 to 60.000 wil be readd as positions
// nevative numbers wil be read as pause in milliseconds
int readIntFromEEPROM(int address) {
  int number = (EEPROM.read(address) << 8) + EEPROM.read(address + 1);
  if (number > 60000) {
    return number - 65536;
  }
  return number;
}

void cleanMemory() {
  positionsStored = 0;
  for (int i = 0; i < memSize; i++) { EEPROM.write(i, 0); }
  if (debug) Serial.println("Memory erased");
  addrLearn = 0;
  addrRun = 0;
}

void loop() {

  rec = 0;
  clear = 0;

  runSerialCommands();

  if (homed == 0) {
    digitalWrite(PinLed, LOW);
    stepper.enableOutputs();

    if (homing_step == 0) {
      if (digitalRead(PinLimitSwitch1) == HIGH) {
        counter = 0;
        homing_step = 1;
      } else {
        stepper.setSpeed(-speed);
        stepper.runSpeed();
      }
    }
    if (homing_step == 1) {

      if (digitalRead(PinLimitSwitch2) == HIGH) {
        maxCount = counter;
        homing_step = 2;
      } else {
        stepper.setSpeed(speed);
        stepper.runSpeed();
      }
    }
    if (homing_step == 2) {
      if (digitalRead(PinLimitSwitch1) == HIGH) {
        counter = 0;
        homed = 1;
        stepper.stop();
        digitalWrite(PinLed, HIGH);
      } else {
        stepper.setSpeed(-speed);
        stepper.runSpeed();
      }
    }

    return;
  }

  if (digitalRead(PinLimitSwitch1) == HIGH) {
    counter = 0;
  }
  if (digitalRead(PinLimitSwitch2) == HIGH) {
    maxCount = counter;
  }

  if (digitalRead(button) == LOW) {
    if (millis() - last_pressed > 100) {  // debounce
      // Serial.println("Button Pressed");
      if (debug) Serial.print(".");
      button_pressed = 1;
      press_start = millis();
      action = 1;
    }

    if (action < 2 && millis() - press_start > 1000) {
      if (debug) Serial.print(".");
      action = 2;
    }

    if (action < 3 && millis() - press_start > 5000) {
      if (debug) Serial.print(".");
      action = 3;
    }

    last_pressed = millis();
  } else {

    if (action && button_pressed) {
      if (debug) Serial.println("");

      if (action == 1) {
        if (!mode) {
          rec = 1;
        }
      }
      if (action == 2) {
        mode = !mode;
        if (debug) {
          Serial.print("Switching to ");
          Serial.print(mode ? "RUN" : "LEARN");
          Serial.println(" mode ...");
        }
      }
      if (action == 3) {
        clear = 1;
      }

      // Clear Flags
      action = 0;
      button_pressed = 0;
    }
  }

  if (clear) {
    cleanMemory();
  }

  // TODO you loop code
  // use the RemoteXY structure for data transfer
  // // do not call delay()

  if (mode == 1) {

    if (positionsStored == 0) {
      if (debug) Serial.println("memory is empty");
      mode = 0;
      return;
    }

    // RUNNING SEQUENCE
    digitalWrite(PinLed, LOW);
    stepper.enableOutputs();

    if (isRunning == 0) {
      // just started:
      if (debug) Serial.println("Running...");
      getNextTarget();
    }
    isRunning = 1;

    if (target < 0) {
      if (debug) {
        Serial.print("Pause ");
        Serial.print(-target);
        Serial.println("ms ");
      }
      stepper.stop();
      delay(-target);
      getNextTarget();
      return;
    }

    // move to target
    target = constrain(target, 0, maxCount);

    // stepper.moveTo(target * enc2Step);
    // stepper.runToPosition();
    // getNextTarget();

    int distance_to_target = target - counter;

    if (abs(distance_to_target) > 0) {
      int sign = distance_to_target > 0 ? 1 : -1;
      stepper.setSpeed(speed * sign);
    } else {
      // target reached --> top moving and get next target
      stepper.stop();
      getNextTarget();
      if (counter == target) {
        if (debug) Serial.println("The only target in memoryy was reached, automatic stop");
        mode = 0;
      }
    }

    stepper.runSpeed();

    if (last_counter != counter) {
      // Serial.println(counter);
      last_counter = counter;
    }
  } else {

    // LEARNING MODE

    if (last_counter != counter) {
      if (debug) {
        Serial.print("Counter: ");
        Serial.println(counter);
      }
      last_counter = counter;
    }

    // Position manually and press REC
    // Disable Motor Diver (free motor)
    // digitalWrite(PinEna, HIGH);
    stepper.disableOutputs();
    digitalWrite(PinLed, HIGH);

    if (isRunning == 1) {
      // just started:

      if (debug) Serial.println("Learning...");
      addrLearn = positionsStored * 2;  // Go to last stored position
    }
    isRunning = 0;

    // use  current position as target;
    target = constrain(counter, 0, maxCount);

    // OR: to learn with Joystick:
    // target += map(RemoteXY.joystick_x, -100, 100, -5, 5);

    if (addrLearn < memSize && rec && target != lastSavedTarget) {

      if (rec || (millis() - last_pressed > 200)) {  // debounce

        lastSavedTarget = target;

        writeIntIntoEEPROM(addrLearn, target);
        addrLearn += 2;  // increment 2 because stored int uses 2 bytes
        positionsStored += 1;

        if (debug) {
          Serial.print("Position ");
          Serial.print(counter);
          Serial.println(" Recorded");
        }
      }
      last_pressed = millis();
    }
  }
}


void runSerialCommands() {
  if (stringComplete) {

    String received = inputString;

    // clear the string:
    inputString = "";
    stringComplete = false;

    String command = getValue(received, ' ', 0);

    /** 
      Command: led     
    **/

    if (command == Commands.pos) {
      if (debug) Serial.println("Command: pos ");
      Serial.println(counter);
      float p = (float)stepper.currentPosition();
      Serial.println(p);

      // uint32_t p = stepper.currentPosition();
      // char buff[128];
      // sprintf(buff, "0x%lX%lX", (uint32_t)(p >> 32), (uint32_t)( p & 0xFFFFFFFFUL));
      // Serial.println(buff);
      return;
    }

    if (command == Commands.home) {
      if (debug) Serial.println("Command: home ");
      homed = 0;
      homing_step = 0;
      Serial.println(1);
      return;
    }

    if (command == Commands.run) {
      if (debug) Serial.println("Command: run ");
      mode = 1;

      Serial.println(1);
      return;
    }

    if (command == Commands.stop) {
      if (debug) Serial.println("Command: stop ");
      // learning mode, disable driver
      mode = 0;
      // stop homing sequence too.
      homed = 1;
      homing_step = 0;
      Serial.println(1);
      return;
    }

    if (command == Commands.rec) {
      if (debug) Serial.println("Command: rec ");
      if (mode) {
        if (debug) Serial.println("Cannot record position while running!");
        Serial.println(0);
        return;
      }
      Serial.println(1);
      rec = 1;
      return;
    }

    if (command == Commands.clear) {
      if (debug) Serial.println("Command: clear ");
      if (mode) {
        if (debug) Serial.println("Cannot cear while running!");
        Serial.println(0);
        return;
      }
      Serial.println(1);
      clear = 1;
      return;
    }

    if (command == Commands.mem) {
      if (debug) Serial.println("Command: mem ");
      for (int i = 0; i < positionsStored; i++) {
        long v = readIntFromEEPROM(i * 2);
        Serial.println(v);
      }
      return;
    }

    if (command == Commands.save) {
      if (debug) Serial.println("Command: save ");
      if (mode) {
        if (debug) Serial.println("Cannot save while running!");
        return;
      }
      if (addrLearn < memSize) {
        String value = getValue(received, ' ', 1);
        target = strtoll(value.c_str(), nullptr, 10);
        lastSavedTarget = target;
        writeIntIntoEEPROM(addrLearn, target);
        addrLearn += 2;  // increment 2 because stored int uses 2 bytes
        positionsStored += 1;
        Serial.println(1);
      } else Serial.println(0);
      return;
    }

    if (command == Commands.debug) {
      if (debug) Serial.print("Command: debug ");
      String value = getValue(received, ' ', 1);
      long v = strtoll(value.c_str(), nullptr, 10);
      debug = v;
      Serial.println(debug);
      return;
    }
    if (command == Commands.help || command == "?") {
      Serial.println("");
      Serial.println("*******************");
      Serial.println("Available commands:");
      Serial.println("*******************");
      Serial.println(Commands.help + "\t?\tShows this hellp");
      Serial.println(Commands.debug + "\t0|1\tActivates or deactivates debug mode");
      Serial.println(Commands.run + "\t\tRuns motion sequence");
      Serial.println(Commands.stop + "\t\tStops motion, activates learn mode");
      Serial.println(Commands.home + "\t\tTrigger homing sequence");
      Serial.println(Commands.pos + "\t\tShow current position counter");
      Serial.println(Commands.save + " \tlong\tAdds new position value in EEPROM");
      Serial.println(Commands.rec + "\t\tStores current position counter in EEPROM");
      Serial.println(Commands.clear + "\t\tClear EEPROM");
      Serial.println(Commands.mem + "\t\tShows EEPROM contents");
      Serial.println("*******************");
      Serial.println("");
      return;
    }

    if (debug) {
      Serial.print("Unrecognized Command: ");
      Serial.println(command);
    }
  }
}
String getValue(String data, char separator, int index) {
  data.trim();
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : data;
}

void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    inputString.toLowerCase();
    // if the incoming character is a newline, set a flag so the main loop can
    // do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}