
void turnOnFan() {
  // Code to turn on the fan
  // ledcWrite(fanPwmChannel, 255);
  digitalWrite(output,HIGH);
    isFanOn = true;
  // Serial.println("Fan turned on");
}

void turnOffFan() {
  // Code to turn off the fan
    ledcWrite(fanPwmChannel, 0); // Turn off the fan
  digitalWrite(output,LOW);
  // Serial.println("Fan turned off");
  isFanOn = false;
  
}
void turnOnLight(){
  digitalWrite(ledPin,HIGH);
  // Serial.println("Lights turned on");
    areLightsOn = true;

}
void turnOffLight(){
    digitalWrite(ledPin,LOW);
  // Serial.println("Lights turned off");
  areLightsOn = false;
}
void turnOnFertilizerPump(){
  digitalWrite(fertilizerPumpPin,HIGH);
  isFertilizerPumpOn=true;
  // Serial.println("Fertilizer pump turned on");
  Alarm.timerOnce(15, turnOffFertilizerPump);    

}
void turnOffFertilizerPump(){

    digitalWrite(fertilizerPumpPin,LOW);
  // Serial.println("Fertilizer pump turned off");
  isFertilizerPumpOn=false;
  
}
void turnOnWaterPump(){

  digitalWrite(waterPumpPin,HIGH);
    isWaterPumpOn = true;
  // Serial.println("Water pump turned on");
  Alarm.timerOnce(780, turnOffWaterPump);            // called once after 10 seconds
}
void turnOffWaterPump(){
  
  digitalWrite(waterPumpPin,LOW);
  isWaterPumpOn = false;
  // Serial.println("Water pump turned off");

}
