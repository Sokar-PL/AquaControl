
//Definitions
#define WITH_LCD 1

#include <ClickEncoder.h>
#include <TimerOne.h>
#include <Wire.h>   // standardowa biblioteka Arduino
#include <RTClib.h>
#include <TimeAlarms.h>
#include <Time.h>

#ifdef WITH_LCD
#include <LiquidCrystal_I2C.h> // dolaczenie pobranej biblioteki I2C dla LCD
#endif

// Creating objects
LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Ustawienie adresu ukladu na 0x3F
RTC_DS1307 RTC;

//------------****************Setting variables
ClickEncoder *encoder;

String ver = "AquaCtrl v0.7.3";


// mENU RELEATED
long last, value, lastPosition, tempValue;
boolean isRunning, inMenu = false;
boolean set, pwmSet, firstRun, nowRunning = true;
const char* menu[] =
{"> Data/Godzina", "> Max. os. noc", "> Max. os. dzien", "> Poczatek dnia", "> Koniec dnia", "> Tryb reczny", "> Ust. tr. recz.", "> Wersja FW"};
const char* menuManual[] =
{"> Wl/Wy os.dzien", "> Moc os. dzien", "> Wl/Wy os. noc", "> Moc os. noc", "> Wyjscie"};



// Date Releated
//const char* days[] =
//{"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
//const char* months[] =
//{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

int dateTaken[3] = {2017, 01, 01};
int timeTaken[2] = {12, 00};

int poczatekDnia[2] = {14, 00};
int koniecDnia[2] = {22, 00};


// TODO: This is usefoul to check length of the array (depends on variable)
//int mysize = sizeof(months) / sizeof(char);
//Serial.print(String(mysize));

// Clock releated
int  lcdLastHour, lcdLastMinute, lcdLastSecond;

//pwm releated
int pwmDzien = 0;
int pwmNoc = 0;
int maxPwmDzien = 110;
int maxPwmNoc = 110;
int lastPwmDzien, lastPwmNoc;
int manualPwmDzien = 255;
int manualPwmNoc = 0;
int tempPwmDzien, tempPwmNoc, tempOnOffPwmDzien, tempOnOffPwmNoc;
int nocPin = 5;
int dzienPin = 6;

boolean manualMode = false;
boolean nightIsNow = true;
boolean pwmControllIsStarted = true;
int pwmCount = 0;
long calculatedTimeOn, calculatedTimeOff, calculatedMaxDayInndex, calculatedMaxNightInndex, calculatedOnTimePeriod;
long calculatedOffsetOn, calculatedOffsetOff, calculatedMaxPwmDay;
long calculatedPwmIncreaseDecrease, calculatedMaxPwmTime, calculatedNormalTime;
int calculatedTimerIncreaseDecrease, calculatedTimerNormal;
AlarmID_t increaseTimerDay, normalTimerUp, normalTimerDown, decreaseTimerDay, dupa;
boolean isIncreaseRunning = false;
boolean isNormalUpRunning = false;
boolean isNormalDownRunning = false;
boolean isDecreasingRunning = false;

//---------------------------------------------------------------Methods

uint32_t syncProvider()
{
  return RTC.now().unixtime();
}

void timerIsr() {
  encoder->service();
}

//#ifdef WITH_LCD
//void displayAccelerationStatus() {
//  lcd.setCursor(0, 1);
//  lcd.print("Acceleration ");
//  lcd.print(encoder->getAccelerationEnabled() ? "on " : "off");
//}
//#endif


//----------------Main setup
void setup() {
  // only for debuging - remove later
  Serial.begin(9600);

  //  Enc PIN1, Enc PIN2, BTN PIN, stepsPerClick, LOW/HIGH
  encoder = new ClickEncoder(2, 4, 8, 4);
  pinMode(nocPin, OUTPUT);
  pinMode(dzienPin, OUTPUT);

  //RTC set
  RTC.begin();

#ifdef WITH_LCD
  //LCD set
  lcd.begin(20, 2);
  lcd.backlight();
  lcd.home();
  lcd.print("Startuje...");
  lcd.setCursor(0, 1);
  lcd.print(ver);
#endif

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);

  // Setting starting values
  last = -1;
  analogWrite(dzienPin, pwmDzien);
  analogWrite(nocPin, pwmNoc);
  //  lastPwmDzien = pwmDzien;
  //  lastPwmNoc = pwmNoc;



  calculatedTimeOn = calculateTime(poczatekDnia[0], poczatekDnia[1], 0);//seconds from 00:00 to target ON time
  calculatedTimeOff = calculateTime(koniecDnia[0], koniecDnia[1], 0); //seconds from 00:00 to target OFF time

  //  calculatedMaxDayInndex = maxPossiblePwmIndex(maxPwmDzien , true);
  //  calculatedMaxNightInndex = maxPossiblePwmIndex(maxPwmNoc , false);

  calculatedOnTimePeriod = calculatedTimeOff - calculatedTimeOn; // Light on period in s
