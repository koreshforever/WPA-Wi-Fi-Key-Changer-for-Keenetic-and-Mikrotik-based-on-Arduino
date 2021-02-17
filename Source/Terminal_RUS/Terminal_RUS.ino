#include <Wire.h>
#include <RTClib.h>     //https://github.com/adafruit/RTClib
RTC_DS3231 Clock;
DateTime dateTimeValue;

//Подключаем локалку
#include <Ethernet.h>   //https://www.arduino.cc/en/Reference/Ethernet
byte MAC[] = {0xA2, 0xAA, 0xAA, 0x3F, 0x7D, 0x09};
boolean keyStatus = false;

//Юзаем EEPROM в ардуине
#include <EEPROM.h>

//Подключаем дисплей
#include <Adafruit_ST7735.h>    //https://github.com/adafruit/Adafruit-ST7735-Library
#include <Adafruit_ST7789.h>
#include <Adafruit_ST77xx.h>
#define CS   7    //пин SS для дисплея, изменил, ибо конфликтует с LAN
#define DC   9
#define RESET  8
Adafruit_ST7735 Screen = Adafruit_ST7735(CS, DC, RESET);

//Юзаем QR-код
#include <qrcode.h>   //https://github.com/ricmoo/QRCode
#define codeCor 2
QRCode codeMatrix;
uint8_t codeMatrixBytes[211];  // размер массива рассчитанный заранее функцией qrcode_getBufferSize(codeVersion) для кода версии 6

//Конастанты прерываний
#define button0 2   //кнопка обязательно на INT0
#define button1 3   //кнопка обязательно на INT1
#define int0    2   //Номер INT0 прерывания (абстракция для Thinary);
#define int1    3   //Номер INT1 прерывания (абстракция для Thinary);
byte buttonStatus;

//ВЫДЕЛИМ ПАМЯТЬ
boolean displayMode = true;
boolean changeMode = true;

String loginString;
String passwordString;
String APstring;
String SSIDstring;
String WPAkey;
String PIN;
byte rType;
byte alarmS;   //СТАТУС БУДИЛЬНИКА
byte alarmH;
byte alarmM;
byte alarmOffs;  //СМЕЩЕНИЕ ПО ВРЕМЕНИ
boolean DHCPStat;
byte clientIP[4];
byte serverIP[4];

volatile unsigned long lastRenew = millis();

//КАК ВСЯ ТРЕБУХА БУДЕТ РАСПОЛАГАТЬСЯ В EEPROM
#define loginStringA 0
#define loginStringL 31

#define passwordStringA 32
#define passwordStringL 63

#define APstringA 96
#define APstringL 31

#define SSIDstringA 128
#define SSIDstringL 31

#define WPAkeyA 160
#define WPAkeyL 21    //МАКСИМАЛЬНАЯ ДЛИННА WPA КЛЮЧА (ДЛЯ СОЗДАВАЕМОЙ МАТРИЦЫ МАКСИМУМ 25 С УЧЁТОМ ОСТАЛЬНЫХ ПАРАМЕТРОВ)

#define PINA 247
#define PINL 8    //ДЛИННА ПИНА
#define PINM 4    //МИНИМАЛЬНАЯ ДЛИННА
bool pinEdit = true;

//Требуха байтовая
#define alarmOffsA 242
#define alarmSA 243
#define alarmHA 244
#define alarmMA 245
#define rTypeA 246

#define DHCPStatA 241      //ХРАНЕНИЕ СТАТУСА DHCP
#define clientIPA 237    //ХРАНЕНИЕ НАЧАЛЬНОЙ ПОЗИЦИИ АДРЕСА КЛИЕНТА (4 байта)
#define serverIPA 233    //ХРАНЕНИЕ НАЧАЛЬНОЙ ПОЗИЦИИ АДРЕСА СЕРВЕРА (4 байта)


EthernetClient client;
String telnetString = "";

