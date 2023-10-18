
#include <EEPROM.h>
#include <Servo.h>
#include <Bounce2.h>
#include <avr/sleep.h>

#define debug           0 //If 1 then debug info will be printed to the serial console
#define resetEEPROM     0 //If 1 the eeprom contents will be reset each cold start

#define gears           10  //Interger number of gears on cassette
#define gearMin         45  //Degrees
#define gearMax         120 //Degrees

#define overShootUp     (gearMax-gearMin)/(gears-1) //Up the cassete to i.e. little to big
#define overShootDown   (gearMax-gearMin)/(gears-1) //Down the cassette i.e. big to little
#define overShootPeriod 500                         //Mili Seconds

#define pwmPin          9 //D9

#define ISRPin          2 //D2
#define gearUpPin       4 //D4
#define gearDownPin     5 //D5
#define programPin      6 //D6

#define debounce        5 //ms = milli seconds

//volatile byte btnPrs = LOW;
Servo myservo;            // create servo object to control a servo
byte currGear;            //a byte for storing the current gear 
byte gearLoc[gears];      //An array to store the posistions for the gears
bool progMode = false;    //Are we in program mode?
unsigned long overShootTimer = 0;

// INSTANTIATE A Button OBJECT
Bounce2::Button gearUp = Bounce2::Button();   //Gear UP button
Bounce2::Button gearDown = Bounce2::Button(); //Gear DOWN button
Bounce2::Button program = Bounce2::Button();  //Program button

void setup() {
  if (resetEEPROM) EEPROM.write(0,bool(0));//If the code is set to reset the EEPROM each run then do that

  //Setup GearUP button
  //gearUp.attach(gearUpPin ,  INPUT_PULLUP );    // USE INTERNAL PULL-UP
  gearUp.attach(gearUpPin ,  INPUT);    // USE External pull down 
  gearUp.interval(debounce);                    // DEBOUNCE INTERVAL IN MILLISECONDS
  gearUp.setPressedState(HIGH);                  // INDICATE THAT THE LOW STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON 

  //Setup GearUP button
  gearDown.attach(gearDownPin ,  INPUT); // USE External pull down 
  gearDown.interval(debounce);                  // DEBOUNCE INTERVAL IN MILLISECONDS
  gearDown.setPressedState(HIGH);                // INDICATE THAT THE LOW STATE CORRESPONDS TO PHYSICALLY PRESSING THE BUTTON 

  program.attach(programPin, INPUT_PULLUP );
  program.interval(debounce);
  program.setPressedState(LOW);
  
  myservo.attach(pwmPin);
  
  if (debug) Serial.begin(9600);
  //EEPROM Layout
  //byte 0 = 1 if there is an array, zero if it needs to be init
  //byte 1 = The current gear
  //byte 2-N = The PWM value for each gear

  //Is the EEPROM alredy configured?
  if (EEPROM.read(0) == 0)//No its not
  {
    if (debug) Serial.println("Configuring EEPROM");
    EEPROM.write(0,01);    //EEPROM is configured
    EEPROM.write(1,0);    //
    for(int i=0; i < gears; i++){ EEPROM.write(i+2, gearMin + (((gearMax-gearMin)/(gears-1))*i));}
  }

  //Read the settings in from the EEPROM and set servo acordingly
  currGear = EEPROM.read(1);
  for(int i=0; i < gears; i++) {gearLoc[i] = EEPROM.read(i+2);}
  myservo.write(gearLoc[currGear]);
  
  if (debug)
  {
    //print the settings to the screen
    Serial.print("EEPROM Status ");
    Serial.println(int(EEPROM.read(0)));;
    Serial.print("Current gear ");  
    Serial.println(currGear);
    for(int i=0; i < gears; i++) 
    {
      Serial.print(gearLoc[i]);
      Serial.print(",");
    }
    Serial.println();
  }
  pinMode(LED_BUILTIN,OUTPUT);
  //pinMode(ISRPin,INPUT_PULLUP);
  digitalWrite(LED_BUILTIN,LOW);
  
}
/*
void wakeup()
{
  if(debug) Serial.println("Interupt");
  sleep_disable();
  detachInterrupt(0);
}

void goToSleep()
{
  if(debug) Serial.println("Going to sleep");
  sleep_enable();
  attachInterrupt(0, wakeup, LOW);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  digitalWrite(LED_BUILTIN,LOW);
  delay(1000);
  sleep_cpu();
  if(debug) Serial.println("Just woke up");
  digitalWrite(LED_BUILTIN,HIGH);
}*/

void loop() {
  // UPDATE THE BUTTON
  // YOU MUST CALL THIS EVERY LOOP
  gearUp.update();
  gearDown.update();
  program.update();

  //Serial.println(gearUp.pressed());
  //Serial.println(gearDown.pressed());

  /*if (gearDown.isPressed() && gearUp.isPressed())
  {
    delay(2000);
    if (gearDown.isPressed() && gearUp.isPressed()) 
    {
      progMode = !progMode;
      if ( progMode ) digitalWrite(LED_BUILTIN,HIGH);
      else digitalWrite(LED_BUILTIN,LOW);
      if (debug) Serial.println("Prog Mode");
    }
  }*/

  if (program.isPressed() )
  {
    progMode = true;
    digitalWrite(LED_BUILTIN,HIGH);
    // if (debug) Serial.println("Prog Mode");
  }else
  {
    progMode = false;
    digitalWrite(LED_BUILTIN,LOW);
  }
  //if in prog mode tweak the gear pos
  if ( gearUp.pressed() && progMode)
  {
    if (debug) Serial.println(currGear);
    gearLoc[currGear]++;
    if (gearLoc[currGear] >= 180 ) gearLoc[currGear] = 180;
    myservo.write(gearLoc[currGear]);
    EEPROM.write(currGear+2,gearLoc[currGear]);
    if (debug) Serial.println(gearLoc[currGear]);
  }
  if ( gearDown.pressed() && progMode)
  {
    if (debug) Serial.println(currGear);
    gearLoc[currGear]--;
    if (gearLoc[currGear] == 255 ) gearLoc[currGear] = 0;
    myservo.write(gearLoc[currGear]);
    EEPROM.write(currGear+2,gearLoc[currGear]);
    if (debug) Serial.println(gearLoc[currGear]);
  }
  
  if ( gearUp.pressed() && !progMode)//&& !gearDown.pressed())
  {
    overShootTimer = millis();
    currGear++;
    if(currGear >= gears) currGear = gears-1;
    if (debug) Serial.println(currGear);
    myservo.write(gearLoc[currGear]+overShootUp);
    EEPROM.write(1,currGear); 
  }
  if ( gearDown.pressed() && !progMode)//&& !gearUp.pressed())
  {
    overShootTimer = millis();
    currGear--;
    if(currGear == 255) currGear = 0;
    if (debug) Serial.println(currGear);
    myservo.write(gearLoc[currGear]-overShootDown);
    EEPROM.write(1,currGear); 
  }
  if (overShootTimer > 0 && millis() - overShootTimer > overShootPeriod ) 
  {
    overShootTimer = 0;
    myservo.write(gearLoc[currGear]);
  }
  //delay(10);
}
