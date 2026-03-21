/******************************************************************************
 * Name:		main.cpp
 * @brief   Überwachungstool für Fröling P2 / Lambdatronic S 3100
 *          -> hier: für ESP32
 *          -> Output auf console, flash-filesystem oder SD-card oder/und MQTT
 *          -> rudimentärer Zugriff per WiFi Async Server
 *          -> IDE: platformio
 * Created:	01.09.2022 (ESP32-adaption)
 * Author:	Original (for Raspberry Pi): Daniel Höpfl
 *          -> https://github.com/dhoepfl/Radiator
 *          Adaption to ESP32: Bernd Griesbach
 *          -> https://github.com/begrie/Radiator_Froeling_S3100_Hoepfl/tree/ESP32
 ******************************************************************************/

/***********************
 *      INCLUDES
 ***********************/
#include "config.h"
#include "debug.h"
#include "device.h"
#include "surveillance.h"
#include "output.h"
#include "files.h"
#include "externalsensors.h"
#include "alarm.h"

/***********************
 *      DEFINES
 ***********************/
// see config.h

/***********************
 *      MACROS
 ***********************/
// see config.h

/***********************
 * FORWARD DECLARATIONS
 ***********************/

/***********************
 * GLOBAL DEFINITIONS
 ***********************/
radiator::NetworkHandler netHandler;
radiator::OutputHandler *ptrOutHandler;

/*********************************************************************
 * @brief 	Setup to start everything
 * @param 	void
 * @return 	void
 *********************************************************************/
void setup()
{
    // int *p = nullptr;
    // *p = 42; // absichtlicher Crash

    Serial.begin(115200);
    delay(1000); // give serial monitor time to connect
    Serial.println("Start setup...");

    bool outputToConsole = OUTPUT_TO_CONSOLE;
    bool outputToMQTT = OUTPUT_TO_MQTT && USE_WIFI; // is turned off without WiFi

    debug_level = D_DEBUG_LEVEL; // var debug_level is globally defined in debug.cpp

    // initialisation of the filesystem at first to minimize heap fragmentation
    std::string pathnameToDataDirectory =
        radiator::FilesystemHandler::initFilesystem(DATA_DIRECTORY); // DATA_DIRECTORY is completed with mountpoint of the filesystem
                                                                     // (/sd/dataDirectory or /littlefs/..., /spiffs/...)

    if (pathnameToDataDirectory.empty())
    {
        std::cout << getMillisAndTime() << "ERROR MOUNTING FILESYSTEM !!! -> Instead: data output to console (std::cout)" << std::endl;
        outputToConsole = true;
    }
    else // filesystem successfull mounted
    {
        // FS_Filehelper::listDir("/", 255, false, FILESYSTEM_TO_USE);

#if REDIRECT_STD_ERR_TO_SYSLOG_FILE
        if (!pathnameToDataDirectory.empty())
            radiator::FilesystemHandler::initRedirectStdErrToSyslogFile();
#endif
    }

    if (!OUTPUT_TO_FILE)
        pathnameToDataDirectory = ""; // empty pathname indicates "no file output" for constuctor of OutputHandler

    if (USE_WIFI)
        outputToMQTT = netHandler.init(outputToMQTT, START_WEBSERVER); // on init-failure -> turn off MQTT

    radiator::AlarmManager::init(); // must be called before any alarm can be raised

#if USE_EXTERNAL_SENSORS
    radiator::ExternalSensors::initExternalSensors();
#endif

    ptrOutHandler = new radiator::OutputHandler(pathnameToDataDirectory, // with empty pathnameToDataDirectory -> no output to file
                                                outputToConsole,
                                                outputToMQTT);

    // ptrOutHandler->setSystemTimeFromRadiatorData(2024, 9, 8, 7, 55, 44);

    RADIATOR_LOG_INFO("\n" << getMillisAndTime()
                           << "***************************************** END STARTUP OF ESP32 *****************************************" << std::endl;)
}

/*********************************************************************
 * @brief 	Loop function
 * @param 	void
 * @return 	void
 *********************************************************************/
