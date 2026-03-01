# Smart Watch Project

## Hardware Components
* ESP32-C6: Main brains of the smart watch
* RTC3231 module: Real time clock for time and event tracking
* 128x64 SPI based OLED display: Display module for GUI
* 3.7V 1000mAh LiPo cell: Power source
* 1S LiPo cell charging module: USB C-Type charging module
* Vibration motor: To alert user about reminders
* BJT: To drive the motor with the help of ESP32 and not damage it's pins
* flyback diode: To prevent back current from flowing into the BJT and protect the driver circuit.

## Software Components
* Calendar.h: Calendar and Event tracker application
* OLED.h: OLED display driver
* RTC_DS3231.h: RTC module driver

## Workflow: To be added...

---

## Calendar.h
This header file contains the implementation of the Calendar application. Its purpose is to track events in memory and remind the user.
* It uses a Wifi Server for adding events via mobile, computer, or iPad.
* Events are added through a website hosted on the server.
* It uses the LittleFS library to store events in the microcontroller EEPROM Flash memory.
* LittleFS adds files to the EEPROM as a modifiable filesystem.

### Declarations
* **`MAX_EVENTS_PER_DAY`**: Maximum events in a day.
* **`WARNING_THRESHOLD`**: Issues a screen warning when total events cross this memory threshold.
* **`MAX_EVENTS`**: Total events tracked by the application.

---

## Data Structures

### Time Struct
Defines a standardized 7-byte timestamp for data logging.
* **`hour` (`uint8_t`)**: Hour of the day (0-23).
* **`min` (`uint8_t`)**: Minute (0-59).
* **`sec` (`uint8_t`)**: Second (0-59).
* **`date` (`uint8_t`)**: Day of the month.
* **`month` (`uint8_t`)**: Month of the year.
* **`year` (`uint16_t`)**: 4-digit year format (e.g., 2026).

### FileHeader Struct
Stores global file metadata.
* **`version` (`int`)**: Keeps track of application version.
* **`totalEvents` (`int`)**: Total count of currently stored records.
* **`currentEventID` (`uint16_t`)**: Unique identifier for the most recent event.

### Event Struct
Boilerplate for all tracked events.
* **`id` (`uint16_t`)**: Ascending numeric ID assigned upon addition.
* **`name` (`char[16]`)**: Event name displayed on the screen.
* **`details` (`char[32]`)**: Description of the event.
* **`year` (`uint8_t`)**: Year of the event.
* **`month` (`uint8_t`)**: Month of the event.
* **`day` (`uint8_t`)**: Day of the event.
* **`hour` (`uint8_t`)**: Hour of the event.
* **`minute` (`uint8_t`)**: Minute of the event.
* **`flags1` (`uint8_t`)**: Flags for repeatability, reminder type, etc.
* **`flags2` (`uint8_t`)**: Flags for repeatability, reminder type, etc.
* **`repeatDays` (`uint8_t`)**: I am not sure.
* **`deletedAt` (`uint32_t`)**: I am not sure.

---

## Calendar Class