//  Serial.print("calculatedOnTimePeriod ");
//  Serial.println(calculatedOnTimePeriod);

  calculatedPwmIncreaseDecrease = calculatedOnTimePeriod * 0.125; //period time in s for increase and decrease light time
//  Serial.print("increase decrease period: ");
//  Serial.println(calculatedPwmIncreaseDecrease);

  calculatedMaxPwmTime = calculatedPwmIncreaseDecrease; // maximum light time in s
//  Serial.print("max time period : ");
//  Serial.println(calculatedMaxPwmTime);

  calculatedNormalTime = calculatedOnTimePeriod * 0.3125; // normal 1125

//  Serial.print("NormalTime time period : ");
//  Serial.println(calculatedNormalTime);

  calculatedTimerIncreaseDecrease = 1 / ((maxPwmDzien * 0.6) / calculatedPwmIncreaseDecrease); // step in seconds for ewvery 1PWM
  calculatedTimerNormal = 1 / ((maxPwmDzien * 0.4) / calculatedNormalTime); // step in seconds for every 1PWM from 60% to 100% (total possible)

  setTime(RTC.now().hour(), RTC.now().minute(), RTC.now().second(), RTC.now().day(), RTC.now().month(), RTC.now().year());

  increaseTimerDay = Alarm.timerRepeat(calculatedTimerIncreaseDecrease, pwmFastIncreaseDay); // this will increase 1PWM per 25s up to 60% - świt
  Alarm.disable(increaseTimerDay); //Disabling it for now
//  Serial.print("calculatedTimerIncreaseDecrease ");
//  Serial.println(calculatedTimerIncreaseDecrease);

  normalTimerUp = Alarm.timerRepeat(calculatedTimerNormal, pwmNormalIncreaseDay); // normal increase from 60% up to 100% (of the allowed
  Alarm.disable(normalTimerUp);
//  Serial.print("calculatedTimerNormal ");
//  Serial.println(calculatedTimerNormal);

  normalTimerDown = Alarm.timerRepeat(calculatedTimerNormal, pwmNormalDecreaseDay);
  Alarm.disable(normalTimerDown);

  decreaseTimerDay = Alarm.timerRepeat(calculatedTimerIncreaseDecrease, pwmFastDecreaseDay); // for increase this will decrease 1PWM per 25s from 60% to 0%
  Alarm.disable(decreaseTimerDay);
//  Serial.print("calculatedTimerIncreaseDecrease ");
//  Serial.println(calculatedTimerIncreaseDecrease);


  Alarm.delay(2000);
  lcd.clear();

}

//------------------ END OF MAIN SETUP







// ******************* METHODS  STARTING HERE
void recalculateEverything() {
  calculatedTimeOn = calculateTime(poczatekDnia[0], poczatekDnia[1], 0);
  calculatedTimeOff = calculateTime(koniecDnia[0], koniecDnia[1], 0);
  calculatedOnTimePeriod = calculatedTimeOff - calculatedTimeOn; // Light on period in s
  calculatedPwmIncreaseDecrease = calculatedOnTimePeriod * 0.125; //period time in s for increase and decrease light time
  calculatedMaxPwmTime = calculatedPwmIncreaseDecrease; // maximum light time in s
  calculatedNormalTime = calculatedOnTimePeriod * 0.3125; // normal
  calculatedTimerIncreaseDecrease = 1 / ((maxPwmDzien * 0.6) / calculatedPwmIncreaseDecrease); // step in seconds for ewvery 1PWM
  calculatedTimerNormal = 1 / ((maxPwmDzien * 0.4) / calculatedNormalTime); // step in seconds for every 1PWM from 60% to 100% (total possible)

  Alarm.write(increaseTimerDay, calculatedTimerIncreaseDecrease);
  Alarm.disable(increaseTimerDay);

  Alarm.write(normalTimerUp, calculatedTimerNormal);
  Alarm.disable(normalTimerUp);

  Alarm.write(normalTimerDown, calculatedTimerNormal);
  Alarm.disable(normalTimerDown);

  Alarm.write(decreaseTimerDay, calculatedTimerIncreaseDecrease);
  Alarm.disable(decreaseTimerDay);
//  Serial.println("*/*/**/*/* RECALCULATION/*/*/*/*/*/*");
//
//
//  Serial.print("calculatedOnTimePeriod ");
//  Serial.println(calculatedOnTimePeriod);
//
//  Serial.print("increase decrease period: ");
//  Serial.println(calculatedPwmIncreaseDecrease);
//
//  Serial.print("max time period : ");
//  Serial.println(calculatedMaxPwmTime);
//
//  Serial.print("NormalTime time period : ");
//  Serial.println(calculatedNormalTime);


  isIncreaseRunning = false;
  isNormalUpRunning = false;
  isNormalDownRunning = false;
  isDecreasingRunning = false;

//  Serial.println("*/*/**/*/*/*/*/*/*/*/*");
//  Serial.print("calculatedTimerIncreaseDecrease ");
//  Serial.println(calculatedTimerIncreaseDecrease);
//  Serial.print("calculatedTimerNormal ");
//  Serial.println(calculatedTimerNormal);
//  Serial.print("calculatedTimerIncreaseDecrease ");
//  Serial.println(calculatedTimerIncreaseDecrease);
//  Serial.println("*/*/**/*/* RECALCULATION END/*/*/*/*/*/*");

}


