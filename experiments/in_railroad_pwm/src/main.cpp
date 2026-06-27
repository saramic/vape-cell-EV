#include <Arduino.h>

// Inertia simulation
int targetSpeed = 0;    // where you want to be (0-255)
int currentSpeed = 0;   // where the PWM actually is (0-255)

// Inertia constants — tune these for feel
const int ACCEL_RATE = 2;   // PWM steps per update when accelerating
const int BRAKE_RATE = 3;   // PWM steps per update when braking
const int UPDATE_MS  = 30;  // how often to update (ms)

void setup() {
  pinMode(A0, INPUT);   // Throttle potentiometer
  pinMode(9, OUTPUT);   // D9 PWM to MOSFET gate
}

void loop() {
  // Read throttle — potentiometer on A0
  targetSpeed = map(analogRead(A0), 0, 1023, 0, 255);

  // Chase target with inertia
  if (currentSpeed < targetSpeed) {
    currentSpeed = min(currentSpeed + ACCEL_RATE, targetSpeed);
  } else if (currentSpeed > targetSpeed) {
    currentSpeed = max(currentSpeed - BRAKE_RATE, targetSpeed);
  }

  analogWrite(9, currentSpeed);  // D9 PWM to MOSFET gate
  delay(UPDATE_MS);
}