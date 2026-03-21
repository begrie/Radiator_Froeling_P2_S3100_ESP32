#ifndef __ALARM_H__
#define __ALARM_H__

/******************************************************************************
 * @brief   Zentraler Alarm-Manager fuer Froeling P2 ESP32 Ueberwachungsbox
 *
 * Prioritaetskonzept (hoehere Zahl = kritischer):
 *
 *   6 LEAK_WATER     Dauerton        Leckwasser detektiert               [HEIZUNG KRITISCH]
 *   5 BOILER_OVERHEAT Stotter-4x     Kesseltemperatur ueberschritten     [HEIZUNG KRITISCH]
 *   4 BOILER_FAULT   700ms-Takt      Heizungsfehlermeldung vom Geraet    [HEIZUNG KRITISCH]
 *   3 BOILER_SILENT  Morse "S" (···) Heizung sendet keine Daten mehr     [HEIZUNG WARNUNG]
 *   2 ESP_FS_ERROR   Rattern 3s      Dateisystem-Problem                  [ESP-SYSTEM]
 *   1 ESP_WIFI_LOST  2 langsame Beeps WiFi-Verbindung unterbrochen        [ESP-SYSTEM]
 *   0 (keiner)       --              Kein aktiver Alarm
 *
 * Nur der Alarm mit der hoechsten Prioritaet ertönt.
 * Heizungsalarme (Prio>=3) ertönen dauerhaft bis Quit-Taste oder Ursache weg.
 * ESP_FS_ERROR ertönt 3 Sekunden und wird zyklisch wiederholt, solange aktiv.
 * WiFi-Ausfall wird akustisch signalisiert (2 langsame Beeps, zyklisch).
 *
 * Quit-Taste (QUIT_BUZZER_BUTTON_PIN) stoppt den aktiven Buzzer temporaer.
 * Wenn die Ursache weiterbesteht, setzt der Alarm nach QUIT_GRACE_PERIOD_S
 * Sekunden wieder ein (nur bei Heizungsalarmen Prio>=3).
 ******************************************************************************/

#include "config.h"

namespace radiator
{
    namespace alarmcfg
    {
        // Quit behavior
        static constexpr int QUIT_GRACE_PERIOD_S = 60;

        // LEVEL 6: LEAK_WATER
        static constexpr int LEAK_WATER_HOLD_STEP_MS = 100;
        static constexpr int LEAK_WATER_HOLD_STEPS = 100;

        // LEVEL 5: BOILER_OVERHEAT
        static constexpr int BOILER_OVERHEAT_PULSE_COUNT = 4;
        static constexpr int BOILER_OVERHEAT_PULSE_ON_MS = 120;
        static constexpr int BOILER_OVERHEAT_PULSE_OFF_MS = 120;
        static constexpr int BOILER_OVERHEAT_BLOCK_PAUSE_MS = 1000;

        // LEVEL 4: BOILER_FAULT
        static constexpr int BOILER_FAULT_ON_MS = 700;
        static constexpr int BOILER_FAULT_OFF_MS = 700;

        // LEVEL 3: BOILER_SILENT (Morse S)
        static constexpr int BOILER_SILENT_PULSE_COUNT = 3;
        static constexpr int BOILER_SILENT_PULSE_ON_MS = 150;
        static constexpr int BOILER_SILENT_PULSE_OFF_MS = 150;
        static constexpr int BOILER_SILENT_BLOCK_PAUSE_MS = 4000;

        // LEVEL 2: ESP_FS_ERROR
        static constexpr int ESP_FS_ERROR_PULSE_COUNT = 30;
        static constexpr int ESP_FS_ERROR_PULSE_ON_MS = 50;
        static constexpr int ESP_FS_ERROR_PULSE_OFF_MS = 50;
        static constexpr int ESP_FS_ERROR_BLOCK_PAUSE_MS = 300000;

        // LEVEL 1: ESP_WIFI_LOST
        static constexpr int ESP_WIFI_LOST_PULSE_COUNT = 2;
        static constexpr int ESP_WIFI_LOST_PULSE_ON_MS = 600;
        static constexpr int ESP_WIFI_LOST_INTER_PULSE_OFF_MS = 300;
        static constexpr int ESP_WIFI_LOST_BLOCK_PAUSE_MS = 60000;

        // Status beep: WiFi connected (non-alarm)
        static constexpr int WIFI_CONNECTED_STATUS_BEEP_COUNT = 5;
        static constexpr int WIFI_CONNECTED_STATUS_ON_MS = 80;
        static constexpr int WIFI_CONNECTED_STATUS_OFF_MS = 80;
        static constexpr int WIFI_CONNECTED_STATUS_PRE_DELAY_MS = 300;

        // Status beep: restart warning (non-alarm)
        static constexpr int RESTART_STATUS_BEEP_COUNT = 3;
        static constexpr int RESTART_STATUS_ON_MS = 500;
        static constexpr int RESTART_STATUS_OFF_MS = 500;
    } // namespace alarmcfg

    class AlarmManager
    {
    public:
        // Alarm levels - higher value = higher priority
        enum class Level : uint8_t
        {
            NONE = 0,
            ESP_WIFI_LOST = 1,   // ESP system: WiFi connection lost
            ESP_FS_ERROR = 2,    // ESP system: filesystem problem
            BOILER_SILENT = 3,   // Heating warning: no data from boiler for too long
            BOILER_FAULT = 4,    // Heating critical: fault message from boiler
            BOILER_OVERHEAT = 5, // Heating critical: temperature limit exceeded
            LEAK_WATER = 6       // Heating critical: leak water detected
        };

        /**
         * @brief Raise an alarm. If this alarm has higher or equal priority than the
         *        current active alarm, the buzzer pattern is updated immediately.
         * @param level   The alarm level to raise
         * @param message Short description for MQTT/log (will be published on raise)
         */
        static void raise(Level level, const char *message = nullptr);

        /**
         * @brief Clear an alarm. If this was the active alarm, the next-highest
         *        pending alarm takes over (or buzzer stops if none).
         */
        static void clear(Level level);

        /**
         * @brief Returns whether the given level is currently raised.
         */
        static bool isActive(Level level);

        /**
         * @brief Returns the currently active (highest priority) alarm level.
         */
        static Level activeLevel();

        /**
         * @brief Must be called once in setup() to start the buzzer task.
         */
        static void init();

    private:
        static volatile uint8_t activeLevelsMask; // bitmask of currently raised levels
        static TaskHandle_t buzzerTaskHandle;
        static volatile bool quitPressed;

        static void buzzerTask(void *param);
        static void playPattern(Level level);
        static void attachQuitInterrupt();
    };

} // namespace radiator

#endif // __ALARM_H__
