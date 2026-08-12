// Define the input pins
const int pinA3 = A3;
const int pinA6 = A6;
const int pinA7 = A7;

void setup() {
  // Start serial communication at 9600 baud rate
  Serial.begin(9600);
  
  // A3 can be explicitly set as input
  pinMode(pinA3, INPUT); 
  
  // Note: pinMode() is not needed for A6 and A7 
  // because they are hardware-restricted to input-only.
}

void loop() {
  // Read raw voltage values (returns 0 to 1023)
  int valueA3 = analogRead(pinA3);
  int valueA6 = analogRead(pinA6);
  int valueA7 = analogRead(pinA7);

  // Print results to the Serial Monitor
  Serial.print("A3: ");
  Serial.print(valueA3);
  
  Serial.print(" | A6: ");
  Serial.print(valueA6);
  
  Serial.print(" | A7: ");
  Serial.println(valueA7);

  // Wait 500 milliseconds before the next read
  delay(2000);
}