void printLcd(boolean reedraw, int row, int line, String message) { // will print on LCD
  // Function will print message on LCD. It can redraw, print on specific line/row.
  if (reedraw) {
    for (int i = 0; i <= 15; i++) {
      lcd.setCursor(i, line);
      lcd.print(" ");
    }
  }
  lcd.setCursor(row, line);
  lcd.print(message);
}

void printLcdtime() {
  // Function will print time on LCD line 0

  lcd.home();
  if (hour() < 10) {
    lcd.print("0");
  }
  lcd.print(hour() + String(":"));
  if (minute() < 10) {
    lcd.print("0");
  }
  lcd.print(minute() +  String(":"));
  if (second() < 10) {
    lcd.print("0");
  }
  lcd.print(second());
}

void printLcdDate() {
  // Function will print date in LCD line 1
  lcd.setCursor(0, 1);
  if (day() < 10) {
    lcd.print("0");
  }
  lcd.print(day() + String("-"));
  lcd.print(month() + String("-"));
  lcd.print(year());
}

int printPwm(int pin, int pwm) {
  // Function will set LIVE PWM for specific channel and return value
  nowRunning = true;
  int lastKnownPwm = pwm;
  printLcd(1, 0, 1, String((float(pwm) * 100.00) / 255.00) + "%");
  // TODO: add sentinel timer
  while (nowRunning) {
    lastPosition = last;
    if (checkRotation()) {
      pwm = setPwmValue(pwm, lastPosition);
      printLcd(1, 0, 1, String((float(pwm) * 100.00) / 255.00) + "%");
      analogWrite(pin, pwm);
    }
    ClickEncoder::Button m = encoder->getButton();
    if (m != ClickEncoder::Open) {
      nowRunning = false;
    }
    Alarm.delay(50);
  }
  //  lastPosition = 0;
  analogWrite(pin, lastKnownPwm);
  return pwm;
}

boolean checkRotation() {
  // Method will check whenever rotator rotated :)
  value += encoder->getValue();
  if (value != last) {
    last = value;
    return true;
  } else {
    return false;
  }
}

int setPwmValue(int pwmValue, int lastPosition) {
  if (value < lastPosition) {
    if (pwmValue >= 0 && pwmValue != 0) {
      pwmValue--;
    }
  } else if (value > lastPosition) {
    if (pwmValue <= 254 && pwmValue != 255) {
      pwmValue++;
    }
  }
  return pwmValue;
}

int setDateValue(int valueToSet, int lastPosition, int index) {
  if (value < lastPosition) {
    if (valueToSet >= 1 && valueToSet != 1) {
      valueToSet--;
    }
  } else if (value > lastPosition) {
    switch (index) {
      case (0):
        valueToSet++;
        break;
      case (1):
        if (valueToSet <= 12 && valueToSet != 12) {
          valueToSet++;
        }
        break;
      case (2):
        if (valueToSet <= 31 && valueToSet != 31) {
          valueToSet++;
        }
        break;
    }
  }
  return valueToSet;
}

