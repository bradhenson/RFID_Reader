/********************************************************************************
Author: Bradford Henson
Date: 11/3/2016
Version: 0.0.0.5
License: USE AT YOUR OWN RISK

Description: 
------------
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

At anytime while in programming mode, if no action is taken the system will timeout and 
return to a normal operating state (Ready to Read). The timeout can be adjusted by changing
the numerical value defined as TIMEOUT.

The Red and Green LEDs will indicate one of four states. If the Red LED is solid on, then the reader
is not ready to read. If the Green LED is solid on the reader is ready for a card to be swiped. When
a card is found to be a match to a user in memory, then the Green LED will flash several times. When
no match is found the Red LED will flash several times.

Hardware connections are as followed:
-------------------------------------
Arduino 4 to RFID Rx
Arduino GND to RFID GND
Arduino Digital pin 2 to RFID enable
Arduino +5V to RFID VCC pin.
Arduino Digital pin 5 used to supply a signal to the locking mechanism
LCD RS pin to digital pin 12
LCD Enable pin to digital pin 11
LCD D4 pin to digital pin 10
LCD D5 pin to digital pin 9
LCD D6 pin to digital pin 8
LCD D7 pin to digital pin 7
LCD R/W pin to ground
LCD VSS pin to ground
LCD VCC pin to 5V
LCD VO pin (pin 3) is used to dim the display via a pot, could use PWM for this
Arduino Digital pin 3 used to connect to a button for initiating programming mode
Arduino Analog pin 0 used to connect to a button for ENTER
Arduino Analog pin 1 used to connect to a button for NEXT
Arduino Analog pin 4 used to connect to a Green LED
Arduino Analog pin 5 used to connect to a Red LED
Arduino Analog pin 2 used to connect to a button for BYPASS

Additional considerations:
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
**********************************************************************************/

 #include <EEPROM.h>
 #include <LiquidCrystal.h>
 #include <SoftwareSerial.h> 

 #define enablePin  2       // Connects to the RFID's ENABLE pin
 #define rxPin      4       // Serial input (connects to the RFID's SOUT pin)
 #define txPin      11      // Serial output (unused)
 #define BUFSIZE    11      // Size of receive buffer (in bytes) (10-byte unique ID + null character)
 #define RFID_START  0x0A   // RFID Reader Start byte
 #define RFID_STOP   0x0D   // RFID Reader Stop byte
 #define GREEN_LED A4       // Green indicator light is connected to bit 2 on PORT C register
 #define RED_LED A5         // Red indicator light is connected to bit 2 on PORT C register
 #define ENTER_BUTTON 0     // the Enter button is connected to bit 0 on PORT C register
 #define NEXT_BUTTON 1      // the Next button is connected to bit 1 on PORT C register
 #define BYPASS_BUTTON 2    // the BYPASS button is connected to bit 2 on PORT C register
 #define RELAY 5            // this defines the pin that the relay will be connected to

/********************************************************************************


Define the amount of time in milli seconds the relay should stay on


*********************************************************************************/

#define RELAY_TIME 3000    //Change the numerical value (milliseconds)

/********************************************************************************


Define the hardcoded TIMEOUT Time when executing a programming event


**********************************************************************************/

#define TIMEOUT 2000      // Timeout counter time (in cycles, not milliseconds) 

