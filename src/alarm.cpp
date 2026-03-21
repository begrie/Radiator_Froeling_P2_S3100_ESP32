/******************************************************************************
 * Name:        alarm.cpp
 * @brief       Zentraler Alarm-Manager: einzige Instanz, die auf BUZZER_PIN schreibt.
 *
 * Prioritaeten und Muster: siehe alarm.h
 *
 * Interne Logik:
 *   - activeLevelsMask: jedes Bit repraesentiert ein Level (Bit 1..6)
 *   - Der Buzzer-Task luft endlos und spielt immer das Muster des hoechsten
 *     aktiven Levels.
 *   - raise()/clear() setzen/loeschen das jeweilige Bit und wecken den Task.
 *   - Quit-Taste: setzt quitPressed=true, Task pausiert QUIT_GRACE_PERIOD_S
 *     Sekunden; danach nur Heizungsalarme (Prio>=3) wieder aktiv.
 ******************************************************************************/

#include "alarm.h"
#include "debug.h"
#include "networking.h"

namespace radiator
{
    static portMUX_TYPE alarmMaskMux = portMUX_INITIALIZER_UNLOCKED;

    // ---- statische Membervariablen ----
    volatile uint8_t AlarmManager::activeLevelsMask = 0;
    TaskHandle_t AlarmManager::buzzerTaskHandle = nullptr;
    volatile bool AlarmManager::quitPressed = false;

    // ---- init ----------------------------------------------------------------
    /*********************************************************************
     * @brief 	Initializes BUZZER and QUIT pins, creates the buzzer FreeRTOS task.
     *          Must be called once in setup() before any alarm can be raised.
     * @return 	void
     *********************************************************************/
    void AlarmManager::init()
    {
        pinMode(BUZZER_PIN, OUTPUT);
        digitalWrite(BUZZER_PIN, LOW);
        pinMode(QUIT_BUZZER_BUTTON_PIN, INPUT_PULLUP);

        xTaskCreatePinnedToCore(
            buzzerTask,
            "xTaskAlarmBuzzer",
            3072,
            nullptr,
            uxTaskPriorityGet(nullptr),
            &buzzerTaskHandle,
            1);
    }

    // ---- raise ---------------------------------------------------------------
    /*********************************************************************
     * @brief 	Raise an alarm level.
     *          If this level is new (not yet raised), the optional message is
     *          published via MQTT/log once to avoid message flooding.
     *          The buzzer task is notified to re-evaluate the active pattern.
     * @param 	level    Alarm level to activate
     * @param 	message  Description for MQTT/syslog; nullptr = no message
     * @return 	void
     *********************************************************************/
    void AlarmManager::raise(Level level, const char *message)
    {
        if (level == Level::NONE)
            return;

        const uint8_t bit = (1u << static_cast<uint8_t>(level));
        bool wasNewAlarm = false;

        portENTER_CRITICAL(&alarmMaskMux);
        wasNewAlarm = !(activeLevelsMask & bit);
        activeLevelsMask |= bit;
        portEXIT_CRITICAL(&alarmMaskMux);

        if (message && wasNewAlarm) // publish only on first raise to avoid MQTT flood
        {
            std::string msg = message;
            NetworkHandler::publishToMQTT(msg, MQTT_SUBTOPIC_SYSLOG);
            RADIATOR_LOG_ERROR(getMillisAndTime() << "ALARM raised [" << static_cast<int>(level) << "]: " << msg << std::endl;)
        }

        if (buzzerTaskHandle)
            xTaskNotifyGive(buzzerTaskHandle); // wake buzzer task
    }

    // ---- clear ---------------------------------------------------------------
    /*********************************************************************
     * @brief 	Clear an alarm level.
     *          If this was the highest active alarm, the next lower active alarm
     *          takes over, or the buzzer stops if no alarm remains.
     * @param 	level    Alarm level to deactivate
     * @return 	void
     *********************************************************************/
    void AlarmManager::clear(Level level)
    {
        if (level == Level::NONE)
            return;

        portENTER_CRITICAL(&alarmMaskMux);
        activeLevelsMask &= ~(1u << static_cast<uint8_t>(level));
        portEXIT_CRITICAL(&alarmMaskMux);

        if (buzzerTaskHandle)
            xTaskNotifyGive(buzzerTaskHandle); // buzzer task re-evaluates
    }

    // ---- isActive ------------------------------------------------------------
    /*********************************************************************
     * @brief 	Check if a specific alarm level is currently active.
     * @param 	level    Alarm level to query
     * @return 	true if the given level is currently raised, false otherwise
     *********************************************************************/
    bool AlarmManager::isActive(Level level)
    {
        bool isSet = false;
        portENTER_CRITICAL(&alarmMaskMux);
        isSet = (activeLevelsMask & (1u << static_cast<uint8_t>(level))) != 0;
        portEXIT_CRITICAL(&alarmMaskMux);
        return isSet;
    }

