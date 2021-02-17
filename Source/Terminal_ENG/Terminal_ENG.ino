#include <Wire.h>
#include <RTClib.h>     //https://github.com/adafruit/RTClib
RTC_DS3231 Clock;
DateTime dateTimeValue;

#include <Ethernet.h>   //https://www.arduino.cc/en/Reference/Ethernet
byte MAC[] = {0xA2, 0xAA, 0xAA, 0x3F, 0x7D, 0x09};
boolean keyStatus = false;

#include <EEPROM.h>

#include <Adafruit_ST7735.h>    //https://github.com/adafruit/Adafruit-ST7735-Library
#define CS   7
#define DC   9
#define RESET  8
Adafruit_ST7735 Screen = Adafruit_ST7735(CS, DC, RESET);

#include <qrcode.h>   //https://github.com/ricmoo/QRCode
#define codeCor 2
QRCode codeMatrix;
uint8_t codeMatrixBytes[211];

#define button0 2
#define button1 3
#define int0    2
#define int1    3
byte buttonStatus;

boolean displayMode = true;
boolean changeMode = true;

String loginString;
String passwordString;
String APstring;
String SSIDstring;
String WPAkey;
String PIN;
byte rType;
byte alarmS;
byte alarmH;
byte alarmM;
byte alarmOffs;
boolean DHCPStat;
byte clientIP[4];
byte serverIP[4];

volatile unsigned long lastRenew = millis();

#define loginStringA 0
#define loginStringL 31

#define passwordStringA 32
#define passwordStringL 63

#define APstringA 96
#define APstringL 31

#define SSIDstringA 128
#define SSIDstringL 31

#define WPAkeyA 160
#define WPAkeyL 21

#define PINA 247
#define PINL 8
#define PINM 4
bool pinEdit = true;

#define alarmOffsA 242
#define alarmSA 243
#define alarmHA 244
#define alarmMA 245
#define rTypeA 246

#define DHCPStatA 241
#define clientIPA 237
#define serverIPA 233


EthernetClient client;
String telnetString = "";

void setup() {
  randomSeed(analogRead(A0));

  rType = eeprom_read_byte(rTypeA) % 2;
  alarmS = eeprom_read_byte(alarmSA);
  alarmH = eeprom_read_byte(alarmHA);
  alarmM = eeprom_read_byte(alarmMA);
  alarmOffs = eeprom_read_byte(alarmOffsA);

  DHCPStat = eeprom_read_byte(DHCPStatA);
  for (byte count = 4; count; count--) {
    clientIP[count - 1] = eeprom_read_byte(clientIPA + count - 1);
    serverIP[count - 1] = eeprom_read_byte(serverIPA + count - 1);
  }

  loginString = eeStringRead(loginStringA, loginStringL);
  passwordString = eeStringRead(passwordStringA, passwordStringL);
  APstring = eeStringRead(APstringA, APstringL);
  SSIDstring = eeStringRead(SSIDstringA, SSIDstringL);
  WPAkey = eeStringRead(WPAkeyA, WPAkeyL);
  PIN = eeStringRead(PINA, PINL);

  pinMode(button0, INPUT_PULLUP); attachInterrupt(int0, interrupt0, FALLING);
  pinMode(button1, INPUT_PULLUP); attachInterrupt(int1, interrupt1, FALLING);
  releaseStatus();

  Screen.initR(INITR_144GREENTAB); Screen.setTextWrap(true); Screen.fillScreen(ST77XX_BLACK);

  if (!Clock.begin()) {
    Screen.setTextColor(ST7735_RED);
    Screen.setCursor(0, 0);
    Screen.print("RTC error!");
    for (;;);
  }

  if (!digitalRead(button0))if (!digitalRead(button1)) setupMode();

  tryRenewKey();
}

void loop() {
  if (checkStatus(4) or Clock.alarmFired(1)) {
    tryRenewRandomizeKey();
    setRTCAlarm();
  }

  if (checkStatus(0) or checkStatus(1)) {
    displayMode = !displayMode;
    changeMode = true;
  }

  if (changeMode) {
    Screen.fillScreen(ST77XX_BLACK);
    changeMode = false;
    if (displayMode) drawQRCode(); else showStatus();
  }

  delay(10);
}

