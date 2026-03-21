#include "output.h"
#include "debug.h"
#include "externalsensors.h"
// #include "analysis.h"

#include <fstream>
#include <iomanip>

#include <regex>

namespace radiator
{
    /*********************************************************************
     * @brief 	class constructor
     * @param 	void
     * @return 	void
     *********************************************************************/
    OutputHandler::OutputHandler(std::string_view pathnameOnly, const bool _toConsole, const bool _toMQTT)
        : outputPath(pathnameOnly),
          toConsole(_toConsole),
          toMQTT(_toMQTT)
    {
        outputPath.empty() ? toFile = false : toFile = true;

        bufStr.reserve(2000); // an attempt to avoid heap fragmentation

        RADIATOR_LOG_INFO(getMillisAndTime() << "Output to\tconsole: \t" << (toConsole ? "ON" : "OFF") << "\n"
                                             << "                     \tfile: \t\t" << (toFile ? "ON" : "OFF") << "\n"
                                             << "                     \tMQTT: \t\t" << (toMQTT ? "ON" : "OFF") << std::endl;)

        if (toFile)
        {
            if (outputPath.back() != '/')
                outputPath += "/";

            // check for path availability (with fstream only possible by usage of a testfile which cannot be deleted here...)
            outFileStream.open(outputPath + "dirtest.tst");
            if (!outFileStream)
            {
                bufStr = getMillisAndTime() + "Output path  " + outputPath + "  CAN NOT BE OPENED -> NO VALUES WILL BE SAVED TO FILE";
                NetworkHandler::publishToMQTT(bufStr, MQTT_SUBTOPIC_SYSLOG);
                RADIATOR_LOG_ERROR(bufStr << std::endl;)

                ::perror("OutputHandler: Unable to open file for output");
                throw("Unable to open file for output");
            }
            outFileStream.close();

            RADIATOR_LOG_INFO(getMillisAndTime() << "Files are saved to path   " << outputPath << std::endl;)
        }
    }

    /*********************************************************************
     * @brief 	class destructor
     * @param 	void
     * @return 	void
     *********************************************************************/
    OutputHandler::~OutputHandler()
    {
    }

