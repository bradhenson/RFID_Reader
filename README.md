This project was developed to provide communcation between a Parallax RFID reader, 
Arduino UNO, and a custom user interface (LCD, Buttons..).

RFID_Henson provides a simple RFID reader application for storing up to 10 users 
in the Arduino's EEPROM and then compares a scanned card to each of the users. 
The output of a successful scan would send a 5 volt signal to the RELAY pin.
An unsuccessful scan would prompt the user to try again with another card. This 
application also provides a user interface via an LCD and several buttons that allow
for programming new cards into memory for each of the 10 users. To enter programming 
mode, a master card is required (this is user 1). Once in programming mode, any of the
user positions can be over written with a new card value by simply scanning the new card
when promted. In the event a card is lost, there is a need to delete that value from memory.
Direct deletion is not possible, but it is intended that the master user would over write 
the user position that needs to be deleted with it's own card information. 
The total time the defined RELAY pin is turned on (HIGH) can be adjusted as a simple interger data 
type.

The Red and Green LEDs will indicate one of four states. If the Red LED is solid on, then the reader
is not ready to read. If the Green LED is solid on the reader is ready for a card to be swiped. When
a card is found to be a match to a user in memory, then the Green LED will flash several times. When
no match is found the Red LED will flash several times.

At anytime while in programming mode, if no action is taken the system will timeout and 
return to a normal operating state (Ready to Read). The timeout can be adjusted by changing
the numerical value defined as TIMEOUT.

The LCD backlight is set to turn off if the device is left inthe Ready to Read state for a 
specified number of cycles. Once the device exits the Ready to Read state, either by 
pressing the programming button or swiping a card, the LCD backlight will turn back on.

At start up the device will check to see if there is an SD card present. If no SD card is present,
a message will be displayed stating to check the card and recycle power. This will not stop
the unit from functioning normally, but will indicate that the logfile will not be written to
until the issue with the SD card is cleared up. Additionally, if the SD card is not available 
upon a successful unlock (match), then the an error message will be displayed stating to check the 
card and recycle power. This error will not stop the relay pin from becoming active. It only 
indicates that the logfile will not be updated.

In the event that all programmed cards are not available to get passed the requirement for a Master
card swipe when entering programming mode, it is possible to bypass this requirement if digital pin 9
is pulled to ground when the LCD prompts for a Master Card.

Hardware connections are as followed:
-------------------------------------
- Arduino 4 to RFID Rx
- Arduino GND to RFID GND
- Arduino Digital pin 2 to RFID enable
- Arduino +5V to RFID VCC pin.
- Arduino Digital pin 5 used to supply a signal to the locking mechanism
- Arduino Digital pin 5 has an LED connected to signal when this pin is HIGH
- Arduino Digital pin 3 used to connect to a button for initiating programming mode
- Arduino pin 8 used to connect to a button for ENTER
- Arduino pin 6 used to connect to a button for NEXT
- Arduino Analog pin 0 used to connect to a Green LED
- Arduino Analog pin 1 used to connect to a Red LED
- Arduino Analog pin 9 used for BYPASS
- Arduino SDA and SCL (I2C) will go to the LCD Backpack
- Arduino SDA and SCL (I2C) will also connect to the RTC via the shield
- Arduino 10, 11, 12, 13 (SPI) connect tot he SD card reader via the shield

Additional considerations:
-------------------------------------
- This application was written to work with the Parallax RFID Reader #28140. Some alternatives do 
  exist but have been tested with this application. This includes RFIDuino, which uses the same
  analog RFID front end chip (EM4095). How the RFIDuino transimits the 10 digit char array may not
  be the same as the Parallax implementation.
- An Adafruit datalogging shield is used to supply connections to an RTC and SD card. Standard libraries 
  list by Adafruit are used for these functions. The SD card is connected to the SPI points and the RTC
  is connected to I2C.
  https://learn.adafruit.com/adafruit-data-logger-shield
- A backpack is used with the LCD to provide connectivity with fewer pins. It also uses the I2C.
  The library used for the backpack in the prototype is from the following website. This effectively
  replaces (extends functionality) the standard LiquidCrystal library. 
  https://bitbucket.org/fmalpartida/new-liquidcrystal/downloads/NewliquidCrystal_1.3.4.zip
  A good turtorial for using this type of backpack is at:
  http://arduino-info.wikispaces.com/LCD-Blue-I2C
