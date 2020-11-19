 #include <SoftwareSerial.h> //Software Serial Port
#include <Servo.h>

#define RxD 7
#define TxD 6
#define ConnStatus A1

#define DEBUG_ENABLED  1

int shieldPairNumber = 5;

// CAUTION: If ConnStatusSupported = true you MUST NOT use pin A1 otherwise "random" reboots will occur
// CAUTION: If ConnStatusSupported = true you MUST set the PIO[1] switch to A1 (not NC)

boolean ConnStatusSupported = true; // Set to "true" when digital connection status is available on Arduino pin

// The following two string variable are used to simplify adaptation of code to different shield pairs

String slaveNameCmd = "\r\n+STNA=Slave"; // This is concatenated with shieldPairNumber later
String recvBuff = "";
boolean autoMode = false; // F = manual, T = auto
int turningMode = 0; // 0 = forward, 1 = turning left, 2 = turning right, 3 = backward, 4 = stop

SoftwareSerial blueToothSerial(RxD,TxD);

Servo servoLeft;
Servo servoRight;
Servo cupServo;

int deg90 = 1000; // Didn't have time to properly calibrate
int smallDeg = deg90 / 3; // Didn't have time to properly calibrate
int reversingDelay = 100; // Didn't have time to properly calibrate
boolean buttonPressed = false;
byte wRight;
byte wLeft;

void setup() {
    Serial.begin(9600);
    blueToothSerial.begin(38400); // Set Bluetooth module to default baud rate 38400
    
    pinMode(RxD, INPUT);
    pinMode(TxD, OUTPUT);
    pinMode(ConnStatus, INPUT);
    pinMode(3, OUTPUT); // Set buzzer pin to output
    pinMode(5, INPUT); // Set front right whisker pin to input
    pinMode(8, INPUT); // Set front left whisker pin to input
    pinMode(9, OUTPUT); pinMode(10, INPUT);  // Set left IR LED & Receiver pins to input and output respectively

    //  Check whether Master and Slave are already connected by polling the ConnStatus pin (A1 on SeeedStudio v1 shield)
    //  This prevents running the full connection setup routine if not necessary.

    if(ConnStatusSupported) Serial.println("Checking Slave-Master connection status.");

    if(ConnStatusSupported && digitalRead(ConnStatus)==1) {
        Serial.println("Already connected to Master - remove USB cable if reboot of Master Bluetooth required.");
    } else {
        Serial.println("Not connected to Master.");
        setupBlueToothConnection(); // Set up the local (slave) Bluetooth module
        
        // Wait one second and flush the serial buffers
        delay(1000); 
        Serial.flush();
        blueToothSerial.flush();
    }
    servoLeft.attach(11);
    servoRight.attach(12);
    cupServo.attach(13);
}

void loop() {
    
    char recvChar;
    
    if(blueToothSerial.available()) {  // Check if there's any data sent from the remote Bluetooth shield
        recvChar = blueToothSerial.read();
        recvBuff += recvChar;
        Serial.print(recvChar);
        if (recvBuff.endsWith("servo") || buttonPressed == true) {
          autoMode = false;
          if (buttonPressed == false) {
            buttonPressed = true;
            servoLeft.writeMicroseconds(1600); // Meant to be slower speeds, couldn't fix it in time though.
            servoRight.writeMicroseconds(1400);
          } else {
            servoLeft.writeMicroseconds(1500);
            servoRight.writeMicroseconds(1500);
            buttonPressed = false;
            turnServo(true);
            delay(2000);
            blueToothSerial.print("\nPing pong ball retrieved!\n");
            tone(3, 1000, 2000);
            autoMode = true;
          }
        } else if (recvBuff.endsWith("auto")) {
          autoMode = true;
          turningMode = 0;
          recvBuff = "";
          align();
        } else if (recvBuff.endsWith("manual")) {
          autoMode = false;
          turningMode = 0;
          recvBuff = "";
        } else if (recvBuff.endsWith("left")) {
          autoMode = false;
          turningMode = 1;
          recvBuff = "";
        } else if (recvBuff.endsWith("right")) {
          autoMode = false;
          turningMode = 2;
          recvBuff = "";
        } else if (recvBuff.endsWith("forward")) {
          autoMode = false;
          turningMode = 0;
          recvBuff = "";
        } else if (recvBuff.endsWith("back")) {
          autoMode = false;
          turningMode = 3;
          recvBuff = "";
        } else if (recvBuff.endsWith("STOP")) {
          autoMode = false;
          turningMode = 4;
          recvBuff = "";
        }
    }

    if(Serial.available()) { // Check if there's any data sent from the local serial terminal.
        recvChar = Serial.read();
        Serial.print(recvChar);
        blueToothSerial.print(recvChar);
    }

    if (autoMode) { // Automatic movement
      autoDrive();
    } else {
        if (turningMode == 0) { // Moving forward
        servoLeft.writeMicroseconds(1700);
        servoRight.writeMicroseconds(1300);
      } else if (turningMode == 3) { // Moving back
        servoLeft.writeMicroseconds(1300);
        servoRight.writeMicroseconds(1700);
      } else if (turningMode == 1) { // Turning left
        servoLeft.writeMicroseconds(1300);
        servoRight.writeMicroseconds(1300);
      } else if (turningMode == 2) { // Turning right
        servoLeft.writeMicroseconds(1500);
        servoRight.writeMicroseconds(1700);
      } else if (turningMode == 4) { // Stop
        servoLeft.writeMicroseconds(1500);
        servoRight.writeMicroseconds(1500);
      }
    }
}

