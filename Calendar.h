#ifndef CALENDAR_H
#define CALENDAR_H

#include <LittleFS.h>
#include <WiFi.h>

#define MAX_EVENTS_PER_DAY 50
#define WARNING_THRESHOLD 400
#define MAX_EVENTS 500

typedef struct {
  uint16_t id;         // 2 bytes
  char name[16];       // 16 bytes
  char details[32];    // 32 bytes
  uint16_t year;       // 2 bytes
  uint8_t month;       // 1 byte
  uint8_t date;        // 1 byte
  uint8_t hour;        // 1 byte
  uint8_t minute;      // 1 byte
  uint8_t flags1;      // 1 byte (bit6 to 5 - EventState, bit4 - isReminderSent, bit3 - isActive flag, bit2 to 0 - reminderType)
  uint8_t flags2;      // 1 byte (bit5 to 3 - repeatInterval, bit2 to 0 - repeatType)
  uint8_t repeatDays;  // 1 byte (0 - Waiting for next occurrence, 1 - Reminder sent, waiting for event, 2 - Event occurred, waiting for next (recurring), 4 - Moved to archive (non-recurring)
  uint32_t deletedAt;  // 4 bytes
} Event;

struct Time {
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint8_t date;
  uint8_t month;
  uint16_t year;
};

struct FileHeader {
  int version;
  int totalEvents;
  uint16_t currentEventID;
  // Add other fields if necessary
};

class Calendar {
public:
  Calendar(uint16_t serverPort = 80)
    : server(serverPort), port(serverPort) {
  }
  void begin() {
    Serial.println("Mounting LittleFS...");
    if (!LittleFS.begin()) {
      Serial.println("LittleFS mount failed! Formatting...");
      LittleFS.format();
      if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed after format!");
        while (true)
          ;
      }
    }
    Serial.println("LittleFS mounted successfully");

    // FIRST: Initialize calendarHeader with zeros to avoid garbage values
    memset(&calendarHeader, 0, sizeof(FileHeader));
    calendarHeader.version = 1;  // Set default version
    calendarHeader.totalEvents = 0;
    calendarHeader.currentEventID = 0;

    // SECOND: Read existing calendarHeader.bin if it exists
    if (LittleFS.exists("/calendarHeader.bin")) {
      File fileCalR = LittleFS.open("/calendarHeader.bin", "r");
      if (fileCalR) {
        FileHeader tempHeader;
        fileCalR.readBytes((char*)&tempHeader, sizeof(FileHeader));
        fileCalR.close();
        
        // Preserve the version from file if it's valid (>0), otherwise keep our default
        if (tempHeader.version > 0) {
          calendarHeader.version = tempHeader.version;
        }
        // Preserve currentEventID from file
        calendarHeader.currentEventID = tempHeader.currentEventID;
        Serial.printf("Read calendarHeader: version=%d, currentEventID=%d\n", 
                    calendarHeader.version, calendarHeader.currentEventID);
      }
    }

    // THIRD: Check active.bin and update totalEvents
    if (!LittleFS.exists("/active.bin")) {
      File file = LittleFS.open("/active.bin", "w");
      if (!file) {
        Serial.println("Failed to create active.bin file");
      } else {
        Serial.println("active.bin file created.");
      }
      file.close();
      calendarHeader.totalEvents = 0;
    } else {
      File file = LittleFS.open("/active.bin", "r");
      calendarHeader.totalEvents = file.size() / sizeof(Event);
      Serial.printf("Event Count: %d\r\n", calendarHeader.totalEvents);
      file.close();
    }

    // FOURTH: Always write back the updated header
    File fileCalW = LittleFS.open("/calendarHeader.bin", "w");
    if (fileCalW) {
      fileCalW.write((uint8_t*)&calendarHeader, sizeof(FileHeader));
      fileCalW.close();
      Serial.printf("Written calendarHeader: version=%d, totalEvents=%d, currentEventID=%d\n", 
                  calendarHeader.version, calendarHeader.totalEvents, calendarHeader.currentEventID);
    }

    if (!LittleFS.exists("/alarmQueue.bin")) {
      File file = LittleFS.open("/alarmQueue.bin", "w");
      if (!file) {
        Serial.println("Failed to create alarmQueue.bin file");
      } else {
        Serial.println("alarmQueue.bin file created.");
      }
      file.close();
    }

