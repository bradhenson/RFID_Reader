# RFID_Henson

RFID_Henson provides a simple RFID reader application for storing up 
to 10 users in program memory and then compares a scanned card to each of the users. 
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

At anytime while in programming mode, if no action is taken the system will timeout and 
return to a normal operating state (Ready to Read). The timeout can be adjusted by changing
the numerical value defined as TIMEOUT.

The Red and Green LEDs will indicate one of four states. If the Red LED is solid on, then the reader
is not ready to read. If the Green LED is solid on the reader is ready for a card to be swiped. When
a card is found to be a match to a user in memory, then the Green LED will flash several times. When
no match is found the Red LED will flash several times.

Hardware connections are as followed:
-------------------------------------
- Arduino 4 to RFID Rx
- Arduino GND to RFID GND
- Arduino Digital pin 2 to RFID enable
- Arduino +5V to RFID VCC pin.
- Arduino Digital pin 5 used to supply a signal to the locking mechanism
- LCD RS pin to digital pin 12
- LCD Enable pin to digital pin 11
- LCD D4 pin to digital pin 10
- LCD D5 pin to digital pin 9
- LCD D6 pin to digital pin 8
- LCD D7 pin to digital pin 7
- LCD R/W pin to ground
- LCD VSS pin to ground
- LCD VCC pin to 5V
- LCD VO pin (pin 3) is used to dim the display via a pot, could use PWM for this
- Arduino Digital pin 3 used to connect to a button for initiating programming mode
- Arduino Analog pin 0 used to connect to a button for ENTER
- Arduino Analog pin 1 used to connect to a button for NEXT
- Arduino Analog pin 4 used to connect to a Green LED
- Arduino Analog pin 5 used to connect to a Red LED
- Arduino Analog pin 2 used to connect to a button for BYPASS

Additional hardware considerations:
-------------------------------------
- This application was written to work with the Parallax RFID Reader #28140. Some alternatives do 
  exist but have been tested with this application. This includes RFIDuino, which uses the same
  analog RFID front end chip (EM4095). How the RFIDuino transimits the 10 digit char array may not
  be the same as the Parallax implementation.
- The Red and Green leds are connected to ground via a pull down resistor, which is 470 ohms each.
- The connections to the external components (ie, Red and Greed led, Relay, and RFID Reader
  are all connected using screw post terminals.
- Another LED can be put in line with the RELAY pin to give a visual
  inidication that power is being applied to the relay.