// To be called every tick of loop. 
// A left whisker trigger is when wLeft == 0, else is 1
// A right whisker trigger is when wRight == 0, else is 1
void autoDrive() {
  wLeft = digitalRead(8);
  wRight = digitalRead(5);
  
  // If the bot approaches walls in general (e.g. alignment didn't work out), do small turns to put it back on track
  if (wLeft == 0 && wRight == 1) { // Only left triggers
    // Reverse a bit
    servoLeft.writeMicroseconds(1300);
    servoRight.writeMicroseconds(1700);
    delay(reversingDelay);

    // Rotate right a little
    servoLeft.writeMicroseconds(1500); // They are not both 1700 as couldn't get it working that way somehow
    servoRight.writeMicroseconds(1700);
    delay(smallDeg);
  } else if (wRight == 0 && wLeft == 1) { // Only right triggers
    // Reverse a bit
    servoLeft.writeMicroseconds(1300);
    servoRight.writeMicroseconds(1700);
    delay(reversingDelay);

    // Rotate left a little
    servoLeft.writeMicroseconds(1300);
    servoRight.writeMicroseconds(1300);
    delay(smallDeg);
  }

  wLeft = digitalRead(8);
  wRight = digitalRead(5);
  
  // If we hit a wall (set to OR in case one whisker hits before the other)
  if (wLeft == 0 || wRight == 0) {
    // Reverse a bit
    servoLeft.writeMicroseconds(1300);
    servoRight.writeMicroseconds(1700);
    delay(8 * reversingDelay);

    // Start rotating left bit by bit, and shimmy back and forth; 
    // if no whisker hits, proceed, else keep shimmying. However, if 90 degrees is
    // hit, turn back to facing forward and try other way (dont want to loop back where we came)
    boolean successLeft = false;
    
    for (int i = 0; i < deg90; i += smallDeg) {
      // Do the rotation
      servoLeft.writeMicroseconds(1300);
      servoRight.writeMicroseconds(1300);
      delay(2 * smallDeg);

      // Shimmy forward a little
      servoLeft.writeMicroseconds(1700);
      servoRight.writeMicroseconds(1300);
      delay(reversingDelay);

      // Stop it, in case processing delay
      servoLeft.writeMicroseconds(1500);
      servoRight.writeMicroseconds(1500);

      // Read values to check how its doing
      wLeft = digitalRead(8);
      wRight = digitalRead(5);
      
      if (wLeft == 1 && wRight == 1) {
        successLeft = true;
        break;
      } else { // If we aren't clear yet
        // Reverse back
        servoLeft.writeMicroseconds(1300);
        servoRight.writeMicroseconds(1700);
        delay(8 * reversingDelay);

        servoLeft.writeMicroseconds(1500);
        servoRight.writeMicroseconds(1500);
      }
    }

    if (successLeft) {
      // Proceed forward
      servoLeft.writeMicroseconds(1700);
      servoRight.writeMicroseconds(1300);
    } else {
      // Return it to straight position, i.e. go 90 degrees right
      servoLeft.writeMicroseconds(1500);
      servoRight.writeMicroseconds(1700);
      delay(deg90);

      // Same thing as before, but rotating right
      boolean successRight = false;
    
      for (int i = 0; i < deg90; i += smallDeg) {
        // Do the rotation
        servoLeft.writeMicroseconds(1500);
        servoRight.writeMicroseconds(1700);
        delay(smallDeg);

        // Shimmy forward a little
        servoLeft.writeMicroseconds(1700);
        servoRight.writeMicroseconds(1300);
        delay(reversingDelay);

        // Stop it, in case processing delay
        servoLeft.writeMicroseconds(1500);
        servoRight.writeMicroseconds(1500);

        // Read values to check how its doing
        wLeft = digitalRead(8);
        wRight = digitalRead(5);
      
        if (wLeft == 1 && wRight == 1) {
          successRight = true;
          break;
        } else { // If we aren't clear yet
          // Reverse back
          servoLeft.writeMicroseconds(1300);
          servoRight.writeMicroseconds(1700);
          delay(8 * reversingDelay);

          servoLeft.writeMicroseconds(1500);
          servoRight.writeMicroseconds(1500);
        }
      }

      if (successRight) {
        // Proceed forward
        servoLeft.writeMicroseconds(1700);
        servoRight.writeMicroseconds(1300);
      } else { // If it failed both directions, just stop, its probably a dead end
        servoLeft.writeMicroseconds(1500);
        servoRight.writeMicroseconds(1500);
      }
    }
  } else { // Else the way is clear
    // Proceed forward
    servoLeft.writeMicroseconds(1700);
    servoRight.writeMicroseconds(1300);
  }
}