    // List files (for debugging)
    Serial.println("Files in LittleFS:");
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
      Serial.printf("  %s (%d bytes)\r\n", file.name(), file.size());
      file = root.openNextFile();
    }
    root.close();
  }

  void setup() {
    // Create Soft Access Point ONLY
    Serial.println("\n=== SETTING UP SOFT ACCESS POINT ===");

    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-Calendar", "calendar123");

    IPAddress apIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(apIP);
    Serial.print("AP SSID: ");
    Serial.println("ESP32-Calendar");
    Serial.print("AP Password: ");
    Serial.println("calendar123");

    // Start server
    server.begin();
    Serial.println("Server started on port 80");

    Serial.println("\n=== READY TO CONNECT ===");
    Serial.println("1. Connect to WiFi: ESP32-Calendar");
    Serial.println("2. Password: calendar123");
    Serial.println("3. Open browser to: http://192.168.4.1");
    Serial.println("============================\n");
  }

  void calendarLoop() {
    WiFiClient client = server.available();
    if (client) {
      Serial.println("\nNew client connected");
      // Call your existing handleClient function
      handleClient(client);
    }
  }

  void handleClient(WiFiClient client) {
    // Read the request (but we'll ignore it for now, just serve index.html)
    String request = client.readStringUntil('\r');
    Serial.println("Request: " + request);

    if (request.indexOf("GET /setTime") != -1) {
      Serial.println("=== /setTime Request Received ===");

      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println("Connection: close");
      client.println();

      int start = request.indexOf("datetime=");
      if (start != -1) {
        start += 9;  // "datetime=" is 9 characters
        int end = request.indexOf(" HTTP/", start);
        if (end == -1) end = request.length();

        String datetime = request.substring(start, end);

        // 1. URL Decode: Replace %3A with :
        datetime.replace("%3A", ":");
        Serial.println("Decoded datetime: " + datetime);

        // 2. Parse date (YYYY-MM-DD)
        cTime.year = datetime.substring(0, 4).toInt();
        cTime.month = datetime.substring(5, 7).toInt();
        cTime.date = datetime.substring(8, 10).toInt();

        // 3. Find time part (after 'T')
        int tIndex = datetime.indexOf('T');
        if (tIndex == -1) {
          client.print("{\"error\": \"Invalid format, missing 'T'\"}");
          client.stop();
          return;
        }

        String timePart = datetime.substring(tIndex + 1);
        Serial.println("Time part: " + timePart);

        // 4. Check if it's 12-hour format (has AM/PM)
        bool is12Hour = false;
        bool isPM = false;

        if (timePart.indexOf("PM") != -1) {
          is12Hour = true;
          isPM = true;
          timePart = timePart.substring(0, timePart.indexOf("PM"));
        } else if (timePart.indexOf("AM") != -1) {
          is12Hour = true;
          isPM = false;
          timePart = timePart.substring(0, timePart.indexOf("AM"));
        }

        // 5. Parse time components
        int hour, minute, second;

        // Find positions of colons
        int colon1 = timePart.indexOf(':');
        int colon2 = timePart.lastIndexOf(':');

        if (colon1 == -1) {
          client.print("{\"error\": \"Invalid time format\"}");
          client.stop();
          return;
        }

        hour = timePart.substring(0, colon1).toInt();

        if (colon2 != -1 && colon2 > colon1) {
          // Has seconds: HH:MM:SS
          minute = timePart.substring(colon1 + 1, colon2).toInt();
          second = timePart.substring(colon2 + 1).toInt();
        } else {
          // No seconds: HH:MM
          minute = timePart.substring(colon1 + 1).toInt();
          second = 0;
        }

        // 6. Convert 12-hour to 24-hour ONLY if it's actually 12-hour format
        if (is12Hour) {
          if (isPM && hour < 12) {
            hour += 12;
          } else if (!isPM && hour == 12) {
            hour = 0;  // 12:xx AM becomes 00:xx
          }
        }
        // If it's already 24-hour format (from JavaScript), leave as-is
        // 12:43 in 24-hour format is noon, not midnight

        cTime.hour = hour;
        cTime.min = minute;
        cTime.sec = second;

        // 7. Send response
        String jsonResponse = "{\"date\": \"" + String(cTime.date) + "\", \"month\": \"" + String(cTime.month) + "\", \"year\": \"" + String(cTime.year) + "\", \"hour\": \"" + String(cTime.hour) + "\", \"min\": \"" + String(cTime.min) + "\", \"sec\": \"" + String(cTime.sec) + "\"}";

        Serial.println("Sending JSON: " + jsonResponse);
        Serial.printf("Parsed as: %02d:%02d:%02d (is12Hour: %d, isPM: %d)\r\n",
                      hour, minute, second, is12Hour, isPM);

        client.print(jsonResponse);

        // TODO: Set ESP32's RTC time here
        Serial.printf("Time set to: %04d-%02d-%02d %02d:%02d:%02d\r\n",
                      cTime.year, cTime.month, cTime.date,
                      cTime.hour, cTime.min, cTime.sec);
      } else {
        client.print("{\"error\": \"No datetime parameter\"}");
      }

      client.stop();
      Serial.println("Client disconnected");
      return;
    }

    if (request.indexOf("GET /getTime") != -1) {
      // Read RTC module and then send updated cTime
      /* ... */
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println("Connection: close");
      client.println();
      String jsonResponse = "{\"date\": \"" + String(cTime.date) + "\", \"month\": \"" + String(cTime.month) + "\", \"year\": \"" + String(cTime.year) + "\", \"hour\": \"" + String(cTime.hour) + "\", \"min\": \"" + String(cTime.min) + "\", \"sec\": \"" + String(cTime.sec) + "\"}";

      Serial.println("Sending JSON: " + jsonResponse);
      client.print(jsonResponse);
      Serial.printf("Time set to: %04d-%02d-%02d %02d:%02d:%02d\r\n",
                    cTime.year, cTime.month, cTime.date,
                    cTime.hour, cTime.min, cTime.sec);
      client.stop();
      Serial.println("Client disconnected");
      return;
    }

    if (request.indexOf("POST /addEvent") != -1) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println("Connection: close");
      client.println();
      File calHeadFile = LittleFS.open("/calendarHeader.bin", "r");
      calHeadFile.readBytes((char*)&calendarHeader, sizeof(FileHeader));
      calHeadFile.close();
      if (calendarHeader.totalEvents >= WARNING_THRESHOLD) {
        client.print("Warning, 80% memory full.");
      }
      if (calendarHeader.totalEvents == MAX_EVENTS) {
        client.print("100% memory full. Delete events please.");
        client.stop();
        Serial.println("Client disconnected");
        return;
      }
      // 1. Read headers to find content length
      int contentLength = 0;
      while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line.startsWith("Content-Length: ")) {
          contentLength = line.substring(16).toInt();
        }
        if (line == "\r") {  // Empty line = end of headers
          break;
        }
        Serial.print("Header: ");
        Serial.println(line);
      }

      Serial.print("Content-Length: ");
      Serial.println(contentLength);

      // 2. Read the JSON body
      String jsonBody = "";
      if (contentLength > 0) {
        for (int i = 0; i < contentLength; i++) {
          if (client.available()) {
            jsonBody += (char)client.read();
          }
        }
      }
      Serial.println("JSON Body: " + jsonBody);

      String eventName = jsonBody.substring(jsonBody.indexOf("\"name\":\"") + 8, jsonBody.indexOf("\"", jsonBody.indexOf("\"name\":\"") + 8));
      String eventDetails = jsonBody.substring(jsonBody.indexOf("\"details\":\"") + 11, jsonBody.indexOf("\"", jsonBody.indexOf("\"details\":\"") + 11));
      String eventDatetime = jsonBody.substring(jsonBody.indexOf("\"date\":") + 8, jsonBody.indexOf("\"", jsonBody.indexOf("\"date\":") + 8));

      uint16_t eventYear = eventDatetime.substring(eventDatetime.indexOf('-') - 4, eventDatetime.indexOf('-')).toInt();
      uint16_t eventMonth = eventDatetime.substring(eventDatetime.indexOf('-') + 1, eventDatetime.indexOf('-') + 3).toInt();
      uint16_t eventDate = eventDatetime.substring(eventDatetime.indexOf('-') + 4, eventDatetime.indexOf('-') + 6).toInt();
      uint8_t eventHour = eventDatetime.substring(eventDatetime.indexOf('T') + 1, eventDatetime.indexOf('T') + 3).toInt();  //eventDatetime.substring(jsonBody.indexOf('T') + 1, jsonBody.indexOf('T') + 3).toInt();
      uint8_t eventMin = eventDatetime.substring(eventDatetime.indexOf('T') + 4, eventDatetime.indexOf('T') + 6).toInt();

      uint8_t reminder = jsonBody.substring(jsonBody.indexOf("\"reminder\":") + String("\"reminder\":").length(), jsonBody.indexOf(",\"repeatType\":")).toInt();
      uint8_t repeatType = jsonBody.substring(jsonBody.indexOf("\"repeatType\":") + String("\"repeatType\":").length(), jsonBody.indexOf(",\"repeatInterval\":")).toInt();
      uint8_t repeatInterval = jsonBody.substring(jsonBody.indexOf("\"repeatInterval\":") + String("\"repeatInterval\":").length(), jsonBody.indexOf(",\"repeatDays\":")).toInt();
      uint8_t repeatDays = jsonBody.substring(jsonBody.indexOf("\"repeatDays\":") + String("\"repeatDays\":").length(), jsonBody.indexOf("}")).toInt();
      size_t freeBytes = LittleFS.totalBytes()-LittleFS.usedBytes(); // redundant
      /*
        Check if space available
      */
      if (freeBytes > sizeof(Event)) {
        Event newEvent;
        newEvent.id = (calendarHeader.totalEvents + 1);
        eventName.toCharArray(newEvent.name, sizeof(eventName));
        eventDetails.toCharArray(newEvent.details, sizeof(eventDetails));
        newEvent.year = eventYear;
        newEvent.month = eventMonth;
        newEvent.date = eventDate;
        newEvent.hour = eventHour;
        newEvent.minute = eventMin;
        newEvent.flags1 = 0;
        newEvent.flags1 = (reminder & 0b00000111) | (1 << 3);
        // newEvent.flags1 = ((reminder << 2) | (0b00000001 << 3)) & ~(0b00000001 << 4) & ~((0b00000011) << 5);
        newEvent.flags2 = 0;
        newEvent.flags2 = (repeatType & 0b00000111) | (((0b00000111) & repeatInterval) << 3);
        newEvent.repeatDays = repeatDays;
        File file = LittleFS.open("/active.bin", "a");

        // CRITICAL CHECK: Was the file actually opened?
        if (!file) {
          client.print("Failed to OPEN file for writing.");
          Serial.println("ERROR: Failed to open /active.bin for appending.");
          // You might also want to print the LittleFS error here:
          // Serial.println(LittleFS.error());
          client.stop();
          return;
        }

        // Now it's safe to try writing
        size_t written = file.write((uint8_t*)&newEvent, sizeof(Event));
        Serial.print("Written bytes: ");
        Serial.println(written);
        file.close();

        if (written == sizeof(Event)) {
          calendarHeader.totalEvents += 1;
          File calHeadFile = LittleFS.open("/calendarHeader.bin","w");
          calHeadFile.write((uint8_t*)&calendarHeader, sizeof(FileHeader));
          calHeadFile.close();
          client.print("Event Received. name: " + String(newEvent.name) + ", details: " + String(newEvent.details) + ", year: " + String(newEvent.year) + ", month: " + String(newEvent.month) + ", date: " + String(newEvent.date) + ", reminder: " + String(/*reminder*/ extractReminderType(&newEvent)) + ", repeatType: " + String(extractRepeatType(&newEvent)) + ", repeatInterval: " + String(extractRepeatInterval(&newEvent)) + ", repeatDays: " + String(newEvent.repeatDays));
          client.print(", hour: " + String(newEvent.hour) + ", minute: " + String(newEvent.minute));
          client.print(", id: " + String(newEvent.id) + ", total events: " + String(calendarHeader.totalEvents));
        } else {
          client.print("Failed to WRITE data to file.");
          Serial.print("ERROR: Write failed. Expected ");
          Serial.print(sizeof(Event));
          Serial.print(" bytes, wrote ");
          Serial.println(written);
        }
        Add2AlarmQueue(&newEvent);
      }

      // client.print("Event Received. reminder: " + String(reminder) + ", repeatType: " + String(repeatType) + ", repeatInterval: " + String(repeatInterval) + ", repeatDays: " + String(repeatDays) + ".");
      client.stop();
      Serial.println("Client disconnected");
      return;
    }

    if (request.indexOf("GET /getEvents") != -1) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println("Connection: close");
      client.println();
      String jsonResponse = "[";
      File eventsFile = LittleFS.open("/active.bin", "r");
      uint16_t eventCount = eventsFile.size() / sizeof(Event);

      Time nextDay = julianToDate(julianDay(cTime.year, cTime.month, cTime.date) + 1, 0, 0, 0);
      Time today = julianToDate(julianDay(cTime.year, cTime.month, cTime.date), 0, 0, 0);

      for (uint16_t i = 0; i < eventCount; i++) {
        Event e;
        eventsFile.readBytes((char*)&e, sizeof(Event));
        Time eventDateTime = applyReminderOffset(&e);
        uint8_t inQueue;
        Serial.print("\r\ntoDay_year: " + String(today.year) + ", toDay_month: " + String(today.month) + ", toDay_date: " + String(today.date));
        Serial.println(", toDay_hour: " + String(today.hour) + ", toDay_minute: " + String(today.min));

        Serial.print("\r\nyear: " + String(eventDateTime.year) + ", month: " + String(eventDateTime.month) + ", date: " + String(eventDateTime.date));
        Serial.println(", hour: " + String(eventDateTime.hour) + ", minute: " + String(eventDateTime.min));

        Serial.print("\r\nnextDay_year: " + String(nextDay.year) + ", nextDay_month: " + String(nextDay.month) + ", nextDay_date: " + String(nextDay.date));
        Serial.println(", nextDay_hour: " + String(nextDay.hour) + ", nextDay_minute: " + String(nextDay.min));
        if ((compareDateTime(&today, &eventDateTime) == -1) && (compareDateTime(&eventDateTime, &nextDay) == -1)) {
          inQueue = 1;
        } else {
          inQueue = 0;
        }
        if (i > 0) {
          jsonResponse += ",";
        }
        jsonResponse += "{";
        jsonResponse += "\"id\":" + String(e.id) + ",";
        jsonResponse += "\"name\":\"" + String(e.name) + "\",";
        jsonResponse += "\"date\":\"" + String(getEventDateTime(&e).year) + "/" + String(getEventDateTime(&e).month) + "/" + String(getEventDateTime(&e).date) + "T" + String(getEventDateTime(&e).hour) + ":" + String(getEventDateTime(&e).min) + "\",";
        jsonResponse += "\"reminder\":\"" + String(extractReminderType(&e)) + "\",";
        jsonResponse += "\"repeat\":\"" + String(extractRepeatType(&e)) + "\",";
        jsonResponse += "\"details\":\"" + String(e.details) + "\",";
        jsonResponse += "\"inQueue\":\"" + String(inQueue) + "\"";
        jsonResponse += "}";
      }
      eventsFile.close();
      jsonResponse += "]";
      client.print(jsonResponse);
      client.stop();
      Serial.println("Client disconnected");
    }

    if (request.indexOf("GET /deleteEvent") != -1) {
      uint16_t eventID = request.substring(request.indexOf("?id=") + 4, request.indexOf(" HTTP/1.1")).toInt();
      Serial.print("Event: ");
      Serial.println(eventID);
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println("Connection: close");
      client.println();

      deleteEventByID(eventID);

      // File allEventsRead = LittleFS.open("/active.bin", "r");
      // // eventsArray = {0};
      // memset(eventsArray, 0, sizeof(eventsArray));
      // allEventsRead.readBytes((char*)eventsArray, allEventsRead.size());
      // allEventsRead.close();

      // File calHeadFileRead = LittleFS.open("/calendarHeader.bin", "r");
      // calHeadFileRead.readBytes((char*)&calendarHeader, sizeof(FileHeader));
      // calHeadFileRead.close();

      // for (uint16_t i = 0; i < calendarHeader.totalEvents; i++) {
      //   Serial.print("id: ");
      //   Serial.print(eventsArray[i].id);
      //   Serial.print(", ");
      // }
      // for (uint16_t i = eventID - 1; i < calendarHeader.totalEvents - 1; i++) {
      //   eventsArray[i] = eventsArray[i + 1];
      //   eventsArray[i].id = i + 1;
      // }
      // Event emptyEvent = { 0 };
      // eventsArray[calendarHeader.totalEvents - 1] = emptyEvent;
      // Serial.println("eventsArray:");
      // for (uint16_t i = 0; i < calendarHeader.totalEvents; i++) {
      //   Serial.print("id: ");
      //   Serial.print(eventsArray[i].id);
      //   Serial.print(", ");
      // }
      // Serial.println();
      // calendarHeader.totalEvents -= 1;

      // File calHeadFileWrite = LittleFS.open("/calendarHeader.bin","w");
      // calHeadFileWrite.write((uint8_t*)&calendarHeader, sizeof(FileHeader));
      // calHeadFileWrite.close();

      // File allEventsWrite = LittleFS.open("/active.bin", "w");
      // allEventsWrite.write((uint8_t*)eventsArray, calendarHeader.totalEvents * sizeof(Event));
      // allEventsWrite.close();
      // File alarmQueueRead = LittleFS.open("/alarmQueue.bin", "r");
      // memset(eventsIDArray, 0, sizeof(eventsIDArray));
      // alarmQueueRead.readBytes((char*)eventsIDArray, alarmQueueRead.size());
      // Serial.println("eventsIDArray:");
      // for (uint16_t i = 0; i < calendarHeader.totalEvents; i++) {
      //   Serial.print("id: ");
      //   Serial.print(eventsIDArray[i]);
      //   Serial.print(", ");
      // }
      // Serial.println();
      // int16_t alarmQueueDeleteIndex = -1;
      // uint16_t alarmQueueLength = alarmQueueRead.size() / sizeof(uint16_t);
      // for (uint16_t i = 0; i < alarmQueueRead.size() / sizeof(uint16_t); i++) {
      //   if (eventsIDArray[i] == eventID) {
      //     alarmQueueDeleteIndex = i;
      //     break;
      //   }
      // }
      // if (alarmQueueDeleteIndex != -1) {
      //   for (uint16_t i = alarmQueueDeleteIndex; i < alarmQueueRead.size() / sizeof(uint16_t) - 1; i++) {
      //     eventsIDArray[i] = eventsIDArray[i + 1];
      //   }
      //   eventsIDArray[alarmQueueRead.size() / sizeof(uint16_t) - 1] = 0;
      //   for (int16_t i = 0; i < alarmQueueRead.size() / sizeof(uint16_t) - 1; i++) {
      //     if (eventsIDArray[i] > eventID) {
      //       eventsIDArray[i]--;
      //     }
      //   }
      //   alarmQueueRead.close();
      //   File alarmQueueWrite = LittleFS.open("/alarmQueue.bin", "w");
      //   alarmQueueWrite.write((uint8_t*)eventsIDArray, (alarmQueueLength - 1) * sizeof(uint16_t));
      //   alarmQueueWrite.close();
      // } else {
      //   // Event being deleted wasn't in the Alarm Queue.
      //   if (alarmQueueRead.size() / sizeof(uint16_t) > 0) {
      //     for (uint16_t i = 0; i < alarmQueueRead.size() / sizeof(uint16_t) - 1; i++) {
      //       if (eventsIDArray[i] > eventID) {
      //         eventsIDArray[i]--;
      //       }
      //     }
      //     alarmQueueRead.close();
      //     File alarmQueueWrite = LittleFS.open("/alarmQueue.bin", "w");
      //     alarmQueueWrite.write((uint8_t*)eventsIDArray, alarmQueueLength * sizeof(uint16_t));
      //     alarmQueueWrite.close();
      //   } else {
      //     alarmQueueRead.close();
      //   }
      // }
      client.stop();
      Serial.println("Client disconnected");
      return;
    }

    if (request.indexOf("GET /getSystemInfo") != -1) {
      File htmlFile = LittleFS.open("/index.html", "r");
      if (!htmlFile) {
        Serial.printf("Failed to open %s\n", "index.html");
        return;
      }

      size_t htmlFileSizeKB = htmlFile.size() / 1024;
      htmlFile.close();

      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      size_t eventSize = sizeof(Event);
      File calHeadFileRead = LittleFS.open("/calendarHeader.bin", "r");
      calHeadFileRead.readBytes((char*)&calendarHeader, sizeof(FileHeader));
      calHeadFileRead.close();
      uint16_t eventsInMemory = calendarHeader.totalEvents;
      size_t totalKB = LittleFS.totalBytes() / 1024;
      size_t usedKB = LittleFS.usedBytes() / 1024;
      uint16_t maxEventsInMemory = 500;
      size_t freeKB = totalKB - usedKB;
      String jsonBody = "{\"eventSize\": \"" + String(eventSize) + "\", \"eventsInMemory\": \"" + String(eventsInMemory) + "\", \"maxEventsInMemory\": \"" + String(maxEventsInMemory) + "\", \"totalKB\": \"" + String(totalKB) + "\", \"usedKB\": \"" + String(usedKB) + "\", \"freeKB\": \"" + String(freeKB) + "\"}";
      client.print(jsonBody);
      client.stop();
      Serial.println("Client disconnected");
      return;
    }

    // Send HTTP headers
    if (request.indexOf("GET / ") != -1) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();

      // Open and send the HTML file
      File file = LittleFS.open("/index.html", "r");
      if (!file) {
        Serial.println("Failed to open file");
        client.println("<h1>404 - File not found</h1>");
        client.println("<p>index.html not found in LittleFS</p>");
      } else {
        // Send file content chunk by chunk
        while (file.available()) {
          client.write(file.read());
        }
        file.close();
        Serial.println("File sent successfully");
        client.stop();
        Serial.println("Client disconnected");
        return;
      }
    }
  }

  void deleteEventByID(uint16_t eventID) {
    File calHeadFileRead = LittleFS.open("/calendarHeader.bin", "r");
    calHeadFileRead.readBytes((char*)&calendarHeader, sizeof(FileHeader));
    calHeadFileRead.close();

    for (uint16_t i = 0; i < calendarHeader.totalEvents; i++) {
      Serial.print("id: ");
      Serial.print(eventsArray[i].id);
      Serial.print(", ");
    }
    for (uint16_t i = eventID - 1; i < calendarHeader.totalEvents - 1; i++) {
      eventsArray[i] = eventsArray[i + 1];
      eventsArray[i].id = i + 1;
    }
    Event emptyEvent = { 0 };
    eventsArray[calendarHeader.totalEvents - 1] = emptyEvent;
    Serial.println("eventsArray:");
    for (uint16_t i = 0; i < calendarHeader.totalEvents; i++) {
      Serial.print("id: ");
      Serial.print(eventsArray[i].id);
      Serial.print(", ");
    }
    Serial.println();
    calendarHeader.totalEvents -= 1;

    File calHeadFileWrite = LittleFS.open("/calendarHeader.bin","w");
    calHeadFileWrite.write((uint8_t*)&calendarHeader, sizeof(FileHeader));
    calHeadFileWrite.close();

    File allEventsWrite = LittleFS.open("/active.bin", "w");
    allEventsWrite.write((uint8_t*)eventsArray, calendarHeader.totalEvents * sizeof(Event));
    allEventsWrite.close();
    File alarmQueueRead = LittleFS.open("/alarmQueue.bin", "r");
    memset(eventsIDArray, 0, sizeof(eventsIDArray));
    alarmQueueRead.readBytes((char*)eventsIDArray, alarmQueueRead.size());
    Serial.println("eventsIDArray:");
    for (uint16_t i = 0; i < calendarHeader.totalEvents; i++) {
      Serial.print("id: ");
      Serial.print(eventsIDArray[i]);
      Serial.print(", ");
    }
    Serial.println();
    int16_t alarmQueueDeleteIndex = -1;
    uint16_t alarmQueueLength = alarmQueueRead.size() / sizeof(uint16_t);
    for (uint16_t i = 0; i < alarmQueueRead.size() / sizeof(uint16_t); i++) {
      if (eventsIDArray[i] == eventID) {
        alarmQueueDeleteIndex = i;
        break;
      }
    }
    if (alarmQueueDeleteIndex != -1) {
      for (uint16_t i = alarmQueueDeleteIndex; i < alarmQueueRead.size() / sizeof(uint16_t) - 1; i++) {
        eventsIDArray[i] = eventsIDArray[i + 1];
      }
      eventsIDArray[alarmQueueRead.size() / sizeof(uint16_t) - 1] = 0;
      for (int16_t i = 0; i < alarmQueueRead.size() / sizeof(uint16_t) - 1; i++) {
        if (eventsIDArray[i] > eventID) {
          eventsIDArray[i]--;
        }
      }
      alarmQueueRead.close();
      File alarmQueueWrite = LittleFS.open("/alarmQueue.bin", "w");
      alarmQueueWrite.write((uint8_t*)eventsIDArray, (alarmQueueLength - 1) * sizeof(uint16_t));
      alarmQueueWrite.close();
    } else {
      // Event being deleted wasn't in the Alarm Queue.
      if (alarmQueueRead.size() / sizeof(uint16_t) > 0) {
        for (uint16_t i = 0; i < alarmQueueRead.size() / sizeof(uint16_t) - 1; i++) {
          if (eventsIDArray[i] > eventID) {
            eventsIDArray[i]--;
          }
        }
        alarmQueueRead.close();
        File alarmQueueWrite = LittleFS.open("/alarmQueue.bin", "w");
        alarmQueueWrite.write((uint8_t*)eventsIDArray, alarmQueueLength * sizeof(uint16_t));
        alarmQueueWrite.close();
      } else {
        alarmQueueRead.close();
      }
    }
  }

  void eventsCleanUp() {
    cTime.year = 2026;
    cTime.month = 1;
    cTime.date = 1;
    File calHeadFile = LittleFS.open("/calendarHeader.bin","r");
    calHeadFile.readBytes((char*)&calendarHeader, sizeof(FileHeader));
    calHeadFile.close();
    File eventsFile = LittleFS.open("/active.bin","r");
    eventsFile.readBytes((char*)eventsArray, calendarHeader.totalEvents * sizeof(Event));
    eventsFile.close();
    for(uint16_t i = 0; i < calendarHeader.totalEvents; i++) {
      uint8_t event_i_state = extractEventState(&eventsArray[i]);
      if(extractReminderType(&eventsArray[i]) == 0) { // should be repeatType
        if(event_i_state != 0) {
          deleteEventByID(eventsArray[i].id);
        }
      }
    }
  }

  void alarmQueueGen() {
    File calHeadFile = LittleFS.open("/calendarHeader.bin","r");
    calHeadFile.readBytes((char*)&calendarHeader, sizeof(FileHeader));
    calHeadFile.close();
    Serial.printf("number of events: %d\r\n", calendarHeader.totalEvents);
    File eventsFile = LittleFS.open("/active.bin","r");
    eventsFile.readBytes((char*)eventsArray, calendarHeader.totalEvents * sizeof(Event));
    eventsFile.close();
    for(uint16_t i = 0; i < calendarHeader.totalEvents; i++) {
      Serial.println("=======");
      Serial.printf("Event_name: %s\r\n", eventsArray[i].name);
      Serial.printf("Event_id: %d\r\n", eventsArray[i].id);
      Serial.printf("Date-time: %d.%d.%d T %d:%d\r\n", eventsArray[i].year, eventsArray[i].month, eventsArray[i].date, eventsArray[i].hour, eventsArray[i].minute);
    }
    Serial.println("=======");
    Time time_max = julianToDate(julianDay(cTime.year, cTime.month, cTime.date) + 1, 0, 0, 0);
    Time time_min = julianToDate(julianDay(cTime.year, cTime.month, cTime.date), 0, 0, 0);
    uint16_t alarmQueueLen = 0;
    for(uint16_t i = 0; i < calendarHeader.totalEvents; i++) {
      Event event_i = eventsArray[i];
      Time time_i = applyReminderOffset(&event_i);
      uint16_t min_index = i;
      Serial.println("element"+String(i));
      for(uint16_t j = i; j < calendarHeader.totalEvents; j++) {
        Time time_j = applyReminderOffset(&eventsArray[j]);
        Serial.printf("%d.%d.%d T %d:%d -Vs- %d.%d.%d T %d:%d => ", time_i.date, time_i.month, time_i.year, time_i.hour, time_i.min, time_j.date, time_j.month, time_j.year, time_j.hour, time_j.min);
        if(compareDateTime(&time_i, &time_j) > 0) {
          time_i = time_j;
          event_i = eventsArray[j];
          min_index = j;
          Serial.println("greater");
        }
        else {
          Serial.println("less than or equal.");
        }
      }
      Serial.printf("Date-time: %d.%d.%d T %d:%d\r\n", time_i.date, time_i.month, time_i.year, time_i.hour, time_i.min);
      Serial.printf("Date-time: %d.%d.%d T %d:%d\r\n", time_min.date, time_min.month, time_min.year, time_min.hour, time_min.min);
      Serial.printf("Date-time: %d.%d.%d T %d:%d\r\n", time_max.date, time_max.month, time_max.year, time_max.hour, time_max.min);
      if((compareDateTime(&time_i, &time_min) >= 0) && (compareDateTime(&time_i, &time_max) < 0)) {
        Event swap_event = eventsArray[i];
        eventsArray[i] = eventsArray[min_index];
        eventsArray[min_index] = swap_event;
        alarmQueueLen += 1;
      }
      else {
        break;
      }
    }
    Serial.println("alarmQueueLen: "+String(alarmQueueLen));
    for(uint16_t i = 0; i < alarmQueueLen; i++) {
      Serial.println("=======");
      Serial.printf("Event_name: %s\r\n", eventsArray[i].name);
      Serial.printf("Event_id: %d\r\n", eventsArray[i].id);
      Serial.printf("Date-time: %d.%d.%d T %d:%d\r\n", eventsArray[i].year, eventsArray[i].month, eventsArray[i].date, eventsArray[i].hour, eventsArray[i].minute);
    }
    Serial.println("=======");
  }