int setTimeValue(int valueToSet, int lastPosition, int index) {
  if (value < lastPosition) {
    if (valueToSet >= 0 && valueToSet != 0) {
      valueToSet--;
    }
  } else if (value > lastPosition) {
    switch (index) {
      case (0):
        if (valueToSet <= 24 && valueToSet != 24)
          valueToSet++;
        break;
      case (1 || 2):
        if (valueToSet <= 59 && valueToSet != 59) {
          valueToSet++;
        }
    }
  }
  return valueToSet;
}

void setDateTime() {
  // This Method is responsible for changing Date and Time on RTC
  int index;

  printLcd(1, 0, 0, "Ustaw date...");
  printLcd(1, 0, 1, String(dateTaken[0]) + "-" + String(dateTaken[1]) + "-" + String(dateTaken[2]));

  lcd.blink();
  for (index = 0; index < ((sizeof(dateTaken) / sizeof(dateTaken[0]))); index = index + 1) {
    nowRunning = true;
    while (nowRunning) {
      //      lcd.setCursor((3 + index), 1);
      lastPosition = last;
      if (checkRotation()) {
        dateTaken[index] = setDateValue(dateTaken[index], lastPosition, index);
        printLcd(1, 0, 1, String(dateTaken[0]) + "-" + String(dateTaken[1]) + "-" + String(dateTaken[2]));
      }
      ClickEncoder::Button m = encoder->getButton();
      if (m != ClickEncoder::Open) {
        nowRunning = false;
      }
      Alarm.delay(50);
    }
  }

  printLcd(1, 0, 0, "Ustaw godzine...");
  printLcd(1, 0, 1, String(timeTaken[0]) + ":0" + String(timeTaken[1]) + ":00");

  for (index = 0; index < ((sizeof(timeTaken) / sizeof(timeTaken[0]))); index = index + 1) {
    nowRunning = true;
    while (nowRunning) {
      lastPosition = last;
      if (checkRotation()) {
        timeTaken[index] = setTimeValue(timeTaken[index], lastPosition, index);
        if (index >= 1 && timeTaken[index] <= 9) {
          printLcd(1, 0, 1, String(timeTaken[0]) + ":0" + String(timeTaken[1]) + ":00");
        } else {
          printLcd(1, 0, 1, String(timeTaken[0]) + ":" + String(timeTaken[1]) + ":00");
        }
      }
      ClickEncoder::Button m = encoder->getButton();
      if (m != ClickEncoder::Open) {
        nowRunning = false;
      }
      Alarm.delay(50);
    }

  }
  lcd.noBlink();
  RTC.adjust(DateTime(dateTaken[0], dateTaken[1], dateTaken[2], timeTaken[0], timeTaken[1], 00));
  //  setTime(timeTaken[0], timeTaken[1], 00), dateTaken[0], dateTaken[1], dateTaken[2]);
  setSyncProvider(syncProvider);
  recalculateEverything();
}

void printLcdPwmValues(boolean forceChange) {
  // Will display current PWM for day and night
  if (manualMode) {
    pwmDzien = manualPwmDzien;
    pwmNoc = manualPwmNoc;
  }

  if (lastPwmDzien != pwmDzien || forceChange) {
    printLcd(0, 0, 1, "D" + String((float(pwmDzien) * 100.00) / 255.00) + "%");
    lastPwmDzien = pwmDzien;
  }

  if (lastPwmNoc != pwmNoc || forceChange) {
    printLcd(0, 8, 1, "N" + String((float(pwmNoc) * 100.00) / 255.00) + "%");
    lastPwmNoc = pwmNoc;
  }
}

boolean setmanualMode(boolean modeManual) {
  int poss;
  printLcd(1, 0, 0, "Tryb reczny?");
  printLcd(1, 1, 1, "TAK");
  printLcd(0, 8, 1, "NIE");
  if (modeManual) {
    poss = 0;
    printLcd(0, 0, 1, "*");
  } else {
    tempPwmDzien = pwmDzien;
    tempPwmNoc = pwmNoc;
    poss = 7;
    printLcd(0, 7, 1, "*");
  }
  nowRunning = true;
  while (nowRunning) {
    lastPosition = last;
    if (checkRotation()) {
      if (value < lastPosition) {
        printLcd(0, poss, 1, " ");
        poss = 0;
        modeManual = true;
        printLcd(0, poss, 1, "*");
      } else if (value > lastPosition) {
        printLcd(0, poss, 1, " ");
        poss = 7;
        modeManual = false;
        printLcd(0, poss, 1, "*");
      }
    }
    ClickEncoder::Button m = encoder->getButton();
    if (m != ClickEncoder::Open) {
      if (modeManual) {
        pwmDzien = manualPwmDzien;
        pwmNoc = manualPwmNoc;
      } else {
        pwmDzien = tempPwmDzien;
        pwmNoc = tempPwmNoc;
      }
      analogWrite(nocPin, pwmNoc);
      analogWrite(dzienPin, pwmDzien);
      nowRunning = false;
    }
    Alarm.delay(50);
  }
  return modeManual;
}

