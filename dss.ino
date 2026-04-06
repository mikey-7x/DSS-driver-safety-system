#include <Servo.h>

#define MOTOR_PIN 2      // PB2 (PWM capable for speed control)
#define SERVO_PIN 3      // PB3
#define PROX_PIN 10      // PD2
#define BUZZER_PIN 11    // PD3
#define BUTTON_PIN 14    // PD6

enum SystemState { SYSTEM_OFF, FULL_MANUAL, ADAS_MONITORING, AUTO_DRIVING, AWAIT_STOPPED, AWAIT_RUNNING };
SystemState currentState = SYSTEM_OFF;

char driverEyeStatus = 'O'; 
int currentSpeed = 0;       // 0 to 255 for PWM motor control
int currentSteering = 90;   
bool guiTakeover = false;   

Servo steeringServo;

void setup() {
  Serial.begin(9600);
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PROX_PIN, INPUT); 
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  steeringServo.attach(SERVO_PIN);
  steeringServo.write(90); 
}

void loop() {
  // 1. Process GUI Commands
  while (Serial.available() > 0) {
    char incoming = Serial.read();
    
    if (incoming == 'O' || incoming == 'C') driverEyeStatus = incoming;
    else if (incoming == 'P') currentState = (currentState == SYSTEM_OFF) ? ADAS_MONITORING : SYSTEM_OFF; // Power Button
    else if (incoming == 'M') currentState = FULL_MANUAL; // Master Override
    else if (incoming == 'A') currentState = ADAS_MONITORING; // Return to Smart ADAS
    else if (incoming == '+') { currentSpeed += 50; if (currentSpeed > 255) currentSpeed = 255; } // Gas Pedal
    else if (incoming == '-') { currentSpeed -= 50; if (currentSpeed < 0) currentSpeed = 0; }     // Brake Pedal
    else if (incoming == 'B') currentSpeed = 0; // Hard Brake
    else if (incoming == '<') currentSteering = 45;      
    else if (incoming == '>') currentSteering = 135;     
    else if (incoming == '^') currentSteering = 90;      
    else if (incoming == 'T') guiTakeover = true;        
  }

  bool trafficClear = (digitalRead(PROX_PIN) == HIGH); 
  bool physicalButtonPressed = (digitalRead(BUTTON_PIN) == LOW); 
  bool triggerTakeover = physicalButtonPressed || guiTakeover;
  guiTakeover = false; 

  // ==========================================
  // COCKPIT STATE MACHINE
  // ==========================================
  
  if (currentState == SYSTEM_OFF) {
    analogWrite(MOTOR_PIN, 0);
    digitalWrite(BUZZER_PIN, LOW);
    steeringServo.write(90);
  }
  else if (currentState == FULL_MANUAL) {
    // 100% Driver Control. AI is ignored.
    analogWrite(MOTOR_PIN, currentSpeed);
    digitalWrite(BUZZER_PIN, LOW);
    steeringServo.write(currentSteering);
  }
  else if (currentState == ADAS_MONITORING) {
    // Standard Driving. AI is watching.
    if (driverEyeStatus == 'C') {
      currentState = AUTO_DRIVING; // Driver asleep! AI takes the wheel.
    } else {
      analogWrite(MOTOR_PIN, currentSpeed);
      digitalWrite(BUZZER_PIN, LOW);
      steeringServo.write(currentSteering);
    }
  } 
  else if (currentState == AUTO_DRIVING) {
    if (driverEyeStatus == 'O') {
      // Driver woke up! 
      if (trafficClear) currentState = AWAIT_RUNNING;
      else currentState = AWAIT_STOPPED; 
    } else {
      // Eyes closed: AI Control Mode
      steeringServo.write(90); 
      if (trafficClear) {
        analogWrite(MOTOR_PIN, 150); // Safe cruise speed
        digitalWrite(BUZZER_PIN, HIGH); // Alarm to wake driver
      } else {
        analogWrite(MOTOR_PIN, 0);  // Obstacle! Emergency Brake.
        digitalWrite(BUZZER_PIN, HIGH); 
      }
    }
  } 
  else if (currentState == AWAIT_STOPPED) {
    analogWrite(MOTOR_PIN, 0); // Keep car stopped
    digitalWrite(BUZZER_PIN, LOW);
    if (triggerTakeover) currentState = ADAS_MONITORING; 
  }
  else if (currentState == AWAIT_RUNNING) {
    steeringServo.write(90);
    analogWrite(MOTOR_PIN, 150); // Keep driving safely
    digitalWrite(BUZZER_PIN, LOW);
    if (triggerTakeover) currentState = ADAS_MONITORING; 
  }

  // ==========================================
  // SEND TELEMETRY TO DASHBOARD
  // ==========================================
  Serial.print("STATE:");
  if (currentState == SYSTEM_OFF) Serial.print("OFF");
  else if (currentState == FULL_MANUAL) Serial.print("FULL_MANUAL");
  else if (currentState == ADAS_MONITORING) Serial.print("ADAS_ACTIVE");
  else if (currentState == AUTO_DRIVING) Serial.print("AI_TAKEOVER");
  else if (currentState == AWAIT_STOPPED) Serial.print("AWAIT_STOPPED");
  else if (currentState == AWAIT_RUNNING) Serial.print("AWAIT_RUNNING");

  Serial.print(",PROX:");
  Serial.print(trafficClear ? "CLEAR" : "OBSTACLE");

  Serial.print(",SPEED:");
  Serial.print(currentSpeed);

  Serial.print(",STEER:");
  int actualSteer = steeringServo.read();
  if (actualSteer < 80) Serial.println("LEFT");
  else if (actualSteer > 100) Serial.println("RIGHT");
  else Serial.println("CENTER");

  delay(50); 
}