void showStatus() {
#define tempPer 20000
  volatile unsigned long capTemp = (unsigned long)(millis() / tempPer) * tempPer + tempPer;
  volatile float currTemp = Clock.getTemperature();
  releaseStatus();
  Screen.setTextColor(ST7735_YELLOW); Screen.setCursor(0, 0);  Screen.print("SSID:");
  Screen.setTextColor(ST7735_CYAN); Screen.setCursor(0, 12); Screen.print(SSIDstring);
  Screen.setTextColor(ST7735_YELLOW); Screen.setCursor(0, 36); Screen.print("WPA key:");
  Screen.setTextColor(ST7735_CYAN); Screen.setCursor(0, 48); Screen.print(WPAkey);
  if (!keyStatus) {
    Screen.setCursor(0, 72);
    Screen.setTextColor(ST77XX_RED);
    Screen.println("WPA key\n     update failed :(");
  }
  do {
    if (capTemp < millis()) {
      capTemp = (unsigned long)(millis() / tempPer) * tempPer + tempPer;
      currTemp = Clock.getTemperature();
    }
    dateTimeValue = Clock.now();
    Screen.setCursor(0, 120);
    Screen.setTextColor(ST7735_YELLOW, ST7735_BLACK);
    Screen.print((String)dateTimeValue.timestamp(1) + "    ");
    if (currTemp > -100 and currTemp < 100) Screen.print(" ");
    if (currTemp > -10 and currTemp < 10) Screen.print(" ");
    if (currTemp >= 0) Screen.print(" ");
    Screen.print((String)currTemp + " C");

    delay(100);
    buttonStatus = buttonStatus & 0b11110011;
  } while ((!buttonStatus) and !Clock.alarmFired(1));
  changeMode = true;
}

void randomizeKey() {
  WPAkey = "";
  for (byte count = WPAkeyL; count; count--) {
    char C = random(61);
    if (C < 10) C += 48; else if (C < 36) C += 55; else C += 61;
    WPAkey.concat(C);
  }
  eeStringWrite(WPAkey, WPAkeyA, WPAkeyL);
}

void tryRenewRandomizeKey() {
  randomizeKey();
  tryRenewKey();
}

void tryRenewKey() {
  Screen.fillScreen(ST77XX_BLACK); Screen.setTextColor(ST7735_YELLOW); Screen.setCursor(0, 0);
  Screen.print("Updating\n       the WPA key...");

  keyStatus = false; changeMode = true;
  for (byte count = 3; count; count--) {
    if (sendKey()) {
      keyStatus = displayMode = true;
      break;
    }
  }
}

boolean sendKey() {
  newQRcode();
  if (Ethernet.linkStatus() != LinkON) return false;
  if (DHCPStat) Ethernet.begin(MAC, clientIP); else if (!Ethernet.begin(MAC)) return false;

  if (!DHCPStat) if (!client.connect(Ethernet.gatewayIP(), 23)) return false;
  if (DHCPStat) if (!client.connect(serverIP, 23)) return false;

  if (!sendReceive("")) return false;

  if (!sendReceive(loginString + enterCode())) return false;

  if (!sendReceive(passwordString + enterCode())) return false;

  delay(5000);
  if (!sendReceive("")) return false;

  switch (rType) {
    case 0:
      if (!sendReceive("interface " + APstring + " authentication wpa-psk " + WPAkey)) return false;
      break;
    case 1:
      if (!sendReceive("/interface wireless security-profiles set " + APstring + " wpa2-pre-shared-key=" + WPAkey + enterCode())) return false;
      if (!sendReceive(":put [/interface wireless security-profiles get " + APstring + " wpa2-pre-shared-key]")) return false;
  }

  telnetString = "";
  if (!sendReceive(enterCode())) return false;
  String contrStr = telnetString;
  if (!rType) if (!sendReceive("system configuration save" + enterCode())) return false;

  client.stop();

  switch (rType) {
    case 0:
      if (contrStr.indexOf("key saved") + 1) return true; else return false;
    case 1:
      if (contrStr.indexOf((String)WPAkey) + 1) return true; else return false;
  }
}

//Отправим ENTER
String enterCode() {
  switch (rType) {
    case 0: return "\n";
    case 1: return "\x0D";
  }
}


