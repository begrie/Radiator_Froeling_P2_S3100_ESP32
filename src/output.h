#ifndef __DH_OUTPUT_H__
#define __DH_OUTPUT_H__

#include <fstream>
#include <sstream>

#include "config.h"
#include "networking.h"
#include "surveillance.h"

namespace radiator
{
    class OutputHandler : public radiator::SurveillanceHandler
    {
    public:
        enum class BuzzerPattern
        {
            INTERVALL,
            CONTINUOUS,
            LIMIT_STUTTER
        };

        OutputHandler(std::string_view pathnameOnly, const bool _toConsole, const bool _toMQTT);
        virtual ~OutputHandler();

        virtual void handleTime(radiator::Surveillance &surveillance,
                                uint8_t dow,
                                uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second);

        virtual void handleMeasurement(Surveillance &surveillance, const std::vector<VALUE_DATA> &values);

        virtual void handleError(Surveillance &surveillance,
                                 uint16_t year, uint8_t month, uint8_t day,
                                 uint8_t hour, uint8_t minute, uint8_t second,
                                 std::string description);

        // typedef std::tuple<time_t, std::string, std::vector<VALUE_DATA>> ValuesWithTime_t;

    protected:
        bool toConsole = true;
        bool toMQTT = false;
        bool toFile = false;
        std::string outputPath;     // base path with mountpoint of targeted filesystem -> without filename (e.g. /littlefs/P2_logfiles)
        std::string outputFilename; // one file per day -> is derived from date, template yyyy-mm-dd.log (e.g. /2022-09-01.log)

        uint16_t MQTTOutputIntervallSec = MQTT_OUTPUTINTERVALL_SEC;
        uint16_t fileOutputIntervallSec = FILE_OUTPUT_INTERVALL_SEC;

    protected:
        std::string lastRadiatorTimeString; // Zeit aus M2 speichern

        std::string bufStr;              // member var instead to local vars to avoid heap fragmentation
        std::ostringstream outStrStream; // for output functions instead to local vars to avoid heap fragmentation
        std::ofstream outFileStream;

        std::string formatValueData(Surveillance &surveillance, const std::vector<VALUE_DATA> &values);
        std::string formatValueDataAsJSON(std::string_view timeStringForValues, const std::vector<VALUE_DATA> &values);

        std::string formatValueDataHeaderForCSV(Surveillance &surveillance);
        std::string formatValueDataForCSV(std::string_view time, const std::vector<VALUE_DATA> &values);

        void outputToConsole(std::string_view output);

        void handleValuesFileOutput(Surveillance &surveillance, const std::string &timeString, const std::vector<VALUE_DATA> &values);
        std::string deriveFilename(const std::string &stringWithDate);
        bool handleFiles(std::string_view filename);
        void outputToFile(std::string_view output, const ulong writeToFileIntervalSec = WRITE_TO_FILE_INTERVAL_SEC); // writeToFileIntervalSec controls how often the streambuffer is written to file and how many data can get lost ...

        void handleValuesMQTTOutput(const std::string &timeString, const std::vector<VALUE_DATA> &values);
        void outputToMQTT(std::string_view output, std::string_view subtopic = "");

        bool checkForRadiatorIsBurning(const std::vector<VALUE_DATA> &values);
        bool checkForLimit(const std::vector<VALUE_DATA> &values, std::string_view parameterName, const int limit, const bool greaterThan = true);

    public:
        void setSystemTimeFromRadiatorData(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);

        bool radiatorIsBurning = false;
        std::string heatingStartTime;
        std::string heatingEndTime;

        void outputErrorToBuzzer(const uint16_t _beepIntervallMs = BEEP_INTERVALL_RADIATOR_ERROR_MS,
                                 const int _timeoutSec = 0,
                                 const BuzzerPattern _pattern = BuzzerPattern::INTERVALL);
    };
}

#endif