void setup() {
  //ЗАПУСТИМ РАНДОМАЙЗЕР
  randomSeed(analogRead(A0));

  //Загрузим настройки
  rType = eeprom_read_byte(rTypeA) % 2;
  alarmS = eeprom_read_byte(alarmSA);  //статус будильника
  alarmH = eeprom_read_byte(alarmHA);  //часы пробуждения
  alarmM = eeprom_read_byte(alarmMA);  //минуты пробуждения
  alarmOffs = eeprom_read_byte(alarmOffsA); //СМЕЩЕНИЕ ПО ВРЕМЕНИ

  DHCPStat = eeprom_read_byte(DHCPStatA); //Статус DHCP
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

  //ИНИЦИАЛИЗИРУЕМ ДИСПЛЕЙ
  Screen.initR(INITR_144GREENTAB); Screen.setTextWrap(true); Screen.fillScreen(ST77XX_BLACK);

  //ЗАПУСТИМ ЧАСЫ
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

//Окно статуса
void showStatus() {
#define tempPer 20000 //Период обновления температуры
  volatile unsigned long capTemp = (unsigned long)(millis() / tempPer) * tempPer + tempPer;
  volatile float currTemp = Clock.getTemperature();
  releaseStatus();
  Screen.setTextColor(ST7735_YELLOW); Screen.setCursor(0, 0);  Screen.print("SSID:");
  Screen.setTextColor(ST7735_CYAN); Screen.setCursor(0, 12); Screen.print(SSIDstring);
  Screen.setTextColor(ST7735_YELLOW); Screen.setCursor(0, 36); Screen.print("\x57\x50\x41\x20\xBA\xBB\x8E\x87\x3A");  //WPA ключ:
  Screen.setTextColor(ST7735_CYAN); Screen.setCursor(0, 48); Screen.print(WPAkey);
  if (!keyStatus) {
    Screen.setCursor(0, 72);
    Screen.setTextColor(ST77XX_RED);
    Screen.println("\x57\x50\x41\x20\xBA\xBB\x8E\x87\x20\xBE\xB1\xBD\xBE\xB2\xB8\x82\x8C\xA\x20\x20\x20\x20\x20\x20\x20\x20\xBD\xB5\x20\x83\xB4\xB0\xBB\xBE\x81\x8C\x20\x3A\x28");  //WPA ключ обновить\n        не удалось :(
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
  } while ((!buttonStatus) and !Clock.alarmFired(1));  //НУЖНО ПОДУМАТЬ
  changeMode = true;
}

//ПРОЦЕДУРА ГЕНЕРАЦИИ НОВОГО ПАРОЛЯ
void randomizeKey() {
  WPAkey = "";
  for (byte count = WPAkeyL; count; count--) {
    char C = random(61);
    if (C < 10) C += 48; else if (C < 36) C += 55; else C += 61;
    WPAkey.concat(C);
  }
  eeStringWrite(WPAkey, WPAkeyA, WPAkeyL);
}

//ПРОЦЕДУРА ОБНОВЛЕНИЯ КЛЮЧА
void tryRenewRandomizeKey() {
  randomizeKey();
  tryRenewKey();
}

//ПРОЦЕДУРА ЗАПИХИВАНИЯ КЛЮЧА В РОУТЕР
void tryRenewKey() {
  Screen.fillScreen(ST77XX_BLACK); Screen.setTextColor(ST7735_YELLOW); Screen.setCursor(0, 0);
  Screen.print("\x9E\xB1\xBD\xBE\xB2\xBB\xB5\xBD\xB8\xB5\xA\x20\x20\x20\x20\x20\x20\x20\x20\x20\x57\x50\x41\x20\xBA\xBB\x8E\x87\xB0\x2E\x2E\x2E");   //Обновление\n         WPA ключа...

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

//СОЗДАНИЕ НОВОЙ МАТРИЦЫ QR-КОДА
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



//ОБРАБОТЧИКИ ПРЕРЫВАНИЙ
void interrupt0() {
  buttonHelper(0);
}
void interrupt1() {
  buttonHelper(1);
}
void buttonHelper(byte flag) {
  for (byte count = 0; count < 200; count++) {
    for (volatile unsigned int x = 4680; x; x--);     //абстракция для задержки, ибо delay() в Thinary не работает
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

//ЧТЕНИЕ ФЛАГОВ КНОПОК УСТАНАВЛИВАЕМЫХ ПРЕРЫВАНИЯМИ
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

//ОКНО НАСТРОЕК/РЕДАКТОРА
void drawSetBox(String &boxstring) {
  Screen.fillScreen(ST77XX_BLACK);
  Screen.setTextColor(ST77XX_WHITE);
  Screen.setCursor(0, 0);
  Screen.print("\x9D\xB0\xB7\xB0\xB4\n++\n\n ");
  Screen.print(boxstring);
  Screen.print("\n\n\n\n\n\n\n\n\n\n\n--\n\x92\xB2\xBE\xB4");
}
//РЕЖИМ НАСТРОЙКИ
void setupMode() {
  String setStr00 = "\x9D\xB0\x81\x82\x80\xBE\xB9\xBA\xB8";                 //Настройки
  String setStr0 = "\xA2\xB8\xBF\x20\x80\xBE\x83\x82\xB5\x80\xB0";          //Тип роутера
  String setStr1 = "\x9B\xBE\xB3\xB8\xBD";                                  //Логин
  String setStr2 = "\x9F\xB0\x80\xBE\xBB\x8C";                              //Пароль
  String setStr3 = "SSID";
  String setStr4 = "\xA2\xBE\x87\xBA\xB0\x2F\xBF\x80\xBE\x84\xB8\xBB\x8C";  //Точка/профиль
  String setStr5 = "\x49\x50\x2D\xB0\xB4\x80\xB5\x81";                      //IP-адрес
  String setStr6 = "\x92\x80\xB5\xBC\x8F";                                  //Время
  String setStr7 = "\x9E\xB1\xBD\xBE\xB2\xBB\xB5\xBD\xB8\xB5";              //Обновление
  String setStr8 = "\x50\x49\x4E\x2D\xBA\xBE\xB4";                          //PIN-код

  releaseStatus();
  //ВВОД PIN
  {
    String enterablePin = "";
    drawSetBox(setStr8);
    if (stringEdit(enterablePin, PINM, PINL)) return;
    if (!(enterablePin == PIN)) {
      Screen.setCursor(5, 72); Screen.setTextColor(ST77XX_RED);
      Screen.print("\x9D\xB5\xB2\xB5\x80\xBD\x8B\xB9\x20\x50\x49\x4E\x21\x21\x21"); //Неверный PIN!!!
      delay(2000);
      return;   //эту строку закоментировать для установки пина
    }
  }
  pinEdit = false;

  //ОКНО НАСТРОЕК
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

//УСТАНОВКА БУДИЛЬНИКА
void setAlarm(String &setStr) {
  String setStr0 = "\x9E\x82\xBA\xBB\x8E\x87\xB5\xBD\xBE";                          //Отключено
  String setStr1 = "\x9A\xB0\xB6\xB4\x8B\xB5\x20\x33\x20\x87\xB0\x81\xB0";          //Каждые 3 часа
  String setStr2 = "\x9A\xB0\xB6\xB4\x8B\xB5\x20\x36\x20\x87\xB0\x81\xBE\xB2";      //Каждые 6 часов
  String setStr3 = "\x9A\xB0\xB6\xB4\x8B\xB5\x20\x31\x32\x20\x87\xB0\x81\xBE\xB2";  //Каждые 12 часов
  String setStr4 = "\x9A\xB0\xB6\xB4\x8B\xB9\x20\xB4\xB5\xBD\x8C";                  //Каждый день
  String setStr5 = "\x9E\xB1\xBD\xBE\xB2\xBB\x8F\x82\x8C\x20\x81\x3A";              //Обновлять с:
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

//УСТАНОВКА БУДИЛЬНИКА В RTC
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

//РЕДАКТОР IP-адреса
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

//НАСТРОЙКА IP-адреса
void setupIP(String &setStr) {
  String setStr0 = "\x94\xB8\xBD\xB0\xBC\xB8\x87\xB5\x81\xBA\xB8\xB9";    //Динамический
  String setStr1 = "\xA1\x82\xB0\x82\xB8\x87\xB5\x81\xBA\xB8\xB9";        //Статический
  String setStr2 = "\x49\x50\x20\xBA\xBB\xB8\xB5\xBD\x82\xB0";            //IP клиента
  String setStr3 = "\x49\x50\x20\x80\xBE\x83\x82\xB5\x80\xB0";            //IP роутера

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

//ВЫБОР ТИПА РОУТЕРА
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

//УСТАНОВКА ЛОГИНА
void loginSetup(String &setStr) {
  drawSetBox(setStr);
  stringEdit(loginString, 1, loginStringL); eeStringWrite(loginString, loginStringA, loginStringL);
}

//УСТАНОВКА ПАРОЛЯ
void passwordSetup(String &setStr) {
  drawSetBox(setStr);
  stringEdit(passwordString, 1, passwordStringL); eeStringWrite(passwordString, passwordStringA, passwordStringL);
}

//УСТАНОВКА ИМЕНИ ТОЧКИ ИЛИ ПРОФИЛЯ
void pointSetup(String &setStr) {
  drawSetBox(setStr);
  stringEdit(APstring, 1, APstringL); eeStringWrite(APstring, APstringA, APstringL);
}

//УСТАНОВКА SSID
void ssidSetup(String &setStr) {
  drawSetBox(setStr);
  stringEdit(SSIDstring, 1, SSIDstringL); eeStringWrite(SSIDstring, SSIDstringA, SSIDstringL);
}

//Редактор времени
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

//УСТАНОВКА ВРЕМЕНИ
void timeSet(String &setStr) {
  dateTimeValue = Clock.now();
  if (timeEdit(setStr)) {
    Clock.adjust(dateTimeValue);
    setRTCAlarm();
    return;
  }
}

//УСТАНОВКА ПИН-КОДА
void pinSet(String &setStr) {
  pinEdit = true;
  drawSetBox(setStr);
  stringEdit(PIN, PINM, PINL); eeStringWrite(PIN, PINA, PINL);
  pinEdit = false;
}

//ЧТЕНИЕ И ЗАПИСЬ В EEPROM
//ЧТЕНИЕ СТРОКИ => String eeStringRead(unsigned int, byte)
String eeStringRead(unsigned int startAddr, byte maxLength) {
  String str;
  byte count;
  for (count = 0; count < maxLength; count++) {
    char C = eeprom_read_byte(startAddr + count);
    if (C) str += C; else break;
  }
  return str;
}

//ЗАПИСЬ eeStringWrite(String, unsigned int)
void eeStringWrite(String &srcStr, unsigned int startAddr, byte maxLength) {
  byte count;
  for (count = 0; count < srcStr.length(); count++) {
    eeprom_update_byte(startAddr + count, srcStr[count]);
  }
  if (count - maxLength) eeprom_update_byte(startAddr + count, 0x0);
}

//ПРОСТЕЙШИЙ СТРОКОВЫЙ РЕДАКТОР
//ПЕРЕМЕННАЯ pinEdit определяет режим редактирования PIN-кода
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