//ЧТЕНИЕ/ЗАПИСЬ ПО СЕТИ
bool sendReceive(String str) {
  if (Ethernet.linkStatus() == LinkON) {
    if (str.length()) client.print(str);
  } else return false;

  volatile unsigned long startCount = millis();
  while (millis() - startCount < 3000) {
    if ((Ethernet.linkStatus() == LinkON)) {
      while (client.available()) {
        byte C = client.read();

        if (C == 255) {
          byte verb, opt;
          byte outBuf[3] = {255, 0, 0};
          verb = client.read();
          //if (verb == 255) return;
          switch (verb) {
            case 251:
              opt = client.read();
              if (opt == -1) break;
              outBuf[1] = 253;
              outBuf[2] = opt;
              client.write(outBuf, 3);
              client.flush();
              break;
            case 253:
              opt = client.read();
              if (opt == -1) break;
              outBuf[1] = 252;
              outBuf[2] = opt;
              client.write(outBuf, 3);
              client.flush();
          }
        } else {
          telnetString.concat((char)C);
          if (telnetString.length() > 255) telnetString.remove(0, 1);  //Буфер увеличен для теста
        }

        startCount = millis();
      }
    } else return false;
    delay(1);
  }
  return true;
}

void newQRcode()
{
  String QRString = String("WIFI:S:" + SSIDstring + ";T:WPA;P:" + WPAkey + ";;");
  char QRCharArray[107];

  QRString.toCharArray(QRCharArray, QRString.length());

  qrcode_initText(&codeMatrix, codeMatrixBytes, 6, codeCor, QRCharArray);
}

//ОТРИСОВКА QR-КОДА
void drawQRCode() {
  Screen.fillScreen(keyStatus ? ST77XX_WHITE : ST77XX_RED);
  for (uint8_t y = 0; y < codeMatrix.size; y++) {
    for (uint8_t x = 0; x < codeMatrix.size; x++) {
      if (qrcode_getModule(&codeMatrix, x, y))Screen.fillRect((2 + x * 3), (2 + y * 3), 3, 3, ST77XX_BLACK);
    }
  }
}


void interrupt0() {
  buttonHelper(0);
}
void interrupt1() {
  buttonHelper(1);
}
void buttonHelper(byte flag) {
  for (byte count = 0; count < 200; count++) {
    for (volatile unsigned int x = 4680; x; x--);     //Thinary
    if (flag) {
      if (digitalRead(button1)) if (count < 20)return;
        else {
          bitSet(buttonStatus, 1);
          return;
        }
    }
    else {
      if (digitalRead(button0)) if (count < 20)return;
        else {
          bitSet(buttonStatus, 0);
          return;
        }
    }
  }
  if (!digitalRead(button0) and !digitalRead(button1)) bitSet(buttonStatus, 4); else bitSet(buttonStatus, (flag + 2));
}

boolean checkStatus(byte statusB) {
  if (bitRead(buttonStatus, statusB)) {
    bitClear(buttonStatus, statusB);
    return true;
  } return false;
}
byte releaseStatus() {
  byte backup = buttonStatus;
  buttonStatus = buttonStatus & 0;
  return backup;
}

void drawSetBox(String &boxstring) {
  Screen.fillScreen(ST77XX_BLACK);
  Screen.setTextColor(ST77XX_WHITE);
  Screen.setCursor(0, 0);
  Screen.print("Back\n++\n\n ");
  Screen.print(boxstring);
  Screen.print("\n\n\n\n\n\n\n\n\n\n\n--\nEnter");
}