    // ---- activeLevel ---------------------------------------------------------
    /*********************************************************************
     * @brief 	Returns the currently active alarm with the highest priority.
     * @return 	Highest active Level, or Level::NONE if no alarm is active
     *********************************************************************/
    AlarmManager::Level AlarmManager::activeLevel()
    {
        uint8_t maskSnapshot = 0;
        portENTER_CRITICAL(&alarmMaskMux);
        maskSnapshot = activeLevelsMask;
        portEXIT_CRITICAL(&alarmMaskMux);

        for (int i = static_cast<int>(Level::LEAK_WATER); i >= static_cast<int>(Level::ESP_WIFI_LOST); --i)
        {
            if (maskSnapshot & (1u << i))
                return static_cast<Level>(i);
        }
        return Level::NONE;
    }

    // ---- attachQuitInterrupt -------------------------------------------------
    /*********************************************************************
     * @brief 	Attaches a falling-edge interrupt on QUIT_BUZZER_BUTTON_PIN.
     *          The interrupt auto-detaches after the first trigger and sets
     *          quitPressed = true.
     * @return 	void
     *********************************************************************/
    void AlarmManager::attachQuitInterrupt()
    {
        quitPressed = false;
        detachInterrupt(QUIT_BUZZER_BUTTON_PIN);
        attachInterrupt(
            QUIT_BUZZER_BUTTON_PIN,
            []() IRAM_ATTR
            { AlarmManager::quitPressed = true; detachInterrupt(QUIT_BUZZER_BUTTON_PIN); },
            FALLING);
    }

    // ---- playPattern ---------------------------------------------------------
    /*********************************************************************
     * @brief 	Plays one cycle of the buzzer pattern for the given alarm level.
     *          Returns after one full pattern cycle or early if quitPressed becomes true.
     * @param 	level    Alarm level whose pattern to play
     * @return 	void
     *********************************************************************/
    // Plays one "cycle" of the buzzer pattern for the given level.
    // Returns when the cycle is complete or quitPressed becomes true.
    void AlarmManager::playPattern(Level level)
    {
        switch (level)
        {
        // --------------------------------------------------------
        // LEVEL 6: LEAK_WATER – Dauerton
        // --------------------------------------------------------
        case Level::LEAK_WATER:
            digitalWrite(BUZZER_PIN, HIGH);
            // Runs until quit or alarm cleared; we just keep a short loop
            for (int i = 0; i < alarmcfg::LEAK_WATER_HOLD_STEPS && !quitPressed && isActive(level); i++)
                vTaskDelay(pdMS_TO_TICKS(alarmcfg::LEAK_WATER_HOLD_STEP_MS));
            break;

        // --------------------------------------------------------
        // LEVEL 5: BOILER_OVERHEAT – Stotter: 4x kurz + 1s Pause
        // --------------------------------------------------------
        case Level::BOILER_OVERHEAT:
            for (int i = 0; i < alarmcfg::BOILER_OVERHEAT_PULSE_COUNT && !quitPressed && isActive(level); i++)
            {
                digitalWrite(BUZZER_PIN, HIGH);
                vTaskDelay(pdMS_TO_TICKS(alarmcfg::BOILER_OVERHEAT_PULSE_ON_MS));
                digitalWrite(BUZZER_PIN, LOW);
                vTaskDelay(pdMS_TO_TICKS(alarmcfg::BOILER_OVERHEAT_PULSE_OFF_MS));
            }
            if (!quitPressed && isActive(level))
                vTaskDelay(pdMS_TO_TICKS(alarmcfg::BOILER_OVERHEAT_BLOCK_PAUSE_MS));
            break;

        // --------------------------------------------------------
        // LEVEL 4: BOILER_FAULT – 700ms EIN / 700ms AUS
        // --------------------------------------------------------
        case Level::BOILER_FAULT:
            digitalWrite(BUZZER_PIN, HIGH);
            vTaskDelay(pdMS_TO_TICKS(alarmcfg::BOILER_FAULT_ON_MS));
            digitalWrite(BUZZER_PIN, LOW);
            vTaskDelay(pdMS_TO_TICKS(alarmcfg::BOILER_FAULT_OFF_MS));
            break;

        // --------------------------------------------------------
        // LEVEL 3: BOILER_SILENT – Morse "S" (···) alle 4s
        //   Heizung schweigt – jemand soll NACHSCHAUEN, muss aber
        //   nicht sofort reagieren wie bei kritischem Fehler
        // --------------------------------------------------------
        case Level::BOILER_SILENT:
            for (int i = 0; i < alarmcfg::BOILER_SILENT_PULSE_COUNT && !quitPressed && isActive(level); i++)
            {
                digitalWrite(BUZZER_PIN, HIGH);
                vTaskDelay(pdMS_TO_TICKS(alarmcfg::BOILER_SILENT_PULSE_ON_MS));
                digitalWrite(BUZZER_PIN, LOW);
                vTaskDelay(pdMS_TO_TICKS(alarmcfg::BOILER_SILENT_PULSE_OFF_MS));
            }
            if (!quitPressed && isActive(level))
                vTaskDelay(pdMS_TO_TICKS(alarmcfg::BOILER_SILENT_BLOCK_PAUSE_MS));
            break;

        // --------------------------------------------------------
        // LEVEL 1: ESP_WIFI_LOST – 2 langsame Beeps + 60s Pause
        //   WiFi nicht verfuegbar: kein MQTT moeglich, ESP laeuft weiter.
        // --------------------------------------------------------
        case Level::ESP_WIFI_LOST:
            for (int i = 0; i < alarmcfg::ESP_WIFI_LOST_PULSE_COUNT && !quitPressed && isActive(level); i++)
            {
                digitalWrite(BUZZER_PIN, HIGH);
                vTaskDelay(pdMS_TO_TICKS(alarmcfg::ESP_WIFI_LOST_PULSE_ON_MS));
                digitalWrite(BUZZER_PIN, LOW);
                if (i < (alarmcfg::ESP_WIFI_LOST_PULSE_COUNT - 1) && !quitPressed && isActive(level))
                    vTaskDelay(pdMS_TO_TICKS(alarmcfg::ESP_WIFI_LOST_INTER_PULSE_OFF_MS));
            }
            if (!quitPressed && isActive(level))
                vTaskDelay(pdMS_TO_TICKS(alarmcfg::ESP_WIFI_LOST_BLOCK_PAUSE_MS));
            break;

        // --------------------------------------------------------
        // LEVEL 2: ESP_FS_ERROR – 50ms Rattern ueber 3s, alle 5 Minuten wiederholen
        // --------------------------------------------------------
        case Level::ESP_FS_ERROR:
        {
            for (int i = 0; i < alarmcfg::ESP_FS_ERROR_PULSE_COUNT && !quitPressed; i++)
            {
                digitalWrite(BUZZER_PIN, HIGH);
                vTaskDelay(pdMS_TO_TICKS(alarmcfg::ESP_FS_ERROR_PULSE_ON_MS));
                digitalWrite(BUZZER_PIN, LOW);
                vTaskDelay(pdMS_TO_TICKS(alarmcfg::ESP_FS_ERROR_PULSE_OFF_MS));
            }
            // 5 Minuten Pause, dann erneut wenn noch aktiv
            if (!quitPressed && isActive(level))
                vTaskDelay(pdMS_TO_TICKS(alarmcfg::ESP_FS_ERROR_BLOCK_PAUSE_MS));
            break;
        }

        default:
            vTaskDelay(pdMS_TO_TICKS(200));
            break;
        }
    }

