/********************************************************************************
Hardware connections are as followed:
Arduino 4 to RFID Rx
Arduino GND to RFID GND
Arduino Digital pin 2 to RFID enable
Arduino +5V to RFID VCC pin.
Arduino Digital pin 13 used to supply a signal to the locking mechanism
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
Arduino Digital pin 5 used to connect to a buzzer/speaker
Arduino Analog pin 0 used to connect to a button for ENTER
Arduino Analog pin 1 used to connect to a button for NEXT
Arduino Analog pin 2 used to connect to a Green LED
Arduino Analog pin 3 used to connect to a Red LED
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
#define GREEN_LED 2        // Green indicator light is connected to bit 2 on PORT C register
#define RED_LED 3          // Red indicator light is connected to bit 2 on PORT C register
#define TIMEOUT 4000       // Timeout counter time (in CPU cycles, not millis) during programming mode
#define ENTER_BUTTON 0     // the Enter button is connected to bit 0 on PORT C register
#define NEXT_BUTTON 1      // the Next button is connected to bit 1 on PORT C register

/********************************************************************************
Define the amount of time in milli seconds the relay should stay on
*********************************************************************************/
#define RELAY_TIME 3000    //Change the numerical value to change the amount of time the relay should be on

SoftwareSerial rfidSerial =  SoftwareSerial(rxPin, txPin); // set up a new serial port for RFID reader
LiquidCrystal lcd(12, 11, 10, 9, 8, 7); // initialize the library with the numbers of the interface pins

char rfidData[BUFSIZE];            // Buffer for incoming data (when reading the RFID Card)
char offset = 0;                   // Offset into buffer (when reading the RFID Card)
uint8_t selectedUser = 0;          // Create a selected user variable (programming function)
boolean swipeState = 0;            // Create a variable used to determine card reading state (programming function)
boolean match = false;             // Create a variable used determine if a card matches one of the saved user values
uint8_t compareCounter = 0;        // Create a variable used as a counter in the compare while loop
uint8_t programButtonState = 0;    // Create a variable used to determine when the programming button was pushed
int timeOutCounter = 0;            // Create a timeout counter to use in subsequent loops
boolean nextButtonState = 0;       // Create an enter button state variable
boolean enterButtonState = 0;      // Create a select button state variable
boolean nextUserFlag = 0;          // Create a user flag for programming mode function
boolean enterButtonFlag = 0;       // Create a button flag for programming mode function
/*********************************************************************************
The following user addresses identify the starting location in EEPROM where that
specific user's code will be stored. Example: Codes are 10 position int arrays, so position 
0 through 9 represent user one, 20 through 29 represent user two.
*********************************************************************************/
int users[10] = {0, 20, 40, 60, 80, 100, 120, 140, 160, 180}; 