void setupMode() {
  String setStr00 = "Settings";
  String setStr0 = "Type of router";
  String setStr1 = "Login";
  String setStr2 = "Password";
  String setStr3 = "SSID";
  String setStr4 = "Point/Profile";
  String setStr5 = "IP-address";
  String setStr6 = "Time";
  String setStr7 = "Update";
  String setStr8 = "PIN-code";

  releaseStatus();
  {
    String enterablePin = "";
    drawSetBox(setStr8);
    if (stringEdit(enterablePin, PINM, PINL)) return;
    if (!(enterablePin == PIN)) {
      Screen.setCursor(5, 72); Screen.setTextColor(ST77XX_RED);
      Screen.print("Invalid PIN!!!");
      delay(2000);
      return;   //эту строку закоментировать для установки пина
    }
  }
  pinEdit = false;

  for (byte pos = 0, prevpos = -1;;) {
    if (pos != prevpos) {
      drawSetBox(setStr00);
      Screen.setCursor(15, 36); Screen.print(setStr0);
      Screen.setCursor(15, 44); Screen.print(setStr1);
      Screen.setCursor(15, 52); Screen.print(setStr2);
      Screen.setCursor(15, 60); Screen.print(setStr3);
      Screen.setCursor(15, 68); Screen.print(setStr4);
      Screen.setCursor(15, 76); Screen.print(setStr5);
      Screen.setCursor(15, 84); Screen.print(setStr6);
      Screen.setCursor(15, 92); Screen.print(setStr7);
      Screen.setCursor(15, 100); Screen.print(setStr8);

      Screen.setCursor(5, (36 + pos * 8));
      Screen.print((char)0x10); prevpos = pos;
    }

    if (checkStatus(0)) if (pos) pos--;
    if (checkStatus(1)) if (pos < 8) pos++;
    if (checkStatus(2)) return;

    if (checkStatus(3)) {
      prevpos = -1;
      switch (pos) {
        case 0: rTypeSetup(setStr0); break;
        case 1: loginSetup(setStr1); break;
        case 2: passwordSetup(setStr2); break;
        case 3: ssidSetup(setStr3); break;
        case 4: pointSetup(setStr4); break;
        case 5: setupIP(setStr5); break;
        case 6: timeSet(setStr6); break;
        case 7: setAlarm(setStr7); break;
        case 8: pinSet(setStr8);
      }
    }
    //delay(10);
  }
}


void setAlarm(String &setStr) {
  String setStr0 = "Disabled";
  String setStr1 = "Every 3 hours";
  String setStr2 = "Every 6 hours";
  String setStr3 = "Every 12 hours";
  String setStr4 = "Every day";
  String setStr5 = "Update from";
  for (byte pos = alarmS % 4, prevpos = -1;;) {
    if (!(pos == prevpos)) {
      drawSetBox(setStr);
      Screen.setCursor(15, 36); Screen.print(setStr0);
      Screen.setCursor(15, 44); Screen.print(setStr1);
      Screen.setCursor(15, 52); Screen.print(setStr2);
      Screen.setCursor(15, 60); Screen.print(setStr3);
      Screen.setCursor(15, 68); Screen.print(setStr4);
      Screen.setCursor(5, (36 + pos * 8));
      Screen.print((char)0x10); prevpos = pos;
    }
    if (checkStatus(0) and pos) pos--;
    if (checkStatus(1) and pos < 4) pos++;
    if (checkStatus(2)) return;
    if (checkStatus(3)) {
      if (pos) {
        byte maxVal = 3;
        for (byte count = pos - 1; count; count--) maxVal += maxVal;
        maxVal--;

        for (byte val = alarmOffs % (maxVal + 1), redr = -1;;) {
          if (!(val == redr)) {
            drawSetBox(setStr5);
            Screen.setCursor(0, 36); Screen.print((String)val + "\n\n[0-" + (String)maxVal + (String)']');
            redr = val;
          }
          if (checkStatus(0) and val < maxVal) val++;
          if (checkStatus(1) and val) val--;
          if (checkStatus(2)) return;
          if (checkStatus(3)) {
            alarmS = pos; alarmOffs = val;
            eeprom_update_byte(alarmSA, alarmS);
            eeprom_update_byte(alarmOffsA, alarmOffs);
            setRTCAlarm();
            return;
          }
        }
      }
      alarmS = 0; alarmOffs = 0;
      eeprom_update_byte(alarmSA, alarmS);
      eeprom_update_byte(alarmOffsA, alarmOffs);
      setRTCAlarm();
      return;
    }
  }
}


void setRTCAlarm() {
  if (alarmS) {
    byte incVal = 3;
    for (byte count = (alarmS % 5) - 1; count; count--) incVal += incVal;
    dateTimeValue = Clock.now();

    byte setHours = alarmOffs;

    for (;; setHours += incVal) {
      if (setHours > dateTimeValue.hour()) if (setHours < 24) break;
        else {
          setHours -= 24;
          break;
        }
    }

    dateTimeValue = DateTime(0, 0, 0, setHours, 0, 0);
    while (!Clock.setAlarm1(dateTimeValue, DS3231_A1_Hour)) {}

  }
  else Clock.disableAlarm(1);
  Clock.clearAlarm(1);
}