// Didn't have time to implement this properly
void align() {
  // First shimmy forward a little
  servoLeft.writeMicroseconds(1700);
  servoRight.writeMicroseconds(1300);
  delay(reversingDelay);
  
  while (true) {
    wLeft = digitalRead(8);
    wRight = digitalRead(5);

    if (wLeft == 1 && wRight == 1) {
      break; // If no whisker hits, assume we are aligned
    } else { // Back up and decide what to do
      servoLeft.writeMicroseconds(1300);
      servoRight.writeMicroseconds(1700);
      delay(8 * reversingDelay);
      
      if (wLeft == 0) {
        // Rotate right
        servoLeft.writeMicroseconds(1500);
        servoRight.writeMicroseconds(1700);
        delay(smallDeg);
      } else if (wRight == 0) {
        // Rotate Left
        servoLeft.writeMicroseconds(1300);
        servoRight.writeMicroseconds(1300);
        delay(smallDeg);
      }
    }

    // Shimmy forward again for next read
    servoLeft.writeMicroseconds(1700);
    servoRight.writeMicroseconds(1300);
    delay(reversingDelay);
  }
}

void turnServo(boolean mode) {
  if (mode == true){
    cupServo.write(-50);
  } else {
    cupServo.write(0);
  }
}

void setupBlueToothConnection() {
    Serial.println("Setting up the local (slave) Bluetooth module.");

    slaveNameCmd += shieldPairNumber;
    slaveNameCmd += "\r\n";

    blueToothSerial.print("\r\n+STWMOD=0\r\n");      // Set the Bluetooth to work in slave mode
    blueToothSerial.print(slaveNameCmd);             // Set the Bluetooth name using slaveNameCmd
    blueToothSerial.print("\r\n+STAUTO=0\r\n");      // Auto-connection should be forbidden here
    blueToothSerial.print("\r\n+STOAUT=1\r\n");      // Permit paired device to connect me
    
    //  print() sets up a transmit/outgoing buffer for the string which is then transmitted via interrupts one character at a time.
    //  This allows the program to keep running, with the transmitting happening in the background.
    //  Serial.flush() does not empty this buffer, instead it pauses the program until all Serial.print()ing is done.
    //  This is useful if there is critical timing mixed in with Serial.print()s.
    //  To clear an "incoming" serial buffer, use while(Serial.available()){Serial.read();}

    blueToothSerial.flush();
    delay(2000);                                     // This delay is required

    blueToothSerial.print("\r\n+INQ=1\r\n");         // Make the slave Bluetooth inquirable
    
    blueToothSerial.flush();
    delay(2000);                                     // This delay is required
    
    Serial.println("The slave bluetooth is inquirable!");
}