/********************************************************************************

Define the hardcoded RFID tag to be used as backup user to get past the security check
The backUpUser will not have swipe access to the lock, it is used for programming mode
in the event the other master card is lost.

**********************************************************************************/

 char backUpUser[] = "ABCD543210";


 SoftwareSerial rfidSerial =  SoftwareSerial(rxPin, txPin); // set up a new serial port for RFID reader
 LiquidCrystal lcd(12, 11, 10, 9, 8, 7); // initialize the library with the numbers of the interface pins

 char rfidData[BUFSIZE];            // Buffer for incoming data (when reading the RFID Card)
 char offset = 0;                   // Offset into buffer (when reading the RFID Card)
 uint8_t selectedUser = 0;          // Create a selected user variable (programming function)
 uint8_t swipeState = 0;            // Used to determine if a swipe has occured
 boolean match = false;             // Create a variable used determine if a card matches one of the saved user values
 uint8_t compareCounter = 0;        // Create a variable used as a counter in the compare while loop
 uint8_t programButtonState = 0;    // Create a variable used to determine when the programming button was pushed
 int timeOutCounter = 0;            // Create a timeout counter to use through out the programming mode section
 boolean nextButtonState = 0;       // Used to determine if the Next Button is being pressed
 boolean enterButtonState = 0;      // Used to determine if the Enter Button is being pressed
 boolean nextUserFlag = 0;          // Create a user flag for programming mode function
 boolean enterButtonFlag = 0;       // Create a button flag for programming mode function
 boolean bypassButtonState = 0;     // Used to determine if the Bypass Button is being pressed
 boolean invalidMasterCard = 0;     // Used to determine if an invalid Master card was used during programming mode
/*********************************************************************************
The following user addresses identify the starting location in EEPROM where that
specific user's code will be stored. Example: Codes are 10 position int arrays, so position 
0 through 9 represent user one, 20 through 29 represent user two.
*********************************************************************************/
 int users[10] = {0, 20, 40, 60, 80, 100, 120, 140, 160, 180}; 

