//   void setSchedules(){
  
//    Alarm.timerRepeat(2, Repeats2);      // timer for every 2 seconds
//   // create the alarms, to trigger at specific times
//   Alarm.alarmRepeat(1,26,0, turnOnLight);  // 8:30am every day
//   Alarm.alarmRepeat(1,26,5,turnOnFan);  // 5:45pm every day
//   Alarm.alarmRepeat(1,26,10,turnOnWaterPump);  // 5:45pm every day
//   Alarm.alarmRepeat(dowWednesday,1,23,30,turnOnFertilizerPump);  // 8:30:30 every Saturday
  
//   // Alarm.alarmRepeat(15,30,15,turnOnFertilizerPump);  // 5:45pm every day



//     Alarm.alarmRepeat(1,27,0, turnOffLight);  // 8:30am every day
//   Alarm.alarmRepeat(1,27,5,turnOffFan);  // 5:45pm every day
//   // Alarm.alarmRepeat(16,19,10,turnOffWaterPump);  // 5:45pm every day
//   // Alarm.alarmRepeat(15,31,15,turnOffFertilizerPump);  // 5:45pm every day
//   // Alarm.alarmRepeat(dowSaturday,8,30,30,WeeklyAlarm);  // 8:30:30 every Saturday

  
//  Serial.println("All schedules are set");

//   }

    void setSchedules(){
  
  //  Alarm.timerRepeat(2, Repeats2);      // timer for every 2 seconds
  // create the alarms, to trigger at specific times


   Alarm.alarmRepeat(6,25,0, turnOnLight);  // 8:30am every day
     Alarm.alarmRepeat(7,30,0,turnOnFan);  // 5:45am every day
   Alarm.alarmRepeat(2,30,0,turnOnWaterPump);  // 5:30am every day
  Alarm.alarmRepeat(8,30,0,turnOnWaterPump);  // 7:30pm every day
  Alarm.alarmRepeat(14,30,0,turnOnWaterPump);  // 10:30pm every day
  Alarm.alarmRepeat(20,30,0,turnOnWaterPump);  // 10:30pm every day
   Alarm.alarmRepeat(dowTuesday,8,0,0,turnOnFertilizerPump);  // 8:30:30 every Saturday
  
  // Alarm.alarmRepeat(15,30,15,turnOnFertilizerPump);  // 5:45pm every day



    Alarm.alarmRepeat(23,59,0, turnOffLight);  // 8:30am every day
  Alarm.alarmRepeat(23,29,5,turnOffFan);  // 5:45pm every day
  // Alarm.alarmRepeat(16,19,10,turnOffWaterPump);  // 5:45pm every day
  // Alarm.alarmRepeat(15,31,15,turnOffFertilizerPump);  // 5:45pm every day
  // Alarm.alarmRepeat(dowSaturday,8,30,30,WeeklyAlarm);  // 8:30:30 every Saturday

  
 Serial.println("All schedules are set");

  }

 