int onOffLed(String message, int pin, int pwm) {
  int poss;
  boolean isOn;
  if (pwm >= 1) {
    isOn = true;
  } else {
    isOn = false;
  }
  printLcd(1, 0, 0, "Wl/Wyl os. " + message);
  printLcd(1, 1, 1, "WLACZ");
  printLcd(0, 8, 1, "WYLACZ");
  if (isOn) {
    poss = 0;
    printLcd(0, 0, 1, "*");
  } else {
    poss = 7;
    printLcd(0, 7, 1, "*");
  }
  nowRunning = true;
  while (nowRunning) {
    lastPosition = last;
    if (checkRotation()) {
      if (value < lastPosition) {
        printLcd(0, poss, 1, " ");
        poss = 0;
        isOn = true;
        analogWrite(pin, pwm);
        printLcd(0, poss, 1, "*");
      } else if (value > lastPosition) {
        printLcd(0, poss, 1, " ");
        poss = 7;
        isOn = false;
        analogWrite(pin, 0);
        pwm = 0;
        printLcd(0, poss, 1, "*");
      }
    }
    ClickEncoder::Button m = encoder->getButton();
    if (m != ClickEncoder::Open) {
      nowRunning = false;
    }
    Alarm.delay(50);

  }
  return pwm;
}



int manualSettingsMenu() {
  int temManualValue = 0;

  if (!manualMode) {
    printLcd(1, 0, 0, "Prosze wlaczyc");
    printLcd(1, 0, 1, "tryb reczny.");
    Alarm.delay(4000);
    return 0;
  }
  printLcd(1, 0, 0, "Menu tr.recznego");
  nowRunning = true;
  while (nowRunning) {

    if (checkRotation()) {
      if (value >= (sizeof(menuManual) / sizeof(menuManual[0]))) {
        value = (sizeof(menuManual) / sizeof(menuManual[0])) - 1;
      } else if (value < 0) {
        value = 0;
      }
      printLcd(1, 0, 1, menuManual[value]);
    }

    ClickEncoder::Button b = encoder->getButton();
    if (manualMode && b != ClickEncoder::Open) {
      switch (b) {
        case ClickEncoder::Clicked:
          switch (value) {
            case (0):
              //              wł/wył os dzien
              temManualValue = value;
              printLcd(1, 0, 0, "Wl/Wyl os. dzien.");
              if (manualPwmDzien != 0) {
                tempOnOffPwmDzien = manualPwmDzien;
              }
              manualPwmDzien = onOffLed("Dzien", dzienPin, tempOnOffPwmDzien);
              value = temManualValue;
              printLcd(1, 0, 0, "Menu tr.recznego");
              printLcd(1, 0, 1, menuManual[value]);
              Alarm.delay(2000);
              continue;
            case (1):
              //              jasnosc tryb dzien
              temManualValue = value;
              printLcd(1, 0, 0, "Moc. os. dzien.");
              manualPwmDzien = printPwm(dzienPin, manualPwmDzien);
              //              manualPwmDzien = pwmDzien;
              value = temManualValue;
              printLcd(1, 0, 0, "Menu tr.recznego");
              printLcd(1, 0, 1, menuManual[value]);
              continue;
            case (2):
              //              wl/wyl tryb noc
              temManualValue = value;
              if (manualPwmNoc != 0) {
                tempOnOffPwmNoc = manualPwmNoc;
              }
              printLcd(1, 0, 0, "Wl/Wyl os. noc.");
              manualPwmNoc = onOffLed("Noc", dzienPin, tempOnOffPwmNoc);
              value = temManualValue;
              printLcd(1, 0, 0, "Menu tr.recznego");
              printLcd(1, 0, 1, menuManual[value]);
              continue;
            case (3):
              //              jasnosc tryb noc
              temManualValue = value;
              printLcd(1, 0, 0, "Moc. os. noc.");
              pwmNoc = printPwm(nocPin, manualPwmNoc);
              manualPwmNoc = pwmNoc;
              value = temManualValue;
              printLcd(1, 0, 0, "Menu tr.recznego");
              printLcd(1, 0, 1, menuManual[value]);
              continue;
            case (4):
              return 0;

          }
      }
    }
  }
}


