# a07g-exploring-the-CLI

* Team Number: 27
* Team Name: Pocket Pet
* Team Members: Kelly LAI, Yuyan WANG
* GitHub Repository URL: [final-project-a07g-a14g-t27-pocket-pet/A07G_README.md at main · ese5160/final-project-a07g-a14g-t27-pocket-pet](https://github.com/ese5160/final-project-a07g-a14g-t27-pocket-pet/blob/main/A07G_README.md)
* Description of test hardware: (development boards, sensors, actuators, laptop + OS, etc)

## **1. Software Architecture**

#### 1.1 Updated HRS & SRS

#### 1.2 Block diagram

#### 1.3 Flowcharts or state machine diagrams


## **2. Understanding the Starter Code**

* What does “InitializeSerialConsole()” do? In said function, what is “cbufRx” and “cbufTx”? What type of data structure is it?
* How are “cbufRx” and “cbufTx” initialized? Where is the library that defines them (please list the *C file they come from).

* Where are the character arrays where the RX and TX characters are being stored at the end? Please mention their name and size.

Tip: Please note cBufRx and cBufTx are structures. 

* Where are the interrupts for UART character received and UART character sent defined?
* What are the callback functions that are called when:

A character is received? (RX) 

A character has been sent? (TX) 

* Explain what is being done on each of these two callbacks and how they relate to the cbufRx and cbufTx buffers.
* Draw a diagram that explains the program flow for UART receive – starting with the user typing a character and ending with how that characters ends up in the circular buffer “cbufRx”. Please make reference to specific functions in the starter code.

* Draw a diagram that explains the program flow for the UART transmission – starting from a string added by the program to the circular buffer “cbufTx” and ending on characters being shown on the screen of a PC (On Teraterm, for example). Please make reference to specific functions in the starter code.
* What is done on the function “startStasks()” in main.c? How many threads are started?
