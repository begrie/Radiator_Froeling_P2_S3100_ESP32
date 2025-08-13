#ifndef __DH_EXTERNALSENSORS_H__
#define __DH_EXTERNALSENSORS_H__

#include "config.h"
#include "networking.h"

#include <Ticker.h>

#if WITH_DHT_SENSOR
#include <DHT.h>
#endif

#if WITH_AIR_INPUT_FLAP
#include "ESP32_Servo.h"
#endif

namespace radiator
{
    class ExternalSensors
    {
    public:
        static bool initExternalSensors();

        static std::string getSensorValues();
        static std::string getSensorValuesForJSON();
        static std::string getSensorValueHeaderForCSV();
        static std::string getSensorValueDataForCSV();

        static void setRadiatorIsBurning() { radiatorIsBurning = true; };
        static void setRadiatorFireIsOff() { radiatorIsBurning = false; };

    protected:
        static void xTaskExternalSensors(void *parameter);
        // we need a mutex semaphore to handle concurrent access from different tasks - otherwise "funny" crashes from bufferQueue-handling
        static SemaphoreHandle_t semaphoreExternalSensors;
        static SemaphoreHandle_t semaphoreAcCurrentSensor; // Definition für AC-Strom Semaphor
        static std::string messageBuf;

        static void autoControlVentilatorAndFlap();
        static bool radiatorIsBurning;

#if WITH_DHT_SENSOR
        static bool initTempHumidityDHTSensor();
        static int16_t readTemp();
        static int16_t readHumidity();
        // static int16_t getTemp() { return lastRoomTemperature; };
        // static int16_t getHumidity() { return lastRoomHumidity; };
        static int dhtGPIO;
        static DHT tempHumidityDHTSensor;
#endif // WITH_DHT_SENSOR
        static int16_t lastRoomTemperature; //zur Vereinfachung trotzdem definieren
        static int16_t lastRoomHumidity;    // zur Vereinfachung trotzdem definieren

        static bool initVentilator();
        static void setVentilatorOn();
        static void setVentilatorOff();
        static int ventilatorRelaisGPIO;
        static bool ventilatorRelaisState;

        static bool initAirInputFlap();
        static bool airInputFlapIsOpen; // immer deklarieren!
#if WITH_AIR_INPUT_FLAP
        static void openAirInputFlap();
        static void closeAirInputFlap();
        static int servoForAirInputFlapGPIO;
        static ESP32_Servo servoForAirInputFlap;
#endif

        static bool initLeakWaterSensor();
        static bool getLeakWaterSensorState() { return leakWaterDetected; };
        static int leakWaterSensorGPIO;
        static volatile bool leakWaterDetected;

        static bool initAcCurrentSensor();
        static void IRAM_ATTR tickerCallbackForReadAndAverageAcCurrentSensor(); // used by ticker
        static double getAcCurrentAmpereRMS();
        static std::string getAcCurrentAmpereRMSasString();
        static int acCurrentSensorGPIO;
        static Ticker tickerForReadAcCurrentSensor;
        static volatile int16_t acCurrentAnalogReadAmplitudeMillivolt;
    };
} // namespace radiator
#endif // #ifndef __DH_EXTERNALSENSORS_H__