void loop()
{
    std::string message;
    message.reserve(800); // an attempt to avoid heap fragmentation
    Ticker tickerConnectToRadiatorInfo;

    // Track how long the boiler has been unreachable.
    // BOILER_SILENT_TIMEOUT_S is defined in config.h.
    static ulong boilerLostSinceMs = 0;
    static bool boilerWasConnected = false;
    static volatile bool boilerConnectEvent = false;
    static portMUX_TYPE boilerStateMux = portMUX_INITIALIZER_UNLOCKED;

    try
    {
        message = getMillisAndTime() + "##### Start connecting to Froeling P2/S3100 ##### \n";
        netHandler.publishToMQTT(message, MQTT_SUBTOPIC_SYSLOG);
        RADIATOR_LOG_WARN(message << std::endl;)
        if (REDIRECT_STD_ERR_TO_SYSLOG_FILE) // for better user info also to console
            std::cout << message;

        // Start BOILER_SILENT watchdog: if surveillance.main_loop() returns without
        // ever having received data, we count waiting time.
        if (!boilerWasConnected && boilerLostSinceMs == 0)
            boilerLostSinceMs = millis();

        tickerConnectToRadiatorInfo.once_ms(
            T_TIMEOUT_BETWEEN_TRANSFERS_MS + 500,
            []()
            {
                // Connection established: clear BOILER_SILENT alarm and signal event.
                radiator::AlarmManager::clear(radiator::AlarmManager::Level::BOILER_SILENT);

                portENTER_CRITICAL(&boilerStateMux);
                boilerConnectEvent = true;
                portEXIT_CRITICAL(&boilerStateMux);

                char msg[80];
                snprintf(msg, sizeof(msg), "%s ##### CONNECTED to Froeling P2/S3100 #####", getMillisAndTime().c_str());
                netHandler.publishToMQTT(msg, MQTT_SUBTOPIC_SYSLOG);
                RADIATOR_LOG_WARN(msg << std::endl;)
                if (REDIRECT_STD_ERR_TO_SYSLOG_FILE)
                    std::cout << msg << std::endl;
            });

        radiator::Surveillance surveillance(S_SERIAL_TO_RADIATOR, T_TIMEOUT_BETWEEN_TRANSFERS_MS, *ptrOutHandler);
        surveillance.main_loop();

        tickerConnectToRadiatorInfo.detach();

        // Apply connection state transition only in loop context (race-free).
        bool gotConnectEvent = false;
        portENTER_CRITICAL(&boilerStateMux);
        gotConnectEvent = boilerConnectEvent;
        boilerConnectEvent = false;
        portEXIT_CRITICAL(&boilerStateMux);

        if (gotConnectEvent)
        {
            boilerWasConnected = true;
            boilerLostSinceMs = 0;
        }
        // Connection dropped after main_loop returned. Re-arm BOILER_SILENT timing
        // so later disconnects are also detected after previous successful sessions.
        else if (boilerWasConnected)
        {
            boilerWasConnected = false;
            boilerLostSinceMs = millis();
        }
        else if (boilerLostSinceMs == 0)
        {
            boilerLostSinceMs = millis();
        }

        message = getMillisAndTime() + "##### Cannot establish connection to Froeling P2/S3100 -> retry in 10s ##### \n";
        netHandler.publishToMQTT(message, MQTT_SUBTOPIC_SYSLOG);
        RADIATOR_LOG_WARN(message << std::endl;)
        if (REDIRECT_STD_ERR_TO_SYSLOG_FILE) // for better user info also to console
            std::cout << message;

        // Check if boiler has been silent long enough to raise BOILER_SILENT alarm.
        if (boilerLostSinceMs > 0 &&
            (millis() - boilerLostSinceMs) >= (BOILER_SILENT_TIMEOUT_S * 1000UL))
        {
            radiator::AlarmManager::raise(radiator::AlarmManager::Level::BOILER_SILENT,
                                          "Heizung sendet keine Daten - bitte pruefen");
        }

        sleep(10);
    }
    catch (const char *error)
    {
        const std::string errorText = (error ? error : "<null>");
        message = getMillisAndTime() +
                  " ms: !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                  "!!!!! error= " +
                  errorText +
                  "!!!!! Failed to open serial device or filesystem -> restarting ESP32 in 10s !!!!!\n"
                  "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
        netHandler.publishToMQTT(message, MQTT_SUBTOPIC_SYSLOG);
        LOG_fatal << message << std::endl;

        // 3 langsame Beeps = ESP-Neustart (direkt auf GPIO; vor Restart sicher nutzbar)
        pinMode(BUZZER_PIN, OUTPUT);
        for (int i = 0; i < radiator::alarmcfg::RESTART_STATUS_BEEP_COUNT; i++)
        {
            digitalWrite(BUZZER_PIN, HIGH);
            delay(radiator::alarmcfg::RESTART_STATUS_ON_MS);
            digitalWrite(BUZZER_PIN, LOW);
            delay(radiator::alarmcfg::RESTART_STATUS_OFF_MS);
        }

        sleep(10);
        esp_restart();
    }
    catch (const std::exception &e)
    {
        message = getMillisAndTime() + " ms: EXCEPTION in loop(): " + e.what() +
                  " -> restarting ESP32 in 10s";
        netHandler.publishToMQTT(message, MQTT_SUBTOPIC_SYSLOG);
        LOG_fatal << message << std::endl;

        // 3 langsame Beeps = ESP-Neustart
        pinMode(BUZZER_PIN, OUTPUT);
        for (int i = 0; i < radiator::alarmcfg::RESTART_STATUS_BEEP_COUNT; i++)
        {
            digitalWrite(BUZZER_PIN, HIGH);
            delay(radiator::alarmcfg::RESTART_STATUS_ON_MS);
            digitalWrite(BUZZER_PIN, LOW);
            delay(radiator::alarmcfg::RESTART_STATUS_OFF_MS);
        }

        sleep(10);
        esp_restart();
    }
    catch (...)
    {
        message = getMillisAndTime() + " ms: UNKNOWN EXCEPTION in loop() -> restarting ESP32 in 10s";
        netHandler.publishToMQTT(message, MQTT_SUBTOPIC_SYSLOG);
        LOG_fatal << message << std::endl;

        // 3 langsame Beeps = ESP-Neustart
        pinMode(BUZZER_PIN, OUTPUT);
        for (int i = 0; i < radiator::alarmcfg::RESTART_STATUS_BEEP_COUNT; i++)
        {
            digitalWrite(BUZZER_PIN, HIGH);
            delay(radiator::alarmcfg::RESTART_STATUS_ON_MS);
            digitalWrite(BUZZER_PIN, LOW);
            delay(radiator::alarmcfg::RESTART_STATUS_OFF_MS);
        }

        sleep(10);
        esp_restart();
    }
}