/*******************************************************************************************************
* 
* 
*                                           ARDUINO SETUP SECTION 
*                                
* 
********************************************************************************************************/
void setup() { 

pinMode(enablePin, OUTPUT);     // Sets the enablePin as an output (for the RFID Reader)
pinMode(rxPin, INPUT);          // Sets the rxPin as an output (for the RFID Reader)
digitalWrite(enablePin, HIGH);  // disable RFID Reader  
pinMode(RELAY, OUTPUT);         // Set digital pin 4 as OUTPUT to be used for the locking mechanism
pinMode(3, INPUT);              // Set pin 3 as an input
pinMode(GREEN_LED, OUTPUT);     // set analog pin GREEN_LED as an output
pinMode(RED_LED, OUTPUT);       // Set analog pin RED_LED as an output

DDRC &= ~(1 << NEXT_BUTTON);
DDRC &= ~(1 << ENTER_BUTTON);
DDRC &= ~(1 << BYPASS_BUTTON);

lcd.begin(16,2);                // Set up the LCD's number of columns and rows

attachInterrupt(1, programButton, FALLING); // Setup the external interrupt for pin 3

Serial.begin(9600);             // setup Arduino Serial Monitor
while (!Serial);                // wait until Serial is ready

rfidSerial.begin(2400);         // set the baud rate for the SoftwareSerial port
}  
/*******************************************************************************************************
* 
* 
*                                           ARDUINO LOOP SECTION
*                                
* 
********************************************************************************************************/
 void loop() { 

  lcd.setCursor(0, 0);             // Set the cursor location setCursor(column, row)
  lcd.print("Reader Is Ready ");   // Set up the initial message   
  lcd.setCursor(0, 1);             // Set the cursor location setCursor(column, row)
  lcd.print("                ");   // Clears out the second row of the LCD

//Initialize the reader and start waiting for a swipe, once a read has occured
//the value is stored as rfidData[], then close out the reader 

  rfidData[0] = 0;                  // Clear the buffer 
  swipeState = 0;                   // Set the state as no card has been swiped yet
  
  serialRecieveFlush(); //Flush the recieve data from the Serial Stream - prevents double scans
  
  digitalWrite(enablePin, LOW);           // enable the RFID Reader
  Serial.println("Ready for RFID Swipe"); // Output to serial that the reader is ready
  
  //Start waiting for a swipe, this program should be in this loop 90% of the time

  digitalWrite(GREEN_LED, HIGH); // When the card reader is ready to read a card, turn on the Green LED
  digitalWrite(RED_LED, LOW);    // When the card reader is ready to read a card, turn off the Red LED
 
  while(1)   // Wait for a response from the RFID Reader
   {
      performCardRead();
      if (swipeState == true) break;
      
      //If the programming button was pushed during the waiting to read loop, execute programming function
      if (programButtonState == 1) 
      {
        programmingMode();
          lcd.setCursor(0, 0);             // Set the cursor location setCursor(column, row)
          lcd.print("                ");   // Set up the initial message   
          lcd.setCursor(0, 1);             // Set the cursor location setCursor(column, row)
          lcd.print("                ");   // Clears out the second row of the LCD
          swipeState = false;              // Sets the state of swipe to false because we just exited programming mode
          rfidData[0] = 0;                 // Clear the buffer 
        break;
      }
   }
  digitalWrite(RED_LED, HIGH);      // The reader is not ready, turn on the Red LED
  digitalWrite(GREEN_LED, LOW);     // The reader is not ready, turn off the Green LED
  digitalWrite(enablePin, HIGH);    // disable RFID Reader 
   
  Serial.print("The card that was swiped is: "); // Message to be printed to the serial interface after a swipe
  Serial.println(rfidData);         // The rfidData string should now contain the tag's unique ID with a null termination
  Serial.flush();                   // Wait for all bytes to be transmitted to the Serial Monitor
   
  delay(500);                       // this delay haults the uC just a little before moving on to the compare                  

/*********************************************************************************
 The following while loop will compare the current swiped RFID code to the ones
 saved in EEPROM. The loop will only execute a total of 10 times or until a match
 is found. When a match is found, the variable state is set to true and code breaks
 out of the loop.
*********************************************************************************/
 compareCounter = 0;          // set the whileCounter back to zero for next cycle
 if(swipeState == 1)          // if a swipe has occured, start comparing the results
 {
  do 
  {                
      lcd.setCursor(0, 0);           // Set the cursor location setCursor(column, row)
      lcd.print("Comparing Tags  "); // indicate on the LCD the current state 

      //The Char readInArray is initalized with some data becuase Arduino kept screwing up
      //without this being done. There would be extra data points appended to the string
      //even when it was ended with a null character. Initializing with some data will get the
      //compiler to add the null character, and everything works as intended.                      
      char readInArray[] = "0123456789";   // char array used to store values from EEPROM
      for (uint8_t i = 0; i < 10; i++) //loop allows each char to be read for a given user
      {
        //contents of EEPROM are read in one byte at a time
        readInArray[i] = EEPROM.read(users[compareCounter] + i); 
      } 
      //Serial.println(readInArray); // indicate out to the serial the current state
      //the newly created readInArray is compared to swiped RFID code
      //the output of the strcmp() function is 0 when the strings match
      if (strcmp(readInArray, rfidData) == 0) 
      {   
        match = true;          
        break;   
      }
      else
      {     
        match = false;
      }
      // increments the counter to prevent the while loop from not ending
      // the copareCounter is also used when reading the EEPROM data
      compareCounter++;      // this will make sure that the compare loop only happens 10 times  
   }while (compareCounter < 10); 
 }
/*********************************************************************************  
  The following section is where we use the information obtained when comparing RFIDs
  to the values saved in EEPROM. 
*********************************************************************************/    
 if (swipeState == 1) // If a swipe has occured, do something with the results of - match -
 {
  if (match == true)                     // When a match is found do this
  {
    digitalWrite(RELAY, HIGH);           // if a match is found, set pin 3 to HIGH
    lcd.setCursor(0, 0);                 // Set the cursor location setCursor(column, row)
    lcd.print("Valid Card      ");       // indicate on the LCD that a match was found
    lcd.setCursor(0, 1);                 // Set the cursor location setCursor(column, row)
    lcd.print("Unlocking Now   ");
    Serial.println("Card is valid, unlocking"); // Tell the serial interface a match was found
    digitalWrite(RED_LED, LOW);          // Turn off the red LED, long enough to toggle the green ones three times
     
    for ( uint8_t i = 0; i < 3; i++)     // Toggle the Green LEDs three times
    {
     digitalWrite(GREEN_LED, HIGH);
     delay(200);
     digitalWrite(GREEN_LED, LOW);
     delay(200); 
    }
     
    digitalWrite(RED_LED, HIGH);         // Turn the Red LED back on, card reader is not ready for a swipe
    delay(RELAY_TIME);                   //This is a delay for how long the relay needs a signal
    digitalWrite(RELAY, LOW);            //turn the relay off
  }
  else                                   // When no match is found do this
  {
   lcd.setCursor(0, 0);                  // Set the cursor location setCursor(column, row)
   lcd.print("Card Not Valid");          // indicate on the LCD that no match found
   lcd.setCursor(0, 1);                  // Set the cursor location setCursor(column, row)
   lcd.print("Try Another Card");
   Serial.println("Card Not Valid");     // Tell the serial interface no match found
       
   for ( uint8_t i = 0; i < 3; i++)      // Toggle the Red LEDs a few times
   {
     digitalWrite(RED_LED, LOW);
     delay(200);
     digitalWrite(RED_LED, HIGH);
     delay(200); 
   }
   delay(500); //This just slows down the event so that "No match was found" can be displayed on the LCD
  }

  //If the programming button was pushed outside the waiting to read loop, execute programming function
  if (programButtonState == 1) programmingMode(); 
 }  
} //This is the end of the Loop() function
   
  /*******************************************************************************************************
  * 
  * 
  *                                         FUNCTION SECTION
  *                                
  * 
  ********************************************************************************************************/
  /*******************************************************************************************************
 * Function Name: programButton()
 * 
 * Author: Bradford Henson
 * 
 * Description: When the programming button is pressed it iniates an Interrupt Service Routine. This funtion
 * serves as the Interrupt Service Routine and sets the programButtonState variable to 1. This can be used to 
 * trigger putting the device in programming mode.
 *******************************************************************************************************/
 void programButton()
 {
  programButtonState = 1;
  return; 
 }
  
 /*******************************************************************************************************
 * Function Name: serialRecieveFlush()
 * 
 * Author: Bradford Henson
 * 
 * Description: The Serial.flush() function will only flush the serial stream on the transmit side. 
 * To prevent the card reader from having double or multiple reads with one swipe of the card, the 
 * recieve side of the serial stream has to be flushed. This rfidSerial.read will get executed after the 
 * the rfidData[] is flushed out. Meaning there won't be a card number to push through the readCard() function 
 * and prevents there being multiple readings.
 *******************************************************************************************************/
 void serialRecieveFlush()
 {
  while(rfidSerial.available())
  rfidSerial.read(); 
  return; 
 }
  
 /*******************************************************************************************************
 * Function Name: performCardRead()
 * 
 * Author: Parallax Wwbsite Code Example, author is uknown
 * 
 * Description: The following function was provided by the manufacture of the reader (Parallax).
 * It was downloaded from the Parallax product website as a code example, then was put wrapped
 * as a function for this application.
 * 
 * When the RFID Reader is active and a valid RFID tag is placed with range of the reader, 
 * the tag's unique ID will be transmitted as a 12-byte printable ASCII string to the host 
 * (start byte + ID + stop byte)
 * 
 * For example, for a tag with a valid ID of 0F0184F07A, the following bytes would be sent:
 * 0x0A, 0x30, 0x46, 0x30, 0x31, 0x38, 0x34, 0x46, 0x30, 0x37, 0x41, 0x0D
 * We'll receive the ID and convert it to a null-terminated string with no start or stop byte.
 * 
 * The only thing that was changed from orginal source code is the addition of a swipeState variable, used
 * to determine the state during a programming event. 
 *******************************************************************************************************/
 void performCardRead()
 {
 // If there are any bytes available to read, then the RFID Reader has probably seen a valid tag
 if (rfidSerial.available() > 0) 
      {
        rfidData[offset] = rfidSerial.read();  // Get the byte and store it in our buffer
        // If we receive the start byte from the RFID Reader, then get ready to receive the tag's unique ID
        if (rfidData[offset] == RFID_START)    
        {
          offset = -1;     // Clear offset (will be incremented back to 0 at the end of the loop)
        }
        // If we receive the stop byte from the RFID Reader, then the tag's entire unique ID has been sent
        else if (rfidData[offset] == RFID_STOP)  
        {
          rfidData[offset] = 0; // Null terminate the string of bytes we just received
          swipeState = 1;
        }
        offset++;  // Increment offset into array
        // If the incoming data string is longer than our buffer, wrap around to avoid going out-of-bounds
        if (offset >= BUFSIZE) offset = 0; 
      }
      
  return; 
 }
 /*******************************************************************************************************
 * Function Name: selectUserInterface()
 * 
 * Author: Bradford Henson
 * 
 * Description: This function is used during a programming event, each time the Next button is pressed
 * this function will determine if the next user is wanted to be displayed or if the current user is selected.
 * There is also a time out feature to prevent the loop from never ending.
 *******************************************************************************************************/
 
 void selectUserInterface(uint8_t user)
 {                   
   timeOutCounter = 0;                     // Zero out the time out counter before entering the loop
   while(1)
   {
     Serial.println(timeOutCounter);       // Tell the serial interface what the count is
     if ((PINC & (1 << NEXT_BUTTON)) == 0) // if the Next button is pressed, do the following
     {                                     // The pin is compared to 0 because it is being pulled up by the debouce hardware
       selectedUser = user + 1;            // Set the selectedUser to the next available one, becuase we want the next user
       nextUserFlag = 1;                   // keeps us in the select user portion of the programming mode
       delay(200);                         // Just slows down uC for humans
      break;                               // break out of this while loop
      }
      
      if ((PINC & (1 << ENTER_BUTTON)) == 0) //if the Enter button is pressed, do the following
      {
        nextUserFlag = 0;                    // We don't want to be in the select user portion anymore
        enterButtonFlag = 1;                 // flags that a user was selected, used to exit the user selection while loop
        selectedUser = user;                 // sets the variable -slectedUser- to the currently selected user                
        delay(200);                          // Just slows down uC for humans   
       break;                                // break out of this while loop
        }
       
       timeOutCounter++;                      // Increment the time out counter
       if (timeOutCounter > TIMEOUT)          // When the counter reaches the defined (at the top) value, break out of loop
       {
        selectedUser = 0;                     // If no user was selected (ie timeout happended), sets variable to 0
        break;                                // If selectedUser is 0, the user won't be prompted to swipe RFID card
       }
   }
   return;
 }
 
 /*******************************************************************************************************
 * Function Name: programmingMode()
 * 
 * Author: Bradford Henson
 * 
 * Description: This will put the Arduino in a Programming Mode so that a new
 * RFID code can be programmed to EEPROM at a selected user position.
 *******************************************************************************************************/
