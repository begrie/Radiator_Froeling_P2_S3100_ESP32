# Froeling P2 ESP32

## Anschlussbelegung (GPIO)

Stand: aus src/config.h und aktueller Code-Nutzung

## Seriell

- USB-Konsole (Setup/Logs): UART0 ueber Board-USB, Monitor 115200 Baud
- Hinweis: RS232-TTL Wandler noetig (z. B. MAX3232):
  - Heizung P2/S3100 RX: GPIO16 (ESP-Eingang), Serial2 mit 9600 8N1
  - Heizung P2/S3100 TX: GPIO17 (ESP-Ausgang), Serial2 mit 9600 8N1

## Speicher

- SD Card CS: GPIO5 (Ausgang)
- SD SPI SCK: GPIO18
- SD SPI MISO: GPIO19
- SD SPI MOSI: GPIO23
- SD SPI Speed: 4 MHz
- Hinweis: entspricht der ueblichen VSPI-Belegung beim ESP32 DevKit

## Bedienung / Signale

- Buzzer (aktiv): GPIO27, digital HIGH/LOW
- Quit-/Setup-Button: GPIO13, INPUT_PULLUP, aktiv LOW
- Button-Funktion beim Boot: WLAN/MQTT-Setup starten
- Button-Funktion im Betrieb: Buzzer quittieren

## Externe Sensoren / Aktoren

- DHT11 Temp/Feuchte: GPIO21
  Aktiv nur wenn USE_EXTERNAL_SENSORS=true und WITH_DHT_SENSOR=true

- Ventilator-Relais: GPIO22
  Aktiv nur wenn USE_EXTERNAL_SENSORS=true und WITH_VENTILATOR=true

- Servo Luftklappe: GPIO25
  Aktiv nur wenn USE_EXTERNAL_SENSORS=true und WITH_AIR_INPUT_FLAP=true

- Leckwasser-Sensor: GPIO39 (Eingang)
  Aktiv wenn USE_EXTERNAL_SENSORS=true
  Hinweis: GPIO39 ohne internen Pullup, Auswertung per Interrupt (CHANGE)

- AC-Stromsensor: GPIO33 (Analog-Eingang)
  Aktiv wenn USE_EXTERNAL_SENSORS=true

## Aktueller Feature-Status

- USE_WIFI=true
- START_WEBSERVER=false
- OUTPUT_TO_MQTT=true
- OUTPUT_TO_FILE=true
- USE_EXTERNAL_SENSORS=true
- WITH_DHT_SENSOR=false
- WITH_VENTILATOR=false
- WITH_AIR_INPUT_FLAP=false

## Kurzcheck vor Verdrahtung

1. Heizungsschnittstelle auf GPIO16/17 und 9600 8N1 pruefen.
2. Buzzer auf GPIO27, Button auf GPIO13 gegen GND (aktiv LOW).
3. SD-CS auf GPIO5 pruefen.
4. SD-SPI-Leitungen auf GPIO18/19/23 pruefen (SCK/MISO/MOSI).
5. Externe Sensorik nur verdrahten, wenn der jeweilige Feature-Schalter aktiv ist.
