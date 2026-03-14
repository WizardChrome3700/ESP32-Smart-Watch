* Implement hourly repeat in `handleClient(wifi client)` inside the `/addEvent(/POST)` and disable `Reminder` drop down in html when `hourly` repeat is selected.
* Change `/export` to `/getEvents` and verify that the events file is downloadable.
* Implement `/import` functionality in microcontroller to save events in to the memory.
    * implement duplicate identification and avoidance.
* eventFlags: 2-bit flag, 3 states 
    * For events with reminder start - 00
        * State when reminder is overshot but alarm pending - 10
        * state when both are overshot - 11
    * For events without reminder - 10
        * State when reminder is overshot but alarm pending - NA
        * state when both are overshot - 11
* Complete alarm queue generator function {alarmQueueGen()}
    * Update and storing the alarmQueue.bin file 