void programmingMode(void)
{
  programButtonState = 0;         //Sets the state back to 0 so that the button can trigger another event later
  digitalWrite(enablePin, HIGH);  // deactivate the RFID reader
  timeOutCounter = 0;             // Set the time out counter to zero
  enterButtonFlag = 0;            // Set initial state of the enter button flag (used for exiting the while loop)
  invalidMasterCard = 0;
  boolean masterSwipeResult;      // used to store the result of the master card swipe as a boolean value
  int masterUsers[2] = {0, 200};  // the two master position are 0 and 200, 200 is hardcoded
  swipeState = 0;
  rfidData[0] = 0;                // Clear the buffer 

  
    /********************************************************************************************************
    * Check to see if the hardcoded backup user is in memory, if not write a predefined value (at the top)
    * to EEPROM memory. This should not happen very often, but is needed incase the EEPROM was erase 
    * somehow. This would allow the backup user to program new cards as users.
    *******************************************************************************************************/
      //The Char readInArray is initalized with some data becuase Arduino kept screwing up
      //without this being done. There would be extra data points appended to the string
      //even when it was ended with a null character. Initializing with some data will get the
      //compiler to add the null character, and everything works as intended.
      char backUpUserTemp[] = "0123456789";   // char array used to store values from EEPROM
      for (uint8_t i = 0; i < 10; i++) //loop allows each char to be read for a given user
      {
        //contents of EEPROM are read in one byte at a time
        backUpUserTemp[i] = EEPROM.read(masterUsers[1] + i); 
      } 
      //the newly created masterSwipe[] is compared to swiped RFID code
      //the output of the strcmp() function is 0 when the strings match
      if (strcmp(backUpUserTemp, backUpUser) != 0) 
      {  
        Serial.println("had to update the back up user in EEPROM");  
        // write the new RFID value to EEPROM for a specified user
        for (uint8_t i = 0; i < 10; i++)
        {
         EEPROM.write(masterUsers[1] + i, backUpUser[i]);
        }          
      }

  lcd.setCursor(0, 0);                        // Set the cursor location setCursor(column, row)
  lcd.print("Master Card Is   ");             // indicate on the LCD that it is in programming mode
  lcd.setCursor(0, 1);                        // Set the cursor location setCursor(column, row)
  lcd.print("Required        ");              // Set the LCD to press enter to continue

  digitalWrite(enablePin, LOW);    // enable the RFID Reader
  digitalWrite(GREEN_LED, HIGH);   // Turn on the Green LED, the reader is ready for a card
  digitalWrite(RED_LED, LOW);      // Turn off the Red LED, the reader is ready for a card
  
  serialRecieveFlush(); //Flush the recieve data from the Serial Stream - prevents double scans
  
  do //Start waiting for a swipe or the bypass button, limit the time it waits by xxxx cycles
  {
    if ((PINC & (1 << BYPASS_BUTTON)) == 0)
    {
      Serial.println(BYPASS_BUTTON);
      swipeState = 2;
      break;
    }
    
     performCardRead();
     if (swipeState == 1) break;
      
     timeOutCounter++;                  // Increment the timeout counter
     Serial.println(timeOutCounter);    // Send the value of the counte to serial, for testing purposes

  } while (timeOutCounter < TIMEOUT);
  
  digitalWrite(RED_LED, HIGH);          // Turn on the Red LED, the reader is not ready
  digitalWrite(GREEN_LED, LOW);         // Turn off the Green LED, the reader is not ready
  digitalWrite(enablePin, HIGH);        // disable RFID Reader 
  delay(200);
  
  if (swipeState == 1)
  {
  compareCounter = 0;
  do
  {                                             
      lcd.setCursor(0, 0);           // Set the cursor location setCursor(column, row)
      lcd.print("Comparing Tags  "); // indicate on the LCD the current state 
      lcd.setCursor(0, 1);           // Set the cursor location setCursor(column, row)
      lcd.print("                "); // indicate on the LCD the current state
      
      char newArray[] = "0123456789";   // char array used to store values from EEPROM
      for (uint8_t i = 0; i < 10; i++)  //loop allows each char to be read for a given user
      {
        //contents of EEPROM are read in one byte at a time
        newArray[i] = EEPROM.read(masterUsers[compareCounter] + i);
      }
      //the newly created masterSwipe[] is compared to swiped RFID code
      //the output of the strcmp() function is 0 when the strings match
      if (strcmp(newArray, rfidData) == 0) 
      {   
        masterSwipeResult = true;
        break;            
      }
      else
      {     
        //Setting these three control flags will cause the application to drop to the end of the
        //program function and present the user with an invalid master card message
        masterSwipeResult = false;
        swipeState = 0;
        invalidMasterCard = 1;
        
        lcd.setCursor(0, 0);           // Set the cursor location setCursor(column, row)
        lcd.print("Invalid Master  "); // indicate on the LCD the current state 
        lcd.setCursor(0, 1);           // Set the cursor location setCursor(column, row)
        lcd.print("Card            "); // indicate on the LCD the current state
        delay(1000); 
        
      }
      // increments the counter to prevent the while loop from not ending
      // the copareCounter is also used when reading the EEPROM data
      compareCounter++;      // this will make sure that the compare loop only happens 2 times 
    }while (compareCounter < 2); // The compare counter would have to be adjusted if more users are added
  }
 digitalWrite(RED_LED, HIGH);                   // Turn on the Red LED, reader is not ready
 digitalWrite(GREEN_LED, LOW);                  // Turn off the Greed LED, reader is not ready
 
 if ((swipeState == 1 && masterSwipeResult == 1) || swipeState == 2)
 {
 timeOutCounter = 0;
  
  do //The following loop provides the user selection interface during a programming event
  {    
      Serial.println(timeOutCounter);         // Send the timeout counter value to serial

       if (masterSwipeResult == 1 || swipeState == 2)
       {
         selectedUser = 1;                    // Sends the program to the first user in the following SWITCH statement
         nextUserFlag = 1;                    // keeps us in the select user portion of the programming mode
         masterSwipeResult = 0;
         swipeState = 0;
       }
                
       if (enterButtonFlag == 1)              // when the enter button flag is true or 1, break out of the select user loop
       {
        break;
       }                                     
       if (nextUserFlag == 1)                 // This the user selection portion of the programming mode
       {                
           switch (selectedUser)  // This will allow the users to be cycled through based on the the selectedUser variable
           {
                case 1:                       
                    
                    //The function calls for the display could be moved to the selectUserInterface() function. 
                    //However, the value being passed to the function is an integer and I was having trouble 
                    //with using an integer directly in the LCD.print() function. I would imagine this value 
                    //would need to be a string data type.                
                    lcd.setCursor(0, 0);               // Set the inital LCD Screen state for the user
                    lcd.print("User 1 Press -  ");     // Sets the message on the LCD
                    lcd.setCursor(0, 1);               // Set the cursor location to the bottom left position
                    lcd.print("ENTER or NEXT   ");     // Prompt for either the Enter button or the Next button
                    delay(500);                        // Slow the uC down to prevent it jumping directly to the next state 
                    selectUserInterface(selectedUser); // Do something based on which button is pressed
                  break;
                case 2:
                    lcd.setCursor(0, 0);    
                    lcd.print("User 2 Press -  ");
                    lcd.setCursor(0, 1);    
                    lcd.print("ENTER or NEXT   ");
                    delay(500);
                    selectUserInterface(selectedUser);
                  break;
                case 3:
                    lcd.setCursor(0, 0);    
                    lcd.print("User 3 Press -  ");
                    lcd.setCursor(0, 1);    
                    lcd.print("ENTER or NEXT   ");
                    delay(500);
                    selectUserInterface(selectedUser);
                  break;
                case 4:
                    lcd.setCursor(0, 0);    
                    lcd.print("User 4 Press -  ");
                    lcd.setCursor(0, 1);    
                    lcd.print("ENTER or NEXT   ");
                    delay(500);
                    selectUserInterface(selectedUser);
                  break;
                case 5:
                    lcd.setCursor(0, 0);    
                    lcd.print("User 5 Press -  ");
                    lcd.setCursor(0, 1);    
                    lcd.print("ENTER or NEXT   ");
                    delay(500);
                    selectUserInterface(selectedUser);
                  break;
                case 6:
                    lcd.setCursor(0, 0);    
                    lcd.print("User 6 Press -  ");
                    lcd.setCursor(0, 1);    
                    lcd.print("ENTER or NEXT   ");
                    delay(500);
                    selectUserInterface(selectedUser);
                  break;
                case 7:
                    lcd.setCursor(0, 0);    
                    lcd.print("User 7 Press -  ");
                    lcd.setCursor(0, 1);    
                    lcd.print("ENTER or NEXT   ");
                    delay(500);
                    selectUserInterface(selectedUser);
                  break;
                case 8:
                    lcd.setCursor(0, 0);    
                    lcd.print("User 8 Press -  ");
                    lcd.setCursor(0, 1);    
                    lcd.print("ENTER or NEXT   ");
                    delay(500);
                    selectUserInterface(selectedUser);
                  break;
                case 9:
                    lcd.setCursor(0, 0);    
                    lcd.print("User 9 Press -  ");
                    lcd.setCursor(0, 1);    
                    lcd.print("ENTER or NEXT   ");
                    delay(500);
                    selectUserInterface(selectedUser);
                  break;
                case 10:
                    lcd.setCursor(0, 0);    
                    lcd.print("User 10 Press -  ");
                    lcd.setCursor(0, 1);    
                    lcd.print("ENTER or NEXT   ");
                    delay(500);
                    selectUserInterface(selectedUser);
                    if (selectedUser == 11)
                    {
                      selectedUser = 1; //By setting the selectedUser back to 1, the application will cycle back thru
                    }
                  break;                                
              }
            }        
    timeOutCounter++;                   // Increment the time out counter
   } while (timeOutCounter < TIMEOUT);  // When the counter reaches the defined (at the top) value, break out of loop

  delay(50); // this delay prevents the program from jumping into the following if statement unintentially
  
  if (selectedUser > 0)   // If the selectedUser is 0, then skip asking the user to swipe a card
  {
 
  lcd.setCursor(0, 0);                   // Set the LCD up to prompt the user to swipe a card
  lcd.print("Programming Mode");
  lcd.setCursor(0, 1);
  lcd.print("Swipe New Card  ");
  
  digitalWrite(GREEN_LED, HIGH);         // Turn on the Green LED, the reader is ready for a card
  digitalWrite(RED_LED, LOW);            // Turn off the Red LED, the reader is ready for a card
  
  rfidData[0] = 0;         // Clear the buffer 
  serialRecieveFlush();    //Flush the recieve data from the Serial Stream - prevents double scans
  digitalWrite(enablePin, LOW);       // enable the RFID Reader
  timeOutCounter = 0;
  swipeState = 0;
 
  serialRecieveFlush(); //Flush the recieve data from the Serial Stream - prevents double scans
  
  do  //Start waiting for a swipe, limit the time it waits by xxxx cycles
  {
     performCardRead();
     if (swipeState == 1) break;
      
     timeOutCounter++;                          // Increment the timeout counter
     Serial.println(timeOutCounter);            // Send the value of the counte to serial, for testing purposes
  } while (timeOutCounter < TIMEOUT);           // When the counter reaches the defined (at the top) value, break out of loop
  
  digitalWrite(RED_LED, HIGH);                  // Turn on the Red LED, the reader is not ready
  digitalWrite(GREEN_LED, LOW);                 // Turn off the Green LED, the reader is not ready
  digitalWrite(enablePin, HIGH);                // disable RFID Reader 
  delay(500);                                   // this delay haults the uC before moving on to something else  
  
  if (swipeState == 1)  // If a swipe did occur, write the value to EEPROM at the selectedUser position in memory
  {
     lcd.setCursor(0, 0);                       // Set the cursor location setCursor(column, row)
     lcd.print("Input Recieved  ");             // Sets the desired message to the display
     uint8_t userAddress = selectedUser - 1;    // The selectedUser will be one value higher then the users[] array
                                                // Subtracting one allows us to reference the users[] array directly
      // write the new RFID value to EEPROM for a specified user
      for (uint8_t i = 0; i < 10; i++)
      {
       EEPROM.write(users[userAddress] + i, rfidData[i]);
      }
      
       lcd.setCursor(0, 0);                    // Set the cursor location setCursor(column, row)          
       lcd.print("Programming Mode");          // Idicates that we are still in programming mode on the LCD
       lcd.setCursor(0, 1);                    // Set the cursor location setCursor(column, row)
       lcd.print("User ID Updated ");          // indicate on the LCD that a user ID updated
       delay(2000);                            // Slows the uC down for humans to see that User ID Updated on the LCD
       Serial.println("User ID Updated");      // Prints message to serial interface
       
     }
   }
 }
 programButtonState = 0; // At the conclusion of programming mode function, set the state back to 0

 if (invalidMasterCard == 0)                   // If a invalid master card was used, don't display the timeout message
   {    
     if (swipeState == 0 || selectedUser == 0) // If there was a timeout, display a message on the LCD
     {
           lcd.setCursor(0, 0);                    // Set the cursor location setCursor(column, row)          
           lcd.print("Programming Mode");          // Idicates that we are still in programming mode on the LCD
           lcd.setCursor(0, 1);                    // Set the cursor location setCursor(column, row)
           lcd.print("Timed Out       ");          
           delay(2000);                            // Slows the uC down for humans to see the timeout on the LCD
           Serial.println("Programming Mode Timed Out");      // Prints message to serial interface
     }
  }
}