void endHour() {

  // This Method is responsible for changing END Time
  int index;

  printLcd(1, 0, 0, "Ust. koniec dnia");
  printLcd(1, 0, 1, String(koniecDnia[0]) + ":0" + String(koniecDnia[1]) + ":00");

  lcd.blink();
  for (index = 0; index <= ((sizeof(koniecDnia) / sizeof(koniecDnia[0]))); index = index + 1) {
    nowRunning = true;
    while (nowRunning) {
      lastPosition = last;
      if (checkRotation()) {
        koniecDnia[index] = setTimeValue(koniecDnia[index], lastPosition, index);
        if (index >= 1 && koniecDnia[index] <= 9) {
          printLcd(1, 0, 1, String(koniecDnia[0]) + ":0" + String(koniecDnia[1]) + ":00");
        } else {
          printLcd(1, 0, 1, String(koniecDnia[0]) + ":" + String(koniecDnia[1]) + ":00");
        }
      }
      ClickEncoder::Button m = encoder->getButton();
      if (m != ClickEncoder::Open) {
        nowRunning = false;
      }
      Alarm.delay(50);
    }
  }
  recalculateEverything();
  //  Serial.println(calculatedTimeOff);

  lcd.noBlink();
}

void startHour() {
  // This Method is responsible for changing START Time
  int index;

  printLcd(1, 0, 0, "Ust. pocz. dnia");
  printLcd(1, 0, 1, String(poczatekDnia[0]) + ":0" + String(poczatekDnia[1]) + ":00");

  lcd.blink();
  for (index = 0; index < ((sizeof(poczatekDnia) / sizeof(poczatekDnia[0]))); index = index + 1) {
    nowRunning = true;
    while (nowRunning) {
      lastPosition = last;
      if (checkRotation()) {
        poczatekDnia[index] = setTimeValue(poczatekDnia[index], lastPosition, index);
        if (index >= 1 && poczatekDnia[index] <= 9) {
          printLcd(1, 0, 1, String(poczatekDnia[0]) + ":0" + String(poczatekDnia[1]) + ":00");
        } else {
          printLcd(1, 0, 1, String(poczatekDnia[0]) + ":" + String(poczatekDnia[1]) + ":00");
        }
      }
      ClickEncoder::Button m = encoder->getButton();
      if (m != ClickEncoder::Open) {
        nowRunning = false;
      }
      Alarm.delay(50);
    }
  }
  recalculateEverything();
  //  Serial.println(calculatedTimeOn);
  lcd.noBlink();
}



long calculateTime(long hours, long minutes, long seconds) {
  return ((hours * 3600) + (minutes * 60) + seconds);
}


//int maxPossiblePwmIndex(int maxPossiblePwm, boolean dzien) {
//  // will show maximum possible index
//  if (!dzien) {
//    for (int index = 0; index <= ((sizeof(mocNoc) / sizeof(mocNoc[0]))); index = index + 1) {
//      if (maxPossiblePwm == mocNoc[index]) {
//        return index;
//      }
//    }
//  } else {
//    for (int index = 0; index < ((sizeof(mocDzien) / sizeof(mocDzien[0]))); index = index + 1) {
//      if (maxPossiblePwm == mocDzien[index]) {
//        return index;
//      }
//    }
//  }
//}

//int current

