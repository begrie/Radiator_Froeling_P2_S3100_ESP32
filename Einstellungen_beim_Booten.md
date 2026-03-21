# Froeling P2 ESP32

## Systemeinstellungen beim Booten ändern

### Voraussetzungen vor dem Boot

- Serielle-Konsole oeffnen (PlatformIO Monitor)
- Monitor-Parameter: 115200 Baud, Echo EIN
- Zeilenende fuer Eingaben: CR oder CRLF

---

### Boot ohne gedrueckten Button (Normalfall)

- ESP nutzt gespeicherte WLAN-Daten aus NVS (persistent)
- WLAN-Verbindung startet im Hintergrund
- MQTT nutzt gespeicherte Broker/User/Pass-Daten
- Kein Konsolen-Dialog

Hinweis:

- Wenn keine gueltigen WLAN-Daten gespeichert sind, kann WLAN-Start fehlschlagen.

---

### Boot MIT gedruecktem gelben Quit-Button (Konfig-Modus)

Button beim Einschalten gedrueckt halten.

Ablauf in der Konsole:

1. WLAN-Scan wird gestartet, gefundene SSIDs werden nummeriert angezeigt.
2. Auswahl eingeben:
   - Zahl 1..N: dieses WLAN verwenden
   - 0: WLAN komplett AUS (gespeicherte AP-Daten loeschen)
   - -1: WLAN unveraendert lassen, nur MQTT-Einstellungen aendern
3. Bei Auswahl 1..N: WLAN-Passwort eingeben.
4. Danach MQTT-Dialog:
   - Broker: ENTER = behalten, 0 = loeschen/deaktivieren, sonst neuer Wert
   - User: ENTER = behalten, 0 = loeschen, sonst neuer Wert
   - Pass: ENTER = behalten, 0 = loeschen, sonst neuer Wert
5. Werte werden persistent im NVS gespeichert.

---

### Schnelle Bedienlogik am Geraet

- Nur normal starten: Button NICHT druecken
- WLAN/MQTT neu einstellen: Button beim Boot druecken
- MQTT abschalten: Im MQTT-Broker-Feld "0" eingeben
- WLAN abschalten: Bei Netzwerkauswahl "0" eingeben

---

### Merksatz

Button beim Boot = Setup-Menue.
Kein Button = gespeicherte Konfiguration verwenden.
