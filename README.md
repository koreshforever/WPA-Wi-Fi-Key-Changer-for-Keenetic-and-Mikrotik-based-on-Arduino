# Wi-Fi WPA Key Changer for Mikrotik and Keenetic based on Arduino
Wi-Fi WPA key changer for Mikrotik and ZyXEL Keenetic routers using Telnet.

This was conceived as a terminal that generates a temporary password for guest access to Wi-Fi.

The connection data is displayed on the display in the form of a QR code, or using text.

The project used:
1. Controller Thinary Nano Every.
2. 128x128 display on the ST7735 controller.
3. Real-time clock on the DS3231 chip.
4. Network module on the W5500 controller.

To build, you need a Thinary Nano Every controller.
You need to make edits according to the instructions in the Docs folder.
The schema is located in the Docs folder.
It will probably work on Arduino Nano Every.

To initialize the memory, you need to flash the EEPROM_Initializer

For the English message to flash Terminal_ENG

To configure it, you need to hold down both buttons and apply power.
Corresponding actions for buttons:
1. The incrementing short push, long push back.
2. Decrementvalue short pressing the confirmation of the long touch.

For the next character to appear, wait 2 seconds after pressing.

The default PIN is 0000. If this is not the case, then re-flash the EEPROM_Initializer.

# Менятель Wi-Fi WPA ключа для Mikrotik и Keenetic на базе Arduino
Менятель Wi-Fi WPA ключа для роутеров Mikrotik и ZyXEL Keenetic, использующий Telnet.

Данное задумывалось как терминал, генерирующий временный пароль для гостевого доступа к Wi-Fi.

Данные для подключения отображаются на дисплее в виде QR-кода, либо с помощью текста.

В проекте использовались:
1. Контроллер Thinary Nano Every.
2. Дисплей 128x128 на контроллере ST7735 .
3. Часы реального времени на микросхеме DS3231.
4. Сетевой модуль на контроллере W5500.

Для сборки нужен контроллер Thinary Nano Every. 
Требуется осуществить правки согласно инструкциям в папке Docs. 
Схема находится в папке Docs.
Возможно будет работать на Arduino Nano Every.

Для инициализации памяти нужно прошить EEPROM_Initializer

Для русских сообщений прошить Terminal_RUS

Для настройки нужно зажать обе кнопки и подать питание.
Соответствующие действия для кнопок:
1. Инкрементирование коротким нажатием, назад длинным нажатием.
2. Декрементирование коротким нажатием, подтверждение длинным нажатием.

Для появления следующего символа ждать 2 секунды после нажатия.

PIN-код по умолчанию 0000. Если это не так, то повторно прошить EEPROM_Initializer.