int switchControlDay() {
  //Method will takeover calculation neccesery for PWM controll (each chanell individually)
  if (manualMode || !pwmControllIsStarted) {
    return 0;
  }

  long currentCalculatedTime = calculateTime(hour(), minute(), second());

  if (currentCalculatedTime >= calculatedTimeOn && currentCalculatedTime < (calculatedTimeOff)) {  //kondycja dla włączonego
    currentCalculatedTime = calculateTime(hour(), minute(), second());
    //    Serial.println("in loop");
    if (currentCalculatedTime < (calculatedTimeOn + calculatedPwmIncreaseDecrease)) { // konducja zwiekszająca - świt
      if (!isIncreaseRunning) {
        //        Serial.println("***");
        //        Serial.println(increaseTimerDay);
        //        Serial.println(calculatedTimerIncreaseDecrease);
        Alarm.enable(increaseTimerDay);
        isIncreaseRunning = true;
        Serial.println("stage 1 - ON");

      }
      Alarm.delay(0);
      // counter++ every X seconds from 0% till 60%
    } else if (currentCalculatedTime >= (calculatedTimeOn + calculatedPwmIncreaseDecrease) && currentCalculatedTime < (calculatedTimeOn + calculatedPwmIncreaseDecrease + calculatedNormalTime)) {
      if (!isNormalUpRunning) {
        Alarm.disable(increaseTimerDay);
        Alarm.enable(normalTimerUp);
        isNormalUpRunning = true;
        isIncreaseRunning = false;
        Serial.println("stage 2 - ON");

      }
    } else if (currentCalculatedTime < (calculatedTimeOff - calculatedPwmIncreaseDecrease - calculatedNormalTime) && currentCalculatedTime >=  (calculatedTimeOn + calculatedPwmIncreaseDecrease + calculatedNormalTime)) {
      Serial.println("MAX PWM time hit");
      if (pwmDzien == maxPwmDzien) {
        Alarm.disable(normalTimerUp);
        Serial.println("stage 3 - ON");

      }
      //counter++ every X seconds from 60% till max (possible - set in menu)

    } else if (currentCalculatedTime >= (calculatedTimeOff - calculatedPwmIncreaseDecrease - calculatedNormalTime) && currentCalculatedTime < (calculatedTimeOff - calculatedPwmIncreaseDecrease)) {
      if (!isNormalDownRunning) {
        Alarm.enable(normalTimerDown);
        isNormalDownRunning = true;
        isNormalUpRunning = false;
        Serial.println("stage 4 - ON");

      }
      // Will stop increaseing PWM if it is 100% (of possible)

      //counter-- every x seconds from max possible (set in menu) to 60%
    } else if (currentCalculatedTime >= (calculatedTimeOff - calculatedPwmIncreaseDecrease) && currentCalculatedTime < calculatedTimeOff) { //zmierzch
      //counter-- every X seconds from 60% till 0%
      if (!isDecreasingRunning) {
        Alarm.disable(normalTimerDown);
        Alarm.enable(decreaseTimerDay);
        isDecreasingRunning = true;
        isNormalDownRunning = false;
        Serial.println("stage 5 - ON");

      }
    }

  } else if (currentCalculatedTime < calculatedTimeOn || currentCalculatedTime >= calculatedTimeOff) { // kondycja dla wylaczonego
    currentCalculatedTime = calculateTime(hour(), minute(), second());
    isDecreasingRunning = false;
    Alarm.disable(decreaseTimerDay);
    Alarm.disable(normalTimerDown);
    Alarm.disable(normalTimerUp);
    Alarm.disable(increaseTimerDay);


    if (currentCalculatedTime >= calculatedTimeOff && currentCalculatedTime <= (calculatedPwmIncreaseDecrease + calculatedTimeOff)) {
      //counter++ increase night light from 0% to 5%
    } else if (currentCalculatedTime <= (calculatedTimeOn - (2 * calculatedPwmIncreaseDecrease))) {
      //counter-- decrease night light
    }
  }
}

void pwmFastIncreaseDay() {
  if (pwmDzien < maxPwmDzien * 0.6) {
    pwmDzien += 1;
    analogWrite(dzienPin, pwmDzien);
    Serial.print("alarm increase short by 1 set to:");
    Serial.println(pwmDzien);
    Serial.println("stage 1");
  }
}

void pwmNormalIncreaseDay() {
  if (pwmDzien < maxPwmDzien) {
    pwmDzien += 1;
    analogWrite(dzienPin, pwmDzien);
    Serial.print("alarm increase long by 1 set to:");
    Serial.println(pwmDzien);
    Serial.println("stage 2");
  }
}

void pwmNormalDecreaseDay(int pin, int pwm) {
  if (pwmDzien >= maxPwmDzien * 0.6 && pwmDzien >= 1) {
    pwmDzien -= 1;
    analogWrite(dzienPin, pwmDzien);
    Serial.print("alarm decrease long by 1 set to:");
    Serial.println(pwmDzien);
    Serial.println("stage 4");
  }
}

void pwmFastDecreaseDay(int pin, int pwm) {
  if (pwmDzien > 0) {
    pwmDzien -= 1;
    analogWrite(dzienPin, pwmDzien);
    Serial.print("alarm decrease short by 1 set to:");
    Serial.println(pwmDzien);
    Serial.println("stage 5");
  }
}

void printLcdMode() {
  long thisTimeNow = (long(hour()) * 3600) + (long(minute()) * 60);
  if (thisTimeNow >= calculatedTimeOn && thisTimeNow < calculatedTimeOff) {
    printLcd(0, 10, 0, "Dzien");
  } else {
    printLcd(0, 13, 0, "  ");
    printLcd(0, 10, 0, "Noc");
  }
}