### Public Members
* **`Calendar Constructor`**: Creates a wifi server instance at port 80 to host the website.
* **`begin()`**: Starts application, initializes LittleFS, and initializes `FileHeader` (version, 0 total events, 0 currentEventID).
    **Files Created by `begin()`**:
    * **`calendarHeader.bin`**: Stores the `calendarHeader` struct value. Value is preserved if file exists. Need to add else condition if file doesn't exist.
    * **`active.bin`**: Stores a contiguous list of tracked events. Value is preserved if file exists.`
    * **`alarmqueue.bin`**: Stores ID of events which come from 00:00 to 23:59 of present day.
* **`setup()`**: It creates a wifi hotspot to which a device with wifi support can connect with a password.
* **`calendarLoop()`**: It checks for requests from client on the wifi server created.
* **`handleClient(WifiClient client)`**: It handles /GET and /POST requestion from and to the client. These are of the following types:-
    * **`/setTime`(/GET)**: It takes time from the client device and updates watche's RTC time. *The fetch and decode section is implemented but the integration with RTC_DS3231 library to set RTC time remains.*
    * **`/getTime`(/GET)**: It takes time from RTC and updates time on the website being accessed by the client. *It also needs to be integrated with getTime function in RTC_DS3231 library.*
    * **`/addEvent`(/POST)**: 
        * It checks if total number of events is reaching `WARNING_THRESHOLD`  or `MAX_EVENTS`. If it is in the middle or less than `WARNING_THRESHOLD` then proceeds normally with caution for exceeding `WARNING_THRESHOLD`.
        * It then parses the json sent by the client and stores them into variables of respective types. 
        * It check the freeBytes against size of event to see if an event can be added. [seems reduntant].
        * It creates an instance of Event struct and stores values in it's respective field.
            * flag1:
                * 0-2: reminder type 
                * 3: isActive flag which tells that an event needs to be tracked.[seems redundant]
            * flag2:
                * 0-2: repeatType
                * 3-5: repeat interval
            * repeatDays: Days on which it repeats for custom repeat interval.
        * It appends the event in the `active.bin` file.
        * It sends an alert to the client about the event added to the application.
        * It adds the event to alarmQueue.bin it it falls on today 00:00 to 23:59.
    * **`/deleteEvent`(/GET)**:
        * It extracts the eventID of the event being requested to be deleted and invokes `deleteEventByID` function to delete the event from memory.
    * **`/getSystemInfo`(/GET)**:
        * It obtains the total memory occupied, total free memory and total memory available.
    * **`/`(/GET)**: It sends the website to the client when a client connects to our wifi and requests the host url `192.168.4.1`(maybe).
* **`deleteEventByID(eventID)`**:
    * It reads the `calendarHeader.bin` file to get total number of events.
    * It performs a replace delete by assigning posterior events into current event pointer from the event to be deleted to the end. (Event buffer maybe needs to be assigned before this.)
    * It then assigns the last element corresponding to the last event as an empty event.
    * It updates the `calendarHeader` for reduced number of events and writes the updated list of events to `active.bin` file.
    * Then it removes the event ID from the `alarmqueue.bin` if it was scheduled for that day.
* **`eventsCleanUp()`**:
    * It deletes event if it is of `no reminder` type. It should be of no repeat type.
* **`alarmQueue()`**:
    * It reads the `calendarHeader.bin` file to get total number of events and uses the entire list of events in `active.bin` file to check if it falls current day and adds those events to the alarmQueue.
    * It is supposed to be run once a day to add events at the start of the day into the alarmQueue.

### Private Members and State
* **`server` (`WiFiServer`)**: Handles incoming TCP connections.
* **`port` (`uint16_t`)**: Port number the server listens on.
* **`calendarHeader` (`FileHeader`)**: Stores global file metadata.
* **`cTime` (`Time`)**: Represents the current system time.
* **`eventsArray` (`Event[MAX_EVENTS]`)**: In-memory cache for event objects.
* **`eventsIDArray` (`uint16_t[MAX_EVENTS_PER_DAY]`)**: Buffer managing scheduled IDs for the current day.

### Methods

**Bitwise Data Extraction**
Parses `flags1` and `flags2` from an Event to retrieve settings.
* **`extractReminderType`**: Returns bits 2–0 of flags1 (e.g., 1 hour before, 1 day before).
* **`extractRepeatType`**: Returns bits 2–0 of flags2 (e.g., Daily, Weekly, Monthly).
* **`extractRepeatInterval`**: Extracts bits 5–3 of flags2 to find repetition frequency.
* **`extractEventFlags`**: Yet to be figured.

**Event State Logic**
* **`isEventActive`**: Checks bit 0 of event flags to see if enabled.
* **`isReminderSent`**: Checks bit 1 of event flags to prevent duplicates.
* **`extractEventState`**: Returns bits 3–2 of event flags for status (00: Not reached, 01: Reminder reached, 10: Alarm reached if ReminderType is 0, 11: Both reached).

**Time and Calendar Utilities**
* **`julianDay`**: Converts Year/Month/Day into a Julian Day Number (JDN).
* **`julianToDate`**: Reverses JDN back to a standard Time struct.
* **`getEventDateTime`**: Maps individual Event fields into a Time struct.
* **`applyReminderOffset`**: Adjusts event time based on ReminderType (1 hour, 1 day, 1 week, 1 month) using Julian math for rollovers.
* **`compareDateTime`**: Chronologically compares two Time structs (1 if A > B, -1 if A < B, 0 if equal).

**File and Queue Management**
* **`findEventByID`**: Opens `active.bin` via LittleFS, uses `seek()` to jump to ID position, and reads bytes into an Event struct.
* **`Add2AlarmQueue` (Core)**: Determines if an event should trigger within the next 24 hours.
* **`Add2AlarmQueue` (Filter)**: Adds event only if calculated offset time is between "Today" and "Tomorrow".
* **`Add2AlarmQueue` (Sorting)**: Reads `alarmQueue.bin` IDs, finds chronological insertion point, and shifts IDs to maintain sorting.
* **`Add2AlarmQueue` (Persistence)**: Writes updated sorted ID array to `/alarmQueue.bin`.