    /*********************************************************************
     * @brief 	The radiator sends every second a "M2" command with the actual time
     *          followed by a "M1" command with the values at this time
     *          ->  in this function the "M2" is handled by formatting the radiator time
     *              and pushing it to the next processing steps
     * @param 	instance of Surveillance working class
     * @param   ... date & time ...
     * @return 	void
     *********************************************************************/
    void OutputHandler::handleTime(Surveillance &surveillance,
                                   uint8_t dow,
                                   uint16_t year, uint8_t month, uint8_t day,
                                   uint8_t hour, uint8_t minute, uint8_t second)
    {
        outStrStream.str(""); // delete stream content

        outStrStream << std::dec
                     << std::setw(4) << std::setfill('0') << (int)year << "-"
                     << std::setw(2) << std::setfill('0') << (int)month << "-"
                     << std::setw(2) << std::setfill('0') << (int)day << DELIMITER_FOR_CSV_FILE
                     << std::setw(2) << std::setfill('0') << (int)hour << ":"
                     << std::setw(2) << std::setfill('0') << (int)minute << ":"
                     << std::setw(2) << std::setfill('0') << (int)second;

        lastRadiatorTimeString = outStrStream.str(); // <--- Zeit in member variable speichern

        static const char *dowString[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

        if (toConsole)
        {
            std::string dayStr;
            if (dow >= 1 && dow <= 7)
                dayStr = dowString[dow - 1];
            else
            {
                RADIATOR_LOG_ERROR(getMillisAndTime() << "handleTime: Invalid day-of-week value= " << (int)dow << std::endl;)
                dayStr = "INVALID";
            }
            outputToConsole((std::string) "[TIME] " + dayStr + ", " + outStrStream.str());
        }

        static ulong nextSystemTimeSet = 0;
        if (millis() >= nextSystemTimeSet) // set/synchronize every 48 hours
        {
            setSystemTimeFromRadiatorData(year, month, day, hour, minute, second);
            // radiator::Analysis::init();
            nextSystemTimeSet = millis() + 48 * 60 * 60 * 1000; // set/synchronize every 48 hours

            char msg[80];
            snprintf(msg, sizeof(msg), "%s System-time synchronized to Froeling P2/S3100", getMillisAndTime().c_str());
            outputToMQTT(msg, MQTT_SUBTOPIC_SYSLOG);
        }
    }

    /*********************************************************************
     * @brief 	handling of the measurement values received (every 1 second) from the radiator
     *          with the "M1" command after a M2 with the time
     * @param 	instance of Surveillance working class
     * @param 	list with values from radiator
     * @return 	void
     *********************************************************************/
    void OutputHandler::handleMeasurement(Surveillance &surveillance, const std::vector<VALUE_DATA> &values)
    {
        if (toConsole)
            outputToConsole(formatValueData(surveillance, values));

        if (toFile)
            handleValuesFileOutput(surveillance, lastRadiatorTimeString, values);

        if (toMQTT)
            handleValuesMQTTOutput(lastRadiatorTimeString, values);

        checkForLimit(values, "Kesseltemp", 90); // check against overheating if more than 90°

#if USE_EXTERNAL_SENSORS
        if (checkForRadiatorIsBurning(values))
            radiator::ExternalSensors::setRadiatorIsBurning();
        else
            radiator::ExternalSensors::setRadiatorFireIsOff();
#endif
    }

    /*********************************************************************
     * @brief 	handling of error messages received from the radiator
     * @param  	instance of Surveillance working class
     * @param 	... date & time ...
     * @param   error description
     * @return 	void
     *********************************************************************/
    void OutputHandler::handleError(Surveillance &surveillance,
                                    uint16_t year, uint8_t month, uint8_t day,
                                    uint8_t hour, uint8_t minute, uint8_t second,
                                    std::string description)
    {
        outStrStream.str(""); // delete stream content

        outStrStream << "[ERROR] "
                     << std::dec
                     << std::setw(4) << std::setfill('0') << (int)year << "-"
                     << std::setw(2) << std::setfill('0') << (int)month << "-"
                     << std::setw(2) << std::setfill('0') << (int)day << ", "
                     << std::setw(2) << std::setfill('0') << (int)hour << ":"
                     << std::setw(2) << std::setfill('0') << (int)minute << ":"
                     << std::setw(2) << std::setfill('0') << (int)second << ": "
                     << description << std::endl;

        if (toConsole)
            outputToConsole(outStrStream.str());

        // signal errors with buzzer sound
        if (description.find("Last errors") == std::string::npos) // NO sound signal for "Last errors" from connection begin
            outputErrorToBuzzer();

        if (toMQTT)
            outputToMQTT(outStrStream.str(), MQTT_SUBTOPIC_ERRORLOG);

        if (toFile)
        {
            if (handleFiles(deriveFilename(outStrStream.str())))
                outputToFile(outStrStream.str());
            else
            {
                bufStr = getMillisAndTime() + "handleError(): Can not save to File";
                NetworkHandler::publishToMQTT(bufStr, MQTT_SUBTOPIC_SYSLOG);
                RADIATOR_LOG_ERROR(bufStr << std::endl;)
            }
        }
    }

    /*********************************************************************
     * @brief 	format received value data to string for output to console or/and file
     * @param  	instance of Surveillance working class
     * @param 	list with values from radiator
     * @return 	formatted string with received values
     *          e.g.:    [] = [Ausgeschaltet ] [S] (6)
     *                   [] = [  Brenner Aus  ] [S] (1)
     *                   [Zustand] = [1] [N] (1)
     *                   [ROST] = [0] [N] (0)
     *                   [Kesseltemp] = [18°] [N] (37)
     *                   [Abgastemp.] = [21°] [N] (21)
     *                   [Abgas. SW ] = [31°] [N] (31)
     *                   [KessStellGr] = [50%] [N] (50)
     *                   [Saugzug  ] = [0%] [N] (0)
     *                   [Zuluftgebl] = [0%] [N] (0)
     *                   [Einschub] = [0%] [N] (0)
     *                   [Fuellst.:] = [99.9%] [N] (20673)
     *                   [Feuerraumt] = [347°] [N] (347)
     *                   [Puffert.ob] = [18°] [N] (37)
     *                   [Puffert.un] = [19°] [N] (38)
     *                   [Puffer Pu.] = [0%] [N] (0)
     *                   [Außentemp] = [17°] [N] (34)
     *                   [Vorlauft.1sw] = [0°] [N] (0)
     *                   [Vorlauft.1] = [20°] [N] (41)
     *                   [Vorlauft.2sw] = [0°] [N] (0)
     *                   [Vorlauft.2] = [20°] [N] (39)
     *                   [KTY6_H2] = [127°] [N] (254)
     *                   [KTY7_H2] = [127°] [N] (254)
     *                   [Brenn.ST] = [10461] [N] (10461)
     *                   [Laufzeit:] = [15878h] [N] (15878)
     *                   [Boardtemp.] = [21°] [N] (21)
     *                   [Die Kesseltemp. soll sein] = [75°] [N] (150)
     *********************************************************************/
    std::string OutputHandler::formatValueData(Surveillance &surveillance, const std::vector<VALUE_DATA> &values)
    {
        bufStr = "";

        auto parameterNames = surveillance.getParameterNames();
        for (auto iter = values.begin();
             iter != values.end();
             ++iter)
        {
            bufStr += " [" + iter->name + "] = [" + iter->value + "]";

            switch (parameterNames[iter->index].type)
            {
            case PNT_STRING:
                bufStr += " [S]";
                break;
            case PNT_VALUE:
                bufStr += " [N]";
                break;
            default:
                bufStr += " [O]";
                break;
            }

            bufStr += (std::string) " (" + std::to_string((int)iter->rawValue) + ")\n";
        }

#if USE_EXTERNAL_SENSORS
        bufStr += radiator::ExternalSensors::getSensorValues();
#endif

        bufStr += "\n";

        return bufStr;
    }

    /*********************************************************************
     * @brief 	format received value data to string for output as JSON
     * @param  	instance of Surveillance working class
     * @param 	string with date and time e.g.  22-09-01; 12:21:35
     * @param 	list with values from radiator
     * @return 	JSON-formatted string with received values
     *          e.g.:  {
     *                   "date": "22-09-01",
     *                   "time": "12:21:35",
     *                   "Betriebsart": "Ausgeschaltet",
     *                   "Status": "Brenner Aus",
     *                   "Zustand": "1",
     *                   "ROST": "0",
     *                   "Kesseltemp": "18°",
     *                   "Abgastemp.": "21°",
     *                   "Abgas. SW": "31°",
     *                   "KessStellGr": "50%",
     *                   "Saugzug": "0%",
     *                   "Zuluftgebl": "0%",
     *                   "Einschub": "0%",
     *                   "Fuellst.:": "99.9%",
     *                   "Feuerraumt": "347°",
     *                   "Puffert.ob": "18°",
     *                   "Puffert.un": "19°",
     *                   "Puffer Pu.": "0%",
     *                   "Außentemp": "17°",
     *                   "Vorlauft.1sw": "0°",
     *                   "Vorlauft.1": "20°",
     *                   "Vorlauft.2sw": "0°",
     *                   "Vorlauft.2": "20°",
     *                   "KTY6_H2": "127°",
     *                   "KTY7_H2": "127°",
     *                   "Brenn.ST": "10461",
     *                   "Laufzeit:": "15878h",
     *                   "Boardtemp.": "21°",
     *                   "Die Kesseltemp. soll sein": "75°",
     *                   "Heizungsraum-Temp": "27°",
     *                   "Heizungsraum-Feuchtigkeit": "14%",
     *                   "Ventilator": "AN",
     *                   "Zuluftklappe": "ZU",
     *                   "Leckwassersensor": "TROCKEN",
     *                   "Stromstaerke": 0.318198"A"
     *                 }
     *********************************************************************/
    std::string OutputHandler::formatValueDataAsJSON(std::string_view timeStringForValues, const std::vector<VALUE_DATA> &values)
    {
        bufStr = "{";

        auto date = timeStringForValues.substr(0, timeStringForValues.find(DELIMITER_FOR_CSV_FILE));
        auto time = timeStringForValues.substr(timeStringForValues.find(DELIMITER_FOR_CSV_FILE) + 1);

        bufStr += "\"date\": \"" + static_cast<std::string>(date) + "\", \"time\": \"" + static_cast<std::string>(time) + "\",";

        for (auto iter = values.begin();
             iter != values.end();
             ++iter)
        {
            bufStr += '"';
            if (iter->name.empty()) // first two value items have no names from radiator output itself -> so lets add them
            {
                if (iter->index == 0)
                    bufStr += "Betriebsart";
                else if (iter->index == 1)
                    bufStr += "Status";
                else
                    bufStr += std::to_string(iter->index);
            }
            else
            {
                bufStr += iter->name;
            }
            bufStr += "\": \"" + iter->value + "\",";
        }

#if USE_EXTERNAL_SENSORS
        bufStr += radiator::ExternalSensors::getSensorValuesForJSON();
#endif
        bufStr.pop_back(); // remove last "," (, must exist and no spaces behind)
        // auto pos = bufStr.find_last_of(","); // remove last "," (missing and spaces behind allowed)
        // if (pos != std::string::npos && pos >= bufStr.length() - 3)
        //   bufStr.erase(pos);

        bufStr += "} \n";

        return bufStr;
    }

    /*********************************************************************
     * @brief 	format data for header of a csv file
     * @param  	instance of Surveillance working class
     * @return 	formatted string with parameter names received from radiator -> delimiter is ; (semicolon)
     *          e.g.: Date; Time;  [S];  [S]; Zustand; ROST; Kesseltemp; Abgastemp.; Abgas. SW; KessStellGr; Saugzug; Zuluftgebl; Einschub; Fuellst.:; Feuerraumt; Puffert.ob; Puffert.un; Puffer Pu.; Außentemp; Vorlauft.1sw; Vorlauft.1; Vorlauft.2sw; Vorlauft.2; KTY6_H2; KTY7_H2; Brenn.ST; Laufzeit:; Boardtemp.; Die Kesseltemp. soll sein;
     *********************************************************************/
    std::string OutputHandler::formatValueDataHeaderForCSV(Surveillance &surveillance)
    {
        bufStr = "Date" DELIMITER_FOR_CSV_FILE " Time" DELIMITER_FOR_CSV_FILE;

        auto parameterNames = surveillance.getParameterNames();
        for (auto el : parameterNames)
        {
            bufStr += el.name;

            if (el.type == PNT_STRING) // first two value items have no names - so lets add them
            {
                if (el.index == 0)
                    bufStr += "Betriebsart";
                else if (el.index == 1)
                    bufStr += "Status";
                else
                    bufStr += std::to_string(el.index);
            }

            bufStr += DELIMITER_FOR_CSV_FILE;
        }

#if USE_EXTERNAL_SENSORS
        bufStr += radiator::ExternalSensors::getSensorValueHeaderForCSV() + DELIMITER_FOR_CSV_FILE;
#endif

        bufStr += "\n";

        RADIATOR_LOG_INFO("formatValueDataHeaderForCSV = " << bufStr << std::endl;)

        return bufStr;
    }

    /*********************************************************************
     * @brief 	format values from radiator for output as CSV file
     * @param 	string with date and time e.g.  22-09-01; 12:21:35
     * @param 	list of radiator values at one time
     * @return 	formatted string with received values
     *          e.g.  22-09-01; 12:21:35; Ausgeschaltet; Brenner Aus; 1; 0; 18°; 21°; 31°; 50%; 0%; 0%; 0%; 99.9%; 347°; 18°; 19°; 0%; 17°; 0°; 20°; 0°; 20°; 127°; 127°; 10461; 15878h; 21°; 75°;
     *********************************************************************/
    std::string OutputHandler::formatValueDataForCSV(std::string_view time, const std::vector<VALUE_DATA> &values)
    {
        bufStr = static_cast<std::string>(time) + DELIMITER_FOR_CSV_FILE;

        for (auto iter = values.begin();
             iter != values.end();
             ++iter)
        {
            bufStr += iter->value + DELIMITER_FOR_CSV_FILE;
        }

#if USE_EXTERNAL_SENSORS
        bufStr += radiator::ExternalSensors::getSensorValueDataForCSV();
#endif

        bufStr += "\n";

        return bufStr;
    }

    /*********************************************************************
     * @brief 	handle output of values to files
     *          -> inclusive check of fileOutputIntervallSec
     * @param 	used instance of Surveillance -> for access to parameter names
     * @return 	void
     *********************************************************************/
    void OutputHandler::handleValuesFileOutput(Surveillance &surveillance, const std::string &timeString, const std::vector<VALUE_DATA> &values)
    {
        if (!toFile)
        {
            RADIATOR_LOG_WARN(getMillisAndTime() << "Output to file is disabled" << std::endl;)
            return;
        }

        static ulong nextFileOutputSec = 0;

        if (millis() / 1000 < nextFileOutputSec)
        {
            RADIATOR_LOG_DEBUG(getMillisAndTime() << "data buffered -> NO file output (next in "
                                                  << (nextFileOutputSec - (millis() / 1000))
                                                  << " seconds)" << std::endl;)
            return;
        }

        nextFileOutputSec = millis() / 1000 + fileOutputIntervallSec;

        auto filenameTillNow = outputFilename;
        if (handleFiles(deriveFilename(timeString)))
        {
            // check for new file to write the header only once
            if (filenameTillNow != outputFilename)
            {
                outputToFile(formatValueDataHeaderForCSV(surveillance) + formatValueDataForCSV(timeString, values));
            }
            else // without header - only value data
                outputToFile(formatValueDataForCSV(timeString, values));
        }
        else // problems with the file
        {
            bufStr = getMillisAndTime() + "ERROR saving to File";
            NetworkHandler::publishToMQTT(bufStr, MQTT_SUBTOPIC_SYSLOG);
            RADIATOR_LOG_ERROR(bufStr << std::endl;)
        }
    }

    /*********************************************************************
     * @brief 	derive filename from input string
     * @param 	string of any length with included date in format yyyy-mm-dd (e.g. 2022-09-01)
     *          -> only the first contained date is used
     * @return 	filename derived from input string like 22-09-01.log
     *          -> or  00-LOST.log  if not date found in input string
     *********************************************************************/
    std::string OutputHandler::deriveFilename(const std::string &stringWithDate)
    {
        RADIATOR_LOG_INFO(getMillisAndTime() << "OutputHandler::deriveFilename: ";)

        // Suche nach erstem Vorkommen eines Datums im Format yyyy-mm-dd
        std::string filename = "00-LOST.log";
        for (size_t i = 0; i + 9 < stringWithDate.size(); ++i)
        {
            // Prüfe auf vierstellige Jahreszahl
            if (isdigit(stringWithDate[i]) && isdigit(stringWithDate[i + 1]) &&
                isdigit(stringWithDate[i + 2]) && isdigit(stringWithDate[i + 3]) &&
                stringWithDate[i + 4] == '-' &&
                isdigit(stringWithDate[i + 5]) && isdigit(stringWithDate[i + 6]) &&
                stringWithDate[i + 7] == '-' &&
                isdigit(stringWithDate[i + 8]) && isdigit(stringWithDate[i + 9]))
            {

                // Optional: Gültigkeitsprüfung von Monat und Tag (einfach)
                int year = std::stoi(stringWithDate.substr(i, 4));
                int month = std::stoi(stringWithDate.substr(i + 5, 2));
                int day = std::stoi(stringWithDate.substr(i + 8, 2));
                if (year >= 2000 && year <= 2099 && month >= 1 && month <= 12 && day >= 1 && day <= 31)
                {
                    // Dateiname im Format "yy-mm-dd.log"
                    filename = stringWithDate.substr(i, 4) + "-" +
                               stringWithDate.substr(i + 5, 2) + "-" +
                               stringWithDate.substr(i + 8, 2) + ".log";
                    RADIATOR_LOG_INFO("stringWithDate= " << stringWithDate
                                                         << ", found date= " << stringWithDate.substr(i, 10)
                                                         << " ->filename= " << filename << std::endl;)
                    break;
                }
            }
        }

        if (filename == "00-LOST.log")
        {
            bufStr = getMillisAndTime() + "deriveFilename(): No suitable date found in given stringWithDate for derivation of the filename ->  00-LOST.log   will be used";
            NetworkHandler::publishToMQTT(bufStr);
            RADIATOR_LOG_ERROR(bufStr << std::endl;)
        }

        return filename;
    }

    /*********************************************************************
     * @brief 	handle files for output stream
     * @param 	filename without complete path (e.g. 22-09-01.log)
     * @return 	true:  output file is opened and associated to class member outFileStream
     *          false: error opening file
     *********************************************************************/
    bool OutputHandler::handleFiles(std::string_view filename)
    {
        RADIATOR_LOG_INFO(getMillisAndTime() << "OutputHandler::handleFiles: ";)

        // same filename as before -> continue saving to it
        if (filename == outputFilename && outFileStream && outFileStream.good())
        {
            RADIATOR_LOG_INFO("The values will continue to be saved in file " << (outputPath + outputFilename) << std::endl;)
            if (outFileStream.is_open())
                return true;
        }

        // if change of filename due to change of date -> close the old file
        if (outFileStream.is_open())
            outFileStream.close();

        // new file or re-open previous file ...
        auto outputPathWithFilename = outputPath + static_cast<std::string>(filename);
        outFileStream.open(outputPathWithFilename, std::ofstream::out | std::ofstream::app);
        if (!outFileStream.is_open() || !outFileStream.good())
        {
            bufStr = getMillisAndTime() + "handleFiles(): ERROR opening file " + outputPathWithFilename + " !! NO VALUES ARE SAVED TO FILE !!";
            NetworkHandler::publishToMQTT(bufStr, MQTT_SUBTOPIC_SYSLOG);
            RADIATOR_LOG_ERROR(bufStr << std::endl;)
            return false;
        }

        outputFilename = filename;

        RADIATOR_LOG_INFO(getMillisAndTime() << "File " << outputPathWithFilename << " was opened" << std::endl;)
        return true;
    }

    /*********************************************************************
     * @brief 	output string to a file which is already associated to outFileStream and opened
     * @param 	string with output data -> should be one line with eol (\n) termination
     * @param   interval (in seconds) to write from stream buffer to file (by closing the filestream)
     *          -> not every call of this function causes a file write operation,
     *             because the data is only held in the stream buffer!
     *          -> at failures (e.g. loss of energy) the data gets lost which is not written to file
     *          -> longer values (e.g. 15 min = 900sec) are reducing the load of the storage medium (espc. good for flash)
     * @return 	void
     *********************************************************************/
    void OutputHandler::outputToFile(std::string_view output, const ulong writeToFileIntervalSec)
    {
        RADIATOR_LOG_INFO(getMillisAndTime() << "OutputHandler::outputToFile"
                                             << " -> ************* File output: *************\n"
                                             << output << std::endl;)

        if (!outFileStream || !outFileStream.is_open() || !outFileStream.good())
        {
            bufStr = getMillisAndTime() + "outputToFile(): Can NOT SAVE to file " + (outputPath + outputFilename) +
                     "(outFileStream.is_open()=" + std::to_string(outFileStream.is_open()) + ", outFileStream.good()=" + std::to_string(outFileStream.good()) + ")";

            NetworkHandler::publishToMQTT(bufStr, MQTT_SUBTOPIC_SYSLOG);
            RADIATOR_LOG_ERROR(bufStr << std::endl;)
            return;
        }

        outFileStream << output;

        // outFileStream.flush(); //with flush only -> nothing is written to file and data gets lost ...

        static ulong lastClose = 0;
        if (millis() - lastClose >= writeToFileIntervalSec * 1000)
        {
            outFileStream.close(); // BUG (with LittleFS only)? -> files are only written at .close() (.flush() is not working)
            lastClose = millis();
            RADIATOR_LOG_INFO(lastClose << " ms: reached end of writeToFileIntervalSec= " << writeToFileIntervalSec
                                        << " -> Buffered file output data is written to file"
                                        << std::endl;)
        }
        else
        {
            RADIATOR_LOG_INFO(getMillisAndTime() << "File output data buffered for next file write in "
                                                 << ((lastClose + writeToFileIntervalSec * 1000 - millis()) / 1000) << " seconds"
                                                 << std::endl;)
        }
    }

    /*********************************************************************
     * @brief 	handle output of values to MQTT broker
     *          -> inclusive check of fileOutputIntervallSec
     * @param 	used instance of Surveillance -> for access to parameter names
     * @return 	void
     *********************************************************************/
    void OutputHandler::handleValuesMQTTOutput(const std::string &timeString, const std::vector<VALUE_DATA> &values)
    {
        if (!toMQTT)
        {
            RADIATOR_LOG_INFO(getMillisAndTime() << "Output to MQTT is disabled" << std::endl;)
            return;
        }

        static ulong nextMQTTOutputSec = 0;

        if (millis() / 1000 < nextMQTTOutputSec)
        {
            RADIATOR_LOG_DEBUG(getMillisAndTime() << "handleValuesMQTTOutput(): data buffered -> NO MQTT output (next in "
                                                  << (nextMQTTOutputSec - (millis() / 1000)) << " seconds)"
                                                  << std::endl;)
            return;
        }

        nextMQTTOutputSec = millis() / 1000 + MQTTOutputIntervallSec;

        outputToMQTT(formatValueDataAsJSON(timeString, values));
    }

    /*********************************************************************
     * @brief 	output string to MQTT as payload
     * @param 	payload message
     * @param 	optional subtopic e.g. "/errorlog" (default "")
     * @return 	void
     *********************************************************************/
    void OutputHandler::outputToMQTT(std::string_view output, std::string_view subtopic)
    {
        radiator::NetworkHandler::publishToMQTT(static_cast<std::string>(output), static_cast<std::string>(subtopic));
    }

    /*********************************************************************
     * @brief 	check if the radiator is burning
     *          - check is done by evaluation of parameter with index 1:
     *            "Brenner Aus" -> false
     *            all other -> true
     *            ("Vorbereitung", "Anheizen", "Vorwärmphase", "Zünden", "Heizen", "Abst.Warten", "Abst.Einschub")
     * @param 	values from one timestamp
     * @return 	true : radiator is burning
     *          false: radiator is NOT burning
     *********************************************************************/
    bool OutputHandler::checkForRadiatorIsBurning(const std::vector<VALUE_DATA> &values)
    {
        RADIATOR_LOG_DEBUG(getMillisAndTime() << "checkForRadiatorIsBurning= ";)

        auto it = std::find_if(
            values.begin(), values.end(),
            [](auto el)
            {
                if (el.index == 1)
                    return true;
                else
                    return false;
            });

        if (it == values.end())
        {
            RADIATOR_LOG_ERROR(getMillisAndTime() << "checkForRadiatorIsBurning(): Index 1 not found" << std::endl;)
            return true;
        }

        RADIATOR_LOG_DEBUG("it->index= " << it->index << ", name= " << it->name << ", value = " << it->value << std::endl;)

        if (it->value.find("Brenner Aus") != std::string::npos)
        {
            RADIATOR_LOG_DEBUG("false (" << it->value << ")" << std::endl;)
            return false;
        }

        RADIATOR_LOG_DEBUG("true (" << it->value << ")" << std::endl;)
        return true;
    }

    /*********************************************************************
     * @brief 	checks one parameter from value data set for exceeding a limit
     *          -> if limit is exceeded: buzzer will be activated and MQTT message is send
     * @param 	values from one timestamp
     * @param 	name of parameter to check
     * @param 	limit to check against
     * @param 	true (standard): value greater than limit?
     *          false: value less than limit?
     * @return 	false: limit NOT exceeded
     *          true : limit exceeded
     *********************************************************************/
    bool OutputHandler::checkForLimit(const std::vector<VALUE_DATA> &values, std::string_view parameterName, const int limit, const bool greaterThan)
    {
        RADIATOR_LOG_DEBUG(getMillisAndTime() << "checkForLimit -> parameterName=" << parameterName << ", limit= " << limit << std::endl;)

        for (auto el : values)
        {
            if (el.name.compare(0, parameterName.size(), parameterName) == 0) // compare from first pos until length of comparing string
            {
                RADIATOR_LOG_DEBUG(getMillisAndTime() << "parameterName=" << parameterName << ", el.name = " << el.name << std::endl;)

                // int valueAsInt = std::stoi(el.value);  //stoi can throw an expection -> so better to use atoi
                int valueAsInt = atoi(el.value.c_str());
                RADIATOR_LOG_DEBUG(getMillisAndTime() << "valueAsInt= " << valueAsInt << std::endl;)

                if ((greaterThan && valueAsInt > limit) || (!greaterThan && valueAsInt < limit))
                {
                    static ulong nextInfoOutputMs = 0;
                    static const int infoOutputIntervallSec = 60; // info output all 60 sec -> consider needed space when output is redirected to syslog file

                    outputErrorToBuzzer(BEEP_INTERVALL_RADIATOR_ERROR_MS,
                                        infoOutputIntervallSec - 1,
                                        BuzzerPattern::LIMIT_STUTTER);

                    if (millis() >= nextInfoOutputMs)
                    {
                        bufStr = getMillisAndTime() + "!!!!! ALERT: LIMIT EXCEEDED !!!!! parameterName= " + static_cast<std::string>(parameterName) + ", limit=" + std::to_string(limit) + ", actual value= " + std::to_string(valueAsInt);

                        std::cout << bufStr << std::endl;
                        RADIATOR_LOG_ERROR(bufStr << std::endl;)
                        NetworkHandler::publishToMQTT(bufStr);

                        nextInfoOutputMs = millis() + infoOutputIntervallSec * 1000;
                    }
                    return true;
                }
                else // parameter found, limit not exceeded
                {
                    RADIATOR_LOG_DEBUG(getMillisAndTime() << "Limit NOT exceeded " << std::endl;)
                    return false;
                }
            }
        }

        RADIATOR_LOG_ERROR(getMillisAndTime() << "checkForLimit(): NO PARAMETER    " << parameterName << "     FOUND to check limit    " << limit << std::endl;)

        return false;
    }

    /*********************************************************************
     * @brief 	set ESP system time from radiator data
     * @param 	values from one timestamp
     * @return 	void
     *********************************************************************/
    void OutputHandler::setSystemTimeFromRadiatorData(uint16_t year, uint8_t month, uint8_t day,
                                                      uint8_t hour, uint8_t minute, uint8_t second)
    {
        tm tmTimeToSet{0};                 // 0 - initializer is absolutely neccessary!!
        tmTimeToSet.tm_year = year - 1900; // years since 1900
        tmTimeToSet.tm_mon = month - 1;    // starts from 0
        tmTimeToSet.tm_mday = day;         // starts from 1
        tmTimeToSet.tm_hour = hour;
        tmTimeToSet.tm_min = minute;
        tmTimeToSet.tm_sec = second;

        RADIATOR_LOG_INFO(getMillisAndTime() << "Setting system time to " << asctime(&tmTimeToSet));
        // Serial.printf("Setting time: %s \n", asctime(&tmTimeToSet));

        timeval timevalTimetoSet{0, 0}; // 0 - initializer are absolutely neccessary!!
        timevalTimetoSet.tv_sec = mktime(&tmTimeToSet);
        settimeofday(&timevalTimetoSet, 0);

        // // only for debugging:
        // time_t actTime_t = time(NULL);
        // RADIATOR_LOG_DEBUG(getMillisAndTime() << "Getting system time: " << ctime(&actTime_t));
        // Serial.printf("%s Getting time= %s \n", getMillisAndTime().c_str(), ctime(&actTime_t));

        // tmTimeToSet = *localtime(&actTime_t);
        // Serial.printf("Getting time: %s \n", asctime(&tmTimeToSet));
    }

    /*********************************************************************
     * @brief 	output acoustic error message to connected buzzer
     *          -> with quit by button
     * @param 	time of beep and pause in milliseconds
     *          0: continous beep
     * @param 	timeout for beeping in seconds
     *          0 (standard): NO timeout
     * @return 	void
     *********************************************************************/
    void OutputHandler::outputErrorToBuzzer(const uint16_t _beepIntervallMs, const int _timeoutSec, const BuzzerPattern _pattern)
    {
        static TaskHandle_t Handle_xTaskBuzzer;
        static volatile bool quitButtonWasPressed;
        static uint16_t beepIntervallMs;
        static int timeoutSec;
        static BuzzerPattern pattern;

        quitButtonWasPressed = false;
        beepIntervallMs = _beepIntervallMs;
        timeoutSec = _timeoutSec;
        pattern = _pattern;

        if (!Handle_xTaskBuzzer)
        {
            auto xTaskBuzzer = [](void *parameter)
            {
                pinMode(BUZZER_PIN, OUTPUT);
                // auto beepIntervallMs = BEEP_INTERVALL_RADIATOR_ERROR_MS;

                const auto quitButtonPin = QUIT_BUZZER_BUTTON_PIN;
                pinMode(quitButtonPin, INPUT_PULLUP);
                quitButtonWasPressed = false;

                detachInterrupt(quitButtonPin); // button is used twice: also for starting WiFi config
                                                // -> detach ISR for WiFi config start
                attachInterrupt(
                    quitButtonPin,
                    []() IRAM_ATTR
                    {
                        quitButtonWasPressed = true;
                        detachInterrupt(quitButtonPin);
                    },
                    CHANGE);

                ulong endOfTimeout = ULONG_MAX;
                if (timeoutSec)
                    endOfTimeout = millis() + timeoutSec;

                bool OnOff = HIGH;
                digitalWrite(BUZZER_PIN, OnOff);

                while (!quitButtonWasPressed && endOfTimeout >= millis())
                {
                    if (pattern == BuzzerPattern::LIMIT_STUTTER)
                    {
                        // Distinct warning pattern for limit exceed: 4 short pulses + pause.
                        for (int i = 0; i < 4 && !quitButtonWasPressed && endOfTimeout >= millis(); i++)
                        {
                            digitalWrite(BUZZER_PIN, HIGH);
                            vTaskDelay(pdMS_TO_TICKS(120));

                            if (quitButtonWasPressed || endOfTimeout < millis())
                                break;

                            digitalWrite(BUZZER_PIN, LOW);
                            vTaskDelay(pdMS_TO_TICKS(120));
                        }

                        if (!quitButtonWasPressed && endOfTimeout >= millis())
                        {
                            digitalWrite(BUZZER_PIN, LOW);
                            vTaskDelay(pdMS_TO_TICKS(1000));
                        }
                    }
                    else if (pattern == BuzzerPattern::CONTINUOUS || !beepIntervallMs)
                    {
                        vTaskDelay(pdMS_TO_TICKS(100)); // give some time for other tasks
                    }
                    else if (beepIntervallMs)
                    {
                        digitalWrite(BUZZER_PIN, OnOff);
                        OnOff = !OnOff;
                        vTaskDelay(pdMS_TO_TICKS(beepIntervallMs));
                    }
                }

                digitalWrite(BUZZER_PIN, LOW);
                Handle_xTaskBuzzer = NULL;
                vTaskDelete(NULL);
            };

            // Create RTOS task
            BaseType_t _Result = xTaskCreatePinnedToCore(
                xTaskBuzzer,             // Task function
                "xTaskBuzzer",           // String with name of task
                3072,                    // Stack size in bytes
                NULL,                    // Parameter passed as input of the task
                uxTaskPriorityGet(NULL), // Priority of the task: higher values -> higher priority
                                         // with uxTaskPriorityGet(NULL)-> same priority as current task
                &Handle_xTaskBuzzer,     // Task handle (Typ: TaskHandle_t)
                1);                      // Core 0 or 1 (Arduino code by default on Core 1)

            if (_Result != pdPASS)
            {
                bufStr = std::to_string(millis()) + " ms: outputErrorToBuzzer: Error creating xTask";
                NetworkHandler::publishToMQTT(bufStr, MQTT_SUBTOPIC_SYSLOG);
                RADIATOR_LOG_ERROR(bufStr << std::endl;)
            }
        }
    }

    /*********************************************************************
     * @brief 	prints the given outout to std::cout (usually Serial)
     * @param 	formatted string for output
     * @return 	void
     *********************************************************************/
    void OutputHandler::outputToConsole(std::string_view output)
    {
        std::cout << output;
    }

} // end namespace