private:
  WiFiServer server;
  uint16_t port;
  FileHeader calendarHeader;  // Declare the instance of FileHeader
  Time cTime;                 // current time struct
  Event eventsArray[MAX_EVENTS] = { 0 };
  uint16_t eventsIDArray[MAX_EVENTS_PER_DAY] = { 0 };

  // Extracts bit2 to 0 of flag1
  uint8_t extractReminderType(Event* e) {
    return (e->flags1 & 0b00000111);
  }

  // Extracts bit2 to 0 of flag2
  uint8_t extractRepeatType(Event* e) {
    return (e->flags2 & 0b00000111);
  }

  // Extracts bit5 to 3 of flag2
  uint8_t extractRepeatInterval(Event* e) {
    return ((e->flags2 >> 3) & 0b00000111);
  }

  // Extracts bit6 to 3 of flag1
  uint8_t extractEventFlags(Event* e) {
    return ((e->flags1 >> 3) & 0b00001111);
  }

  // checks if Event is active by checking bit0 of Event flags byte
  bool isEventActive(Event* e) {
    if (extractEventFlags(e) & 0b00000001) {
      return true;
    } else {
      return false;
    }
  }

  // checks if Event Reminder is sent by checking bit1 of Event flags byte
  bool isReminderSent(Event* e) {
    if ((extractEventFlags(e) >> 1) & 0b00000001) {
      return true;
    } else {
      return false;
    }
  }

  // checks Event state by checking bit3 to 2 of Event flags byte
  /*
    if ReminderType == 0:
      00 - Alarm not reached
      10 - Alarm reached
    if ReminderType != 0:
      00 - Alarm not reached, Reminder not reached
      01 - Alarm not reached, Reminder reached
      11 - Alarm reached, Reminder reached
  */
  uint8_t extractEventState(Event* e) {
    return (extractEventFlags(e) >> 2) & 0b00000011;
  }

  Time getEventDateTime(Event* e) {
    Time EventDateTime;
    EventDateTime.date = e->date;
    EventDateTime.month = e->month;
    EventDateTime.year = e->year;
    EventDateTime.sec = 0;
    EventDateTime.min = e->minute;
    EventDateTime.hour = e->hour;
    return EventDateTime;
  }

  // Convert date to Julian Day Number
  uint32_t julianDay(uint16_t y, uint8_t m, uint8_t d) {
    uint16_t a = (14 - m) / 12;
    uint16_t year = y + 4800 - a;
    uint16_t month = m + 12 * a - 3;

    uint32_t jdn = d + ((153 * month + 2) / 5) + (365 * year) + (year / 4) - (year / 100) + (year / 400) - 32045;
    return jdn;
  }

  // Convert Julian Day Number back to date
  Time julianToDate(uint32_t jd, uint8_t hour, uint8_t min, uint8_t sec) {
    Time t;
    uint32_t f = jd + 1401 + (((4 * jd + 274277) / 146097) * 3) / 4 - 38;
    uint32_t e = 4 * f + 3;
    uint32_t g = (e % 1461) / 4;
    uint32_t h = 5 * g + 2;

    t.date = (h % 153) / 5 + 1;
    t.month = (h / 153 + 2) % 12 + 1;
    t.year = (e / 1461) - 4716 + (12 + 2 - t.month) / 12;
    t.hour = hour;
    t.min = min;
    t.sec = sec;

    return t;
  }

  Time applyReminderOffset(Event* e) {
    Time offsetDateTime;
    uint32_t julianTime = julianDay(e->year, e->month, e->date);
    uint8_t reminderType = extractReminderType(e);
    switch (reminderType) {
      case 0:
        offsetDateTime = getEventDateTime(e);
        break;
      case 1:
        if (e->hour == 0) {
          julianTime -= 1;
          offsetDateTime = julianToDate(julianTime, 23, e->minute, 0);
          offsetDateTime.hour = 23;
          break;
        } else {
          offsetDateTime = getEventDateTime(e);
          offsetDateTime.hour -= 1;
          break;
        }
      case 2:
        julianTime -= 1;
        offsetDateTime = julianToDate(julianTime, e->hour, e->minute, 0);
        break;
      case 3:
        julianTime -= 7;
        offsetDateTime = julianToDate(julianTime, e->hour, e->minute, 0);
        break;
      case 4:
        julianTime -= 31;
        offsetDateTime = julianToDate(julianTime, e->hour, e->minute, 0);
        break;
      default:
        break;
    }
    return offsetDateTime;
  }

  int8_t compareDateTime(Time* a, Time* b) {
    uint32_t jdA = julianDay(a->year, a->month, a->date);
    uint32_t jdB = julianDay(b->year, b->month, b->date);
    if (jdA > jdB) {
      return 1;
    } else if (jdA < jdB) {
      return -1;
    } else {
      if (a->hour > b->hour) {
        return 1;
      } else if (a->hour < b->hour) {
        return -1;
      } else {
        if (a->min > b->min) {
          return 1;
        } else if (a->min < b->min) {
          return -1;
        } else {
          return 0;
        }
      }
    }
  }

  Event findEventByID(uint16_t eventID) {
    Event e;
    File activeEvents = LittleFS.open("/active.bin", "r");
    activeEvents.seek((eventID - 1) * sizeof(Event), SeekSet);
    activeEvents.readBytes((char*)&e, sizeof(Event));
    activeEvents.close();
    return e;
  }

  void Add2AlarmQueue(Event* e) {
    Time eventDateTime = applyReminderOffset(e);
    Serial.print("year: " + String(e->year) + ", month: " + String(e->month) + ", date: " + String(e->date) + ", reminder: " + String(/*reminder*/ extractReminderType(e)) + ", repeatType: " + String(extractRepeatType(e)) + ", repeatInterval: " + String(extractRepeatInterval(e)) + ", repeatDays: " + String(e->repeatDays));
    Serial.print(", hour: " + String(e->hour) + ", minute: " + String(e->minute));
    Time nextDay = julianToDate(julianDay(cTime.year, cTime.month, cTime.date) + 1, 0, 0, 0);
    Serial.println("\r\n");
    Time today = julianToDate(julianDay(cTime.year, cTime.month, cTime.date), 0, 0, 0);

    Serial.print("\r\ntoDay_year: " + String(today.year) + ", toDay_month: " + String(today.month) + ", toDay_date: " + String(today.date));
    Serial.println(", toDay_hour: " + String(today.hour) + ", toDay_minute: " + String(today.min));

    Serial.print("\r\nyear: " + String(eventDateTime.year) + ", month: " + String(eventDateTime.month) + ", date: " + String(eventDateTime.date));
    Serial.println(", hour: " + String(eventDateTime.hour) + ", minute: " + String(eventDateTime.min));

    Serial.print("\r\nnextDay_year: " + String(nextDay.year) + ", nextDay_month: " + String(nextDay.month) + ", nextDay_date: " + String(nextDay.date));
    Serial.println(", nextDay_hour: " + String(nextDay.hour) + ", nextDay_minute: " + String(nextDay.min));

    if ((compareDateTime(&today, &eventDateTime) == -1) && (compareDateTime(&eventDateTime, &nextDay) == -1)) {
      Serial.println("Event is today.");
      File alarmQueue_read = LittleFS.open("/alarmQueue.bin", "r");
      File activeEvents = LittleFS.open("/active.bin", "r");
      uint16_t currentLength = alarmQueue_read.size() / sizeof(uint16_t);
      if (currentLength >= MAX_EVENTS_PER_DAY) {
        Serial.println("Error: Daily event limit reached");
        alarmQueue_read.close();
        activeEvents.close();
        return;  // Don't add more events
      }
      if (alarmQueue_read.size() > 0) {
        uint16_t ID_posn = alarmQueue_read.size() / sizeof(uint16_t) + 1;
        // eventsIDArray[MAX_EVENTS_PER_DAY] = {0};
        memset(eventsIDArray, 0, sizeof(eventsIDArray));
        size_t bytesRead = alarmQueue_read.readBytes((char*)eventsIDArray, alarmQueue_read.size());
        if (bytesRead != alarmQueue_read.size()) {
          Serial.println("Error reading files.");
        }
        for (uint16_t i = 0; i < alarmQueue_read.size() / sizeof(uint16_t); i++) {
          uint16_t ID_i = eventsIDArray[i];
          Event event_i = findEventByID(ID_i);
          Time dateTime_i = applyReminderOffset(&event_i);
          if (compareDateTime(&dateTime_i, &eventDateTime) >= 0) {
            ID_posn = i + 1;
            break;
          }
        }
        for (int16_t i = alarmQueue_read.size() / sizeof(uint16_t); i > ID_posn; i--) {
          eventsIDArray[i] = eventsIDArray[i - 1];
        }
        uint16_t alarmQueueLength = alarmQueue_read.size() / sizeof(uint16_t);
        eventsIDArray[ID_posn - 1] = e->id;
        alarmQueue_read.close();
        activeEvents.close();

        File alarmQueue_write = LittleFS.open("/alarmQueue.bin", "w");
        alarmQueue_write.write((uint8_t*)eventsIDArray, (alarmQueueLength + 1) * sizeof(uint16_t));
        alarmQueue_write.close();
      } else {
        memset(eventsIDArray, 0, sizeof(eventsIDArray));
        uint16_t alarmQueueLength = alarmQueue_read.size() / sizeof(uint16_t);
        eventsIDArray[0] = e->id;
        alarmQueue_read.close();
        activeEvents.close();

        File alarmQueue_write = LittleFS.open("/alarmQueue.bin", "w");
        alarmQueue_write.write((uint8_t*)eventsIDArray, (alarmQueueLength + 1) * sizeof(uint16_t));
        alarmQueue_write.close();
      }
    } else {
      Serial.println("Event is not today.");
    }
  }
};

#endif