boolean editIP(String setStr, byte addr[4]) {
  byte temp[4];
  *temp = *addr;
  for (byte redraw = 1, pos = 0;;) {
    if (redraw) {
      drawSetBox(setStr);
      Screen.setCursor(0, 40);
      if (addr[0] < 100) Screen.print('0'); if (addr[0] < 10) Screen.print('0'); Screen.print(addr[0]); Screen.print('.');
      if (addr[1] < 100) Screen.print('0'); if (addr[1] < 10) Screen.print('0'); Screen.print(addr[1]); Screen.print('.');
      if (addr[2] < 100) Screen.print('0'); if (addr[2] < 10) Screen.print('0'); Screen.print(addr[2]); Screen.print('.');
      if (addr[3] < 100) Screen.print('0'); if (addr[3] < 10) Screen.print('0'); Screen.print(addr[3]);
      Screen.setCursor(24 * pos, 48); Screen.print("^^^");
      redraw = !redraw;
    }
    if (checkStatus(0)) {
      addr[pos]++;
      if (!addr[pos] and (!pos or !(pos - 3))) addr[pos]++;
      redraw = true;
    }
    if (checkStatus(1)) {
      addr[pos]--;
      if (!addr[pos] and (!pos or !(pos - 3))) addr[pos]--;
      redraw = true;
    }
    if (checkStatus(2)) if (pos) {
        pos--;
        redraw = true;
      } else {
        *addr = *temp;
        return false;
      }
    if (checkStatus(3)) if (pos < 3) {
        pos++;
        redraw = true;
      } else return true;
  }
}

void eeUpdateIP(byte addr, byte data[4]) {
  for (byte count = 4; count; count--) {
    eeprom_update_byte(addr + count - 1, data[count - 1]);
  }
}


void setupIP(String &setStr) {
  String setStr0 = "Dynamic";
  String setStr1 = "Static";
  String setStr2 = "Client IP";
  String setStr3 = "Router IP";

  for (byte pos = DHCPStat % 2, prevpos = -1;;) {
    if (!(pos == prevpos)) {
      drawSetBox(setStr);
      Screen.setCursor(15, 36); Screen.print(setStr0);
      Screen.setCursor(15, 44); Screen.print(setStr1);
      Screen.setCursor(5, (36 + pos * 8));
      Screen.print((char)0x10); prevpos = pos;
    }
    if (checkStatus(0)) if (pos) pos--; if (checkStatus(1)) if (pos < 1) pos++;
    if (checkStatus(2)) return;
    if (checkStatus(3)) {
      if (!pos) {
        DHCPStat = pos;
        eeprom_update_byte(DHCPStatA, DHCPStat);
      }
      if (pos and editIP(setStr2, clientIP) and editIP(setStr3, serverIP)) {
        eeUpdateIP(clientIPA, clientIP); eeUpdateIP(serverIPA, serverIP);
        DHCPStat = pos; eeprom_update_byte(DHCPStatA, DHCPStat);
      }
      return;
    }
  }
}


void rTypeSetup(String &setStr) {
  String setStr0 = "Keenetic";
  String setStr1 = "Mikrotik";
  for (byte pos = rType, prevpos = -1;;) {
    if (!(pos == prevpos)) {
      drawSetBox(setStr);
      Screen.setCursor(15, 36); Screen.print(setStr0);
      Screen.setCursor(15, 44); Screen.print(setStr1);
      Screen.setCursor(5, (36 + pos * 8));
      Screen.print((char)0x10); prevpos = pos;
    }
    if (checkStatus(0)) if (pos) pos--; if (checkStatus(1)) if (pos < 1) pos++;
    if (checkStatus(2)) return;
    if (checkStatus(3)) {
      rType = pos;
      eeprom_update_byte(rTypeA, rType);
      return;
    }
  }
}


void loginSetup(String &setStr) {
  drawSetBox(setStr);
  stringEdit(loginString, 1, loginStringL); eeStringWrite(loginString, loginStringA, loginStringL);
}


void passwordSetup(String &setStr) {
  drawSetBox(setStr);
  stringEdit(passwordString, 1, passwordStringL); eeStringWrite(passwordString, passwordStringA, passwordStringL);
}


void pointSetup(String &setStr) {
  drawSetBox(setStr);
  stringEdit(APstring, 1, APstringL); eeStringWrite(APstring, APstringA, APstringL);
}


void ssidSetup(String &setStr) {
  drawSetBox(setStr);
  stringEdit(SSIDstring, 1, SSIDstringL); eeStringWrite(SSIDstring, SSIDstringA, SSIDstringL);
}