//------------------------------------------- MAIN LOOP -------------------------------------------------------------//
void loop() {
  Alarm.delay(0);
  if (!RTC.isrunning()) {
    RTC.adjust(DateTime(__DATE__, __TIME__)); //Dostosowanie do czasu zbudowania jesli RTC nie dziala
  }
  //  DateTime timeNow = RTC.now();

  if (value >= (sizeof(menu) / sizeof(menu[0]))) {
    value = (sizeof(menu) / sizeof(menu[0])) - 1;
  } else if (value < 0) {
    value = 0;
  }

  if (inMenu) {
    if (checkRotation()) {
      printLcd(1, 0, 1, menu[value]);
    }
  } else {
    value = 0;
    printLcdtime();
    //    printLcdDate(now);
    printLcdPwmValues(true);
    printLcdMode();
  }

  switchControlDay();

  ClickEncoder::Button b = encoder->getButton();
  if (inMenu && b != ClickEncoder::Open) {

    switch (b) {
      case ClickEncoder::Clicked:
        switch (value) {

          case (0):
            tempValue = value;
            setDateTime();
            printLcd(1, 0, 0, "Menu");
            value = tempValue;
            printLcd(1, 0, 1, menu[value]);
            break;

          case (1): // Max PWM noc [%]
            tempValue = value;
            printLcd(1, 0, 0, "Max. PWM noc");
            maxPwmNoc = printPwm(nocPin, maxPwmNoc);
            recalculateEverything();
            printLcd(1, 0, 0, "Menu");
            value = tempValue;
            printLcd(1, 0, 1, menu[value]);
            break;

          case (2): // MAX PWM dzien [%]
            tempValue = value;
            printLcd(1, 0, 0, "Max. PWM dzien");
            maxPwmDzien = printPwm(dzienPin, maxPwmDzien);
            recalculateEverything();
            printLcd(1, 0, 0, "Menu");
            value = tempValue;
            printLcd(1, 0, 1, menu[value]);
            break;

          case (3): // Poczatek dnia
            tempValue = value;
            printLcd(1, 0, 0, "Ust. pocz. dnia");
            startHour();
            printLcd(1, 0, 0, "Menu");
            value = tempValue;
            printLcd(1, 0, 1, menu[value]);
            break;

          case (4)://Koniec Dnia
            tempValue = value;
            printLcd(1, 0, 0, "Ust. koniec dnia");
            endHour();
            printLcd(1, 0, 0, "Menu");
            value = tempValue;
            printLcd(1, 0, 1, menu[value]);
            break;

          case (5): //  Tryb reczny
            tempValue = value;
            manualMode = setmanualMode(manualMode);
            printLcd(1, 0, 0, "Menu");
            value = tempValue;
            printLcd(1, 0, 1, menu[value]);
            break;

          case (6): // Ustawienia trybu manualnego
            tempValue = value;
            value = 0;
            manualSettingsMenu();
            printLcd(1, 0, 0, "Menu");
            value = tempValue;
            printLcd(1, 0, 1, menu[value]);
            break;


          case (7): //FW version
            tempValue = value;
            //        lcd.clear();
            printLcd(1, 0, 0, ver);
            printLcd(1, 0, 1, __DATE__);
            Alarm.delay(5000);
            value = tempValue;
            printLcd(1, 0, 0, "Menu");
            printLcd(1, 0, 1, menu[value]);
            break;

          case (8):
            //            Serial.println("8");
            break;
        }
        break;
    }
  }




  if (b != ClickEncoder::Open) {
    //#define VERBOSECASE(label) case label: Serial.println(#label); break;

    switch (b) {
      //        VERBOSECASE(ClickEncoder::Pressed);
      case ClickEncoder::Held:
        Serial.println("-----Menu------");
        inMenu = !inMenu;
        if (inMenu) {
          value = 0;
          lcd.clear();
          lcd.print("Menu");
          printLcd(1, 0, 1, menu[value]);
          Alarm.delay(500);
        } else if (!inMenu) {
          lcd.clear();
          printLcdtime();
          printLcdPwmValues(true);
          Alarm.delay(500);
        }
        break;

      //        VERBOSECASE(ClickEncoder::Released)
      case ClickEncoder::DoubleClicked:
        Serial.println("ClickEncoder::DoubleClicked");
//        encoder->setAccelerationEnabled(!encoder->getAccelerationEnabled());
        //        Serial.print("  Acceleration is ");
        //        Serial.println((encoder->getAccelerationEnabled()) ? "enabled" : "disabled");

        //#ifdef WITH_LCD
        //        displayAccelerationStatus();
        //#endif
        break;
    }
  }

}