    // ---- buzzerTask ----------------------------------------------------------
    /*********************************************************************
     * @brief 	FreeRTOS task: endless loop that plays the highest active alarm
     *          pattern. Sleeps indefinitely (portMAX_DELAY) when no alarm active.
     *          Handles quit-button grace period for heating alarms (Prio >= BOILER_SILENT):
     *          after pressing Quit the alarm is silenced for QUIT_GRACE_PERIOD_S;
     *          it resumes automatically if the root cause has not been cleared.
     *          A higher-priority alarm always breaks the grace period immediately.
     * @param 	param    Unused (FreeRTOS task parameter convention)
     * @return 	void (task never returns)
     *********************************************************************/
    void AlarmManager::buzzerTask(void *param)
    {
        while (true)
        {
            Level current = activeLevel();

            if (current == Level::NONE)
            {
                digitalWrite(BUZZER_PIN, LOW);
                // Sleep until notified by raise() or clear()
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                continue;
            }

            // Attach quit interrupt before playing pattern
            attachQuitInterrupt();
            digitalWrite(BUZZER_PIN, LOW); // clean start

            playPattern(current);

            digitalWrite(BUZZER_PIN, LOW);

            // Check if user pressed quit
            if (quitPressed)
            {
                quitPressed = false;
                const Level levelAtQuit = current;

                // Inform via MQTT that alarm was acknowledged
                NetworkHandler::publishToMQTT(
                    getMillisAndTime() + "Alarm-Buzzer gestoppt (Quit-Taste) fuer Level " + std::to_string(static_cast<int>(levelAtQuit)),
                    MQTT_SUBTOPIC_SYSLOG);

                // For heating alarms (Prio>=3): grace period, then resume if still active
                if (static_cast<uint8_t>(levelAtQuit) >= static_cast<uint8_t>(Level::BOILER_SILENT))
                {
                    // Sleep for grace period in short chunks so we can react to higher-prio alarms
                    const int graceSlices = alarmcfg::QUIT_GRACE_PERIOD_S * 10; // 100ms slices
                    for (int i = 0; i < graceSlices; i++)
                    {
                        vTaskDelay(pdMS_TO_TICKS(100));
                        // A *higher* priority alarm overrides the grace period immediately
                        if (static_cast<uint8_t>(activeLevel()) > static_cast<uint8_t>(levelAtQuit))
                            break;
                    }
                }
                // For ESP alarms (Prio 1-2): no grace period, pattern continues cyclically.
            }
            else
            {
                // No quit: yield briefly for other tasks between pattern cycles
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
    }

} // namespace radiator