/*******************************************************************************************************
* 
* 
*                                           SETUP SECTION
*                                
* 
********************************************************************************************************/
void setup() { 

pinMode(enablePin, OUTPUT);     // Sets the enablePin as an output (for the RFID Reader)
pinMode(rxPin, INPUT);          // Sets the rxPin as an output (for the RFID Reader)
digitalWrite(enablePin, HIGH);  // disable RFID Reader  
pinMode(13, OUTPUT);            // Set digital pin 4 as OUTPUT to be used for the locking mechanism
pinMode(3, INPUT_PULLUP);       // Set the internal pull up resistor on digital pin 3
pinMode(5, OUTPUT);             // Set digital pin 5 as an OUTPUT to be used for making tones
// To free up a couple "digital" pins on the Arduino, we will use some of the anaglog pins as digital
// To do this we have to set the Data Direction Registery for the Port C Registery 
DDRC = 0b00001100;              // Set Arduino Analog Pin 0 and 1 as a digital input and 2 and 3 as digital outputs  
//Set the inital state of the LED pins to low by clearing the associated bits in the PORTC register
PORTC &= ~(1 << GREEN_LED);     // Clears the bit in the PORTC registers associated with pin 2 on the Arduino
PORTC &= ~(1 << RED_LED);       // Clears the bit in the PORTC registers associated with pin 3 on the Arduino
lcd.begin(16,2);                // Set up the LCD's number of columns and rows

attachInterrupt(1, programButton, FALLING); // Setup the external interrupt for pin 3

Serial.begin(9600);             // setup Arduino Serial Monitor
while (!Serial);                // wait until Serial is ready

rfidSerial.begin(2400);         // set the baud rate for the SoftwareSerial port
}  
/*******************************************************************************************************
* 
* 
*                                            LOOP SECTION
*                                
* 
********************************************************************************************************/
 void loop() { 

  lcd.setCursor(0, 0);             // Set the cursor location setCursor(column, row)
  lcd.print("Ready For Input ");   // Set up the initial message   
  lcd.setCursor(0, 1);             // Set the cursor location setCursor(column, row)
  lcd.print("                ");   // Clears out the second row of the LCD

/*********************************************************************************
Initialize the reader and start waiting for a swipe, once a read has occured
the value is stored as rfidData[], then close out the reader
*********************************************************************************/     
  // Wait for a response from the RFID Reader
  // See Arduino readBytesUntil() as an alternative solution to read data from the reader
  rfidData[0] = 0;                  // Clear the buffer 
  swipeState = 0;                   // Set the state as no card has been swiped yet
  serialRecieveFlush(); //Flush the recieve data from the Serial Stream - prevents double scans
  
  digitalWrite(enablePin, LOW);           // enable the RFID Reader
  Serial.println("Ready for RFID Swipe"); // Output to serial that the reader is ready
  
//Start waiting for a swipe, this program should be in this loop 90% of the time
/*******************************************************************************************
When the RFID Reader is active and a valid RFID tag is placed with range of the reader,
the tag's unique ID will be transmitted as a 12-byte printable ASCII string to the host 
(start byte + ID + stop byte)
 
For example, for a tag with a valid ID of 0F0184F07A, the following bytes would be sent:
0x0A, 0x30, 0x46, 0x30, 0x31, 0x38, 0x34, 0x46, 0x30, 0x37, 0x41, 0x0D
We'll receive the ID and convert it to a null-terminated string with no start or stop byte.

The only thing that was changed from orginal source code is the addition of a swipeState variable, used
to determine the state during a programming event. 
*****************************************************************************************/
 PORTC |= (1 << GREEN_LED);     // When the card reader is ready to read a card, turn on the Green LED
 PORTC &= ~(1 << RED_LED);      // When the card reader is ready to read a card, turn off the Red LED
  while(1)
   {

     if (rfidSerial.available() > 0) // If there are any bytes available to read, then the RFID Reader has probably seen a valid tag
      {
        rfidData[offset] = rfidSerial.read();  // Get the byte and store it in our buffer
        if (rfidData[offset] == RFID_START)    // If we receive the start byte from the RFID Reader, then get ready to receive the tag's unique ID
        {
          offset = -1;     // Clear offset (will be incremented back to 0 at the end of the loop)
        }
        else if (rfidData[offset] == RFID_STOP)  // If we receive the stop byte from the RFID Reader, then the tag's entire unique ID has been sent
        {
          rfidData[offset] = 0; // Null terminate the string of bytes we just received
          swipeState = true;
          break;
        }
        offset++;  // Increment offset into array
        if (offset >= BUFSIZE) offset = 0; // If the incoming data string is longer than our buffer, wrap around to avoid going out-of-bounds
      }
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
  PORTC |= (1 << RED_LED);          // The reader is not ready, turn on the Red LED
  PORTC &= ~(1 << GREEN_LED);       // The reader is not ready, turn off the Green LED
  Serial.println(rfidData);         // The rfidData string should now contain the tag's unique ID with a null termination
  Serial.flush();                   // Wait for all bytes to be transmitted to the Serial Monitor
  //lcd.setCursor(0, 0);            // Set the cursor location setCursor(column, row)
  //lcd.print("Input Recieved  ");  // Sets the desired message to the display
  digitalWrite(enablePin, HIGH);    // disable RFID Reader 
  delay(500);                       // this delay haults the uC just a little before moving on to something else                  

/*********************************************************************************
 The following while loop will compare the current swiped RFID code to the ones
 saved in EEPROM. The loop will only execute a total of 10 times or until a match
 is found. When a match is found, the variable state is set to true and code breaks
 out of the loop.
*********************************************************************************/

compareCounter = 0;          // set the whileCounter back to zero for next cycle
if(swipeState == true)       // if a swipe has occured, start comparing the results
{
  while (1) 
  {                
      lcd.setCursor(0, 0);           // Set the cursor location setCursor(column, row)
      lcd.print("Comparing Tags  "); // indicate on the LCD the current state 
                             
      char readInArray[10];   // char array used to store values from EEPROM
      for (uint8_t i = 0; i < 10; i++) //loop allows each char to be read for a given user
      {
        //contents of EEPROM are read in one byte at a time
        readInArray[i] = EEPROM.read(users[compareCounter] + i); 
      } 
      Serial.println(readInArray); // indicate out to the serial the current state
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
      if(compareCounter > 9) // The compare counter would have to be adjusted if more users are added
      
      {
       break;  //When a total of 10 comparisons have been made, break out of the loop
      } 
   }
}
/*********************************************************************************  
  The following section is where we use the information obtained when comparing RFIDs
  to the values saved in EEPROM. 
*********************************************************************************/    
  if (swipeState == true) // If a swipe has occured, do something with the results of - match -
{
  if (match == true)                     // When a match is found do this
  {
    digitalWrite(13, HIGH);              // if a match is found, set pin 3 to HIGH
    lcd.setCursor(0, 0);                 // Set the cursor location setCursor(column, row)
    lcd.print("RFID Match Found");       // indicate on the LCD that a match was found
    Serial.println("A match was found"); // Tell the serial interface a match was found
    PORTC &= ~(1 << RED_LED);            // Turn off the red LED, long enough to toggle the green ones three times
   for ( uint8_t i = 0; i < 6; i++)      // Toggle the Green LEDs three times
   {
     PORTC ^= (1 << GREEN_LED);          // Bitwise for toggle
     delay(200);                         // Allow for a small delay, for human reaction time to see the blinking
   }
   PORTC |= (1 << RED_LED);              // Turn the Red LED back on, card reader is not ready for a swipe
    delay(RELAY_TIME);                   //This is a termporary delay to allow the user to see pin 13 light up
    digitalWrite(13, LOW);               //turn the pin 13 LED off
  }
  else                                   // When no match is found do this
  {
   lcd.setCursor(0, 0);                  // Set the cursor location setCursor(column, row)
   lcd.print("No RFID Match   ");        // indicate on the LCD that no match found
   Serial.println("No match was found"); // Tell the serial interface no match found
       
   for ( uint8_t i = 0; i < 6; i++)      // Toggle the Red LEDs a few times
   {
     PORTC ^= (1 << RED_LED);            // Bitwise for toggle
     delay(200);                         // Allow for a small delay, for human reaction time to see the blinking
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
 * serves as the Interrupt Service Routine and sets the programButtonState variable to 1. This can be used to trigger 
 * putting the device in programming mode.
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
 while(rfidSerial.available()) //flushes out rfidSerial data on the incoming path
  rfidSerial.read(); 
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
       break;                              // break out of this while loop
      }
       if ((PINC & (1 << ENTER_BUTTON)) == 0) //if the Enter button is pressed, do the following
       {
         nextUserFlag = 0;                    // We don't want to be in the select user portion anymore
         enterButtonFlag = 1;                 // flags that a user was selected, used to exit the user selection while loop
         selectedUser = user;                 // sets the variable -slectedUser- to the currently selected user                
         delay(200);                          // Just slows down uC for humans   
         break;                               // break out of this while loop
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
 * Description: This will put the Arduino in a so called Programming Mode so that a new
 * RFID code can be programmed to EEPROM.
 *******************************************************************************************************/
void programmingMode(void)
{
  programButtonState = 0; // At the conclusion of programming mode function, set the state back to 0
  digitalWrite(enablePin, HIGH);              // deactivate the RFID reader
  lcd.setCursor(0, 0);                        // Set the cursor location setCursor(column, row)
  lcd.print("Programming Mode");              // indicate on the LCD that it is in programming mode
  lcd.setCursor(0, 1);                        // Set the cursor location setCursor(column, row)
  lcd.print("Enter to Cont.  ");              // Set the LCD to press enter to continue
  timeOutCounter = 0;                         // Set the time out counter to zero
  enterButtonFlag = 0;                        // Set initial state of the enter button flag (used for exiting the while loop

 PORTC |= (1 << RED_LED);                     // Turn on the Red LED, reader is not ready
 PORTC &= ~(1 << GREEN_LED);                  // Turn off the Greed LED, reader is not ready
 
  //The following loop provides the user selection interface during a programming event
  while (1)                                   // Select User Loop
  {    
      Serial.println(timeOutCounter);         // Send the timeout counter value to serial
       if ((PINC & (1 << ENTER_BUTTON)) == 0) // If enter button is pressed, continue with selecting a user 
       {
         selectedUser = 1;                    // Sends the program to the first user in the following SWITCH statement
         nextUserFlag = 1;                    // keeps us in the select user portion of the programming mode
       }
         
       if (enterButtonFlag == 1)              // when the enter button flag is true or 1, break out of the select user loop
       {
        break;
       }                                     
       if (nextUserFlag == 1)                 // This the user selection portion of the programming mode
       {                
           switch (selectedUser)              // This will allow the users to be cycled through based on the the selectedUser variable
           {
                case 1:                       
                    lcd.setCursor(0, 0);               // Set the inital LCD Screen state for the user
                    lcd.print("User 1 Press -  ");     // Sets the message on the LCD
                    lcd.setCursor(0, 1);               // Set the cursor location to the bottom left position
                    lcd.print("ENTER or NEXT   ");     // Prompt for either the Enter button or the Next button
                    delay(500);                        // Slow the uC down to prevent it jumping directly to the next state 
                    selectUserInterface(selectedUser); // Do something based on which button is pressed, function is defined earlier
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
                  break;                                
              }
            }        
    timeOutCounter++;                   // Increment the time out counter
    if (timeOutCounter > TIMEOUT)       // When the counter reaches the defined (at the top) value, break out of loop
     {
      break; 
     }
   }

  delay(50); // this delay prevents the program from jumping into the following if statement unintentially
  if (selectedUser > 0)   // If the selectedUser is 0, then skip asking the user to swipe a card
  {
  /*********************************************************************************  
     Now that a user is selected, we prompt for the new RFID card to be swiped
  *********************************************************************************/  
  lcd.setCursor(0, 0);                   // Set the LCD up to prompt the user to swipe a card
  lcd.print("Programming Mode");
  lcd.setCursor(0, 1);
  lcd.print("Swipe RFID Card ");
  
  PORTC |= (1 << GREEN_LED);             // Turn on the Green LED, the reader is ready for a card
  PORTC &= ~(1 << RED_LED);              // Turn off the Red LED, the reader is ready for a card
  
  rfidData[0] = 0;         // Clear the buffer 
  serialRecieveFlush(); //Flush the recieve data from the Serial Stream - prevents double scans
  digitalWrite(enablePin, LOW);   // enable the RFID Reader
  //Serial.println("Ready for RFID"); //Output to serial that the reader is ready
  timeOutCounter = 0;
  swipeState = 0;
 
  serialRecieveFlush(); //Flush the recieve data from the Serial Stream - prevents double scans
  
  while(1) //Start waiting for a swipe, limit the time it waits by xxxx cycles
  {
    if (rfidSerial.available() > 0) // If there are any bytes available to read, then the RFID Reader has probably seen a valid tag
      {
        rfidData[offset] = rfidSerial.read();  // Get the byte and store it in our buffer
        if (rfidData[offset] == RFID_START)    // If we receive the start byte from the RFID Reader, then get ready to receive the tag's unique ID
        {
          offset = -1;     // Clear offset (will be incremented back to 0 at the end of the loop)
        }
        else if (rfidData[offset] == RFID_STOP)  // If we receive the stop byte from the RFID Reader, then the tag's entire unique ID has been sent
        {
          rfidData[offset] = 0; // Null terminate the string of bytes we just received
          swipeState = 1;
          break;
        }
        offset++;  // Increment offset into array
        if (offset >= BUFSIZE) offset = 0; // If the incoming data string is longer than our buffer, wrap around to avoid going out-of-bounds
      }
      
     timeOutCounter++;                          // Increment the timeout counter
     Serial.println(timeOutCounter);            // Send the value of the counte to serial, for testing purposes
     if (timeOutCounter > TIMEOUT)              // When the counter reaches the defined (at the top) value, break out of loop
     {
      break; 
     }
  }
  PORTC |= (1 << RED_LED);                      // Turn on the Red LED, the reader is not ready
  PORTC &= ~(1 << GREEN_LED);                   // Turn off the Green LED, the reader is not ready
  //Serial.println(rfidData);                   // The rfidData string should now contain the tag's unique ID with a null termination
  //Serial.flush();                             // Wait for all bytes to be transmitted to the Serial Monitor
  digitalWrite(enablePin, HIGH);                // disable RFID Reader 
  delay(500);                                   // this delay haults the uC before moving on to something else  
  
  if (swipeState == 1)                          // If a swipe did occur, write the value to EEPROM at the selectedUser position in memory
  {
     lcd.setCursor(0, 0);                       // Set the cursor location setCursor(column, row)
     lcd.print("Input Recieved  ");             // Sets the desired message to the display
     uint8_t userAddress = selectedUser - 1;    // The selectedUser will be one value higher then it's corrisponding position within the users[] array
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
 

  





