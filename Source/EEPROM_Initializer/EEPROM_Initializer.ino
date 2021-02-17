//Юзаем EEPROM в ардуине
#include <EEPROM.h>

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

//ЗАПИСЬ eeStringWrite(String , unsigned int, byte)
void eeStringWrite(String srcStr, unsigned int startAddr, byte maxLength) {
  byte count;
  for (count = 0; count < srcStr.length(); count++) {
    eeprom_update_byte(startAddr + count, srcStr[count]);
  }
  if (count - maxLength) eeprom_update_byte(startAddr + count, 0x0);
}



void setup() {
  //Запишем стартовые значение
  //Байты
  eeprom_update_byte(rTypeA, 0);
  eeprom_update_byte(alarmSA, 0);  //статус будильника
  eeprom_update_byte(alarmHA, 0);  //часы пробуждения
  eeprom_update_byte(alarmMA, 0);  //минуты пробуждения
  eeprom_update_byte(alarmOffsA, 0); //СМЕЩЕНИЕ ПО ВРЕМЕНИ
  eeprom_update_byte(DHCPStatA, 0); //Статус DHCP
  //IP Клиента
  eeprom_update_byte(clientIPA, 192);
  eeprom_update_byte(clientIPA + 1, 168);
  eeprom_update_byte(clientIPA + 2, 1);
  eeprom_update_byte(clientIPA + 3, 1);
  //IP Сервера
  eeprom_update_byte(serverIPA, 192);
  eeprom_update_byte(serverIPA + 1, 168);
  eeprom_update_byte(serverIPA + 2, 1);
  eeprom_update_byte(serverIPA + 3, 254);
  //Строки
  eeStringWrite("admin", loginStringA, loginStringL);
  eeStringWrite("password", passwordStringA, passwordStringL);
  eeStringWrite("AccessPoint", APstringA, APstringL);
  eeStringWrite("default", SSIDstringA, SSIDstringL);
  eeStringWrite("012345678901234567890", WPAkeyA, WPAkeyL);
  eeStringWrite("0000", PINA, PINL);
}

void loop() {

}