boolean timeEdit(String &setStr) {
  uint8_t hr = dateTimeValue.hour(); uint8_t mn = dateTimeValue.minute();
  for (byte pos = 0, prevpos = -1;;) {
    if (pos != prevpos) {
      drawSetBox(setStr);
      Screen.setCursor(0, 40);
      if (hr < 10) Screen.print('0'); Screen.print(hr); Screen.print(':'); if (mn < 10) Screen.print('0'); Screen.print(mn);
      Screen.setCursor((pos * 18), 48); Screen.print("^^");
      delay(10); prevpos = pos;
    }
    if (checkStatus(0)) {
      if (pos) mn++; else hr++;
      if (mn > 59) mn = 0; if (hr > 23) hr = 0;
      prevpos = -1;
    }
    if (checkStatus(1)) {
        if (pos) if (mn) mn--; else mn = 59; else if (hr) hr--; else hr = 23;
      prevpos = -1;
    }
      if (checkStatus(2)) if (pos) pos--; else return false;
      if (checkStatus(3)) if (!pos) pos++; else {
        dateTimeValue = DateTime(dateTimeValue.year(), dateTimeValue.month(), dateTimeValue.day(), hr, mn, 1);
        return true;
      }
  }
}


void timeSet(String &setStr) {
  dateTimeValue = Clock.now();
  if (timeEdit(setStr)) {
    Clock.adjust(dateTimeValue);
    setRTCAlarm();
    return;
  }
}


void pinSet(String &setStr) {
  pinEdit = true;
  drawSetBox(setStr);
  stringEdit(PIN, PINM, PINL); eeStringWrite(PIN, PINA, PINL);
  pinEdit = false;
}



String eeStringRead(unsigned int startAddr, byte maxLength) {
  String str;
  byte count;
  for (count = 0; count < maxLength; count++) {
    char C = eeprom_read_byte(startAddr + count);
    if (C) str += C; else break;
  }
  return str;
}


void eeStringWrite(String &srcStr, unsigned int startAddr, byte maxLength) {
  byte count;
  for (count = 0; count < srcStr.length(); count++) {
    eeprom_update_byte(startAddr + count, srcStr[count]);
  }
  if (count - maxLength) eeprom_update_byte(startAddr + count, 0x0);
}



bool stringEdit(String &editableString, byte minLength, byte maxLength) {
  String backupString = editableString;
#define releaseTimeout 200
#define minCharCode 33
#define maxCharCode 126
  String avalibleChars {"!_-.0123456789ABCDEFGHIGKLMNOPQRSTUVWXYZabcdefghigklmnopqrstuvwxyz"};
  if (pinEdit) avalibleChars = "0123456789";

  for (boolean timeout = false;;) {
    byte timeCount = releaseTimeout;
    //editableString.remove(126);
    Screen.fillRect(0, 40, 128, 64, ST77XX_BLACK);
    Screen.setCursor(0, 40); Screen.print(editableString + (char)0x16 + "\n\n<" + minLength + '-' + maxLength + ">:" + editableString.length());
    for (;;) {
      byte lengthStr = editableString.length() - 1;
      if (checkStatus(0)) {
        if (!editableString.length()) {
          if (pinEdit) editableString = "0";
          else editableString = "a";
          timeout++;
        }
        else {
          if (timeout) {
            do editableString[lengthStr]++; while ( !(avalibleChars.indexOf(editableString[lengthStr]) + 1) or !editableString[lengthStr]);
          } else timeout++;
        }
        break;
      }
      if (checkStatus(1)) {
        if (!editableString.length()) {
          if (pinEdit) editableString = "0";
          else editableString = "a";
          timeout++;
        }
        else {
          if (timeout) {
            do editableString[lengthStr]--; while ( !(avalibleChars.indexOf(editableString[lengthStr]) + 1) or !editableString[lengthStr]);
          } else timeout++;
        }
        break;
      }
      if (checkStatus(2)) {
        timeout = false; timeCount = releaseTimeout;
        if (!editableString.length()) {
          editableString = backupString;
          return true;
        }
        editableString.remove(lengthStr); break;
      }

      if (checkStatus(3)) {
        if (editableString.length() >= minLength) {
          //editableString = backupString;
          return false;
        }
      }
      delay(10);
      if (timeout) timeCount--;
      if (!timeCount) {
        timeout = 0;
        if (editableString.length() < maxLength) {
          if (pinEdit) editableString.concat('0'); else editableString.concat('a');
        }
        break;
      }
    }
  }
}
