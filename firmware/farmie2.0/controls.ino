
void turnOnFan() {
  // Code to turn on the fan
  ledcWrite(fanPwmChannel, 255);
  digitalWrite(output,HIGH);
    
  Serial.println("Fan turned on");
}

void turnOffFan() {
  // Code to turn off the fan
    ledcWrite(fanPwmChannel, 0); // Turn off the fan
  digitalWrite(output,LOW);
  Serial.println("Fan turned off");
  
}