#include "OLED.h"
#include "RTC_DS3231.h"
#include "Calendar.h"

// Create OLED instance with your pin configuration
// SCK=18, DIN=19, CS=22, DC=21, RES=20
OLED display(18, 19, 22, 21, 20);
RTC_DS3231 rtc(14, 15);
Calendar app;
// int TIME_TO_SLEEP = 5;
// unsigned long long uS_TO_S_FACTOR = 1000000;
// RTC_DATA_ATTR int bootCount = 0;

void setup() {
  Serial.begin(115200);
  // FileHeader f1;
  app.begin();
  // app.setup();
  rtc.begin();
  display.sh1106_init();
  // readCalendarHeader();

  // app.setup();

  // esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  // Serial.flush();
  // esp_deep_sleep_start();
  // delay(1000);
  
  // Serial.println("Testing OLED Library");
  
  // // Initialize display
  // display.sh1106_init();
  
  // // Clear buffer
  // display.clearBuffer();
  
  // // Draw some text
  // display.drawStringCentered(10, "Hello World!", 1);
  // display.drawString(10, 25, "Left Aligned", 1);
  // display.drawStringRight(35, "Right Aligned", 1);
  
  // // Draw graphics
  // display.drawRect(20, 45, 40, 15, 1);
  // display.fillRect(70, 45, 40, 15, 1);
  
  // // Update display
  // display.updateDisplay();
  
  // rtc.begin();
  // rtc.setTime(13, 24, 0, 24, 121, 2025, 4);
  
  // app.calendarLoop();
  // app.eventsCleanUp();
  // app.alarmQueueGen();
}

void loop() {
  // Your code here
  // static uint32_t lastUpdate = 0;
  // static uint32_t lastTemp = 0;
  
  // if(millis() - lastUpdate >= 1000) {
  //   lastUpdate = millis();
    
  //   uint8_t h, m, s, d, mon, wd;
  //   uint16_t y;
  //   rtc.getTime(h, m, s, d, mon, y, wd);
    
  //   Serial.print("Time: ");
  //   printTwoDigits(h); Serial.print(":");
  //   printTwoDigits(m); Serial.print(":");
  //   printTwoDigits(s);
    
  //   Serial.print("  Date: ");
  //   printTwoDigits(d); Serial.print("/");
  //   printTwoDigits(mon); Serial.print("/");
  //   Serial.print(y);
    
  //   const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  //   Serial.print("  ");
  //   Serial.println(days[wd-1]);
    
  //   // Display temperature every 30 seconds
  //   if(millis() - lastTemp >= 30000) {
  //     lastTemp = millis();
  //     float temp = rtc.getTemperature();
  //     if(temp > -100) {
  //       Serial.print("Temperature: ");
  //       Serial.print(temp, 2);
  //       Serial.println(" °C");
  //     }
  //   }
  // }
  app.calendarLoop();
}

// void printTwoDigits(uint8_t num) {
//   if(num < 10) Serial.print("0");
//   Serial.print(num);
// }

void readCalendarHeader() {
  // Check if file exists
  if(!LittleFS.exists("/calendarHeader.bin")) {
    Serial.println("ERROR: calendarHeader.bin does not exist!");
    return;
  }
  
  // Open and read the file
  File file = LittleFS.open("/calendarHeader.bin", "r");
  if(!file) {
    Serial.println("ERROR: Failed to open file!");
    return;
  }
  
  // Read the header structure
  FileHeader header;
  size_t bytesRead = file.readBytes((char*)&header, sizeof(FileHeader));
  file.close();
  
  if(bytesRead != sizeof(FileHeader)) {
    Serial.printf("ERROR: Read %d bytes, expected %d\n", 
                  bytesRead, sizeof(FileHeader));
    return;
  }
  
  // Display the data
  Serial.println("SUCCESS: File read successfully!");
  Serial.printf("  Version: %d\n", header.version);
  Serial.printf("  Total Events: %d\n", header.totalEvents);
  Serial.printf("  Current Event ID: %d\n", header.currentEventID);
  
  // Also show active.bin info
  if(LittleFS.exists("/active.bin")) {
    File activeFile = LittleFS.open("/active.bin", "r");
    uint16_t eventsInFile = activeFile.size() / sizeof(Event);
    activeFile.close();
    Serial.printf("  Events in active.bin: %d\n", eventsInFile);
  }
}