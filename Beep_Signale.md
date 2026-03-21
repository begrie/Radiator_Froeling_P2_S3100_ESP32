# Froeling P2 ESP32

## Beep-Signale

### 1) Dauerton (ohne Pause)

**Bedeutung:** LECKWASSER erkannt (kritisch)
**Aktion:** Sofort auf Wasserleck pruefen, Bereich sichern, Ursache beheben.

---

### 2) Stotter-Alarm in Bloecken

**Muster:** 4x kurz (120 ms EIN / 120 ms AUS), dann 1 s Pause, Wiederholung
**Bedeutung:** Grenzwert ueberschritten (z. B. Kesseltemperatur > 90 C)
**Aktion:** Anlage und Messwerte sofort kontrollieren.

---

### 3) Langsames Intervall-Piepen

**Muster:** ca. 0,7 s EIN / 0,7 s AUS
**Bedeutung:** Heizungs-Fehlermeldung (normaler Error-Alarm)
**Aktionen:** 1. Fehlertext auf Heizungsdisplay pruefen, 2. Stoerung beheben und AN HEIZUNGSSTEUERUNG quittieren (Enter-Taste), 3. An ESP-Zusatzbox mit GELBER Quit-Taste Fehler-Buzzer stoppen

---

### 4) 5 schnelle Beeps (kurz)

**Bedeutung:** WLAN verbunden
**Aktion:** Keine (nur Statusinfo)

---

### 5) 3 langsame Beeps (lang)

**Bedeutung:** WLAN/SSID nicht verfuegbar
**Aktion:** Router/SSID/Empfang pruefen, ggf. Boot-Setup nutzen.

---

### 6) Sehr schnelles Rattern (einige Sekunden)

**Bedeutung:** Dateisystem-/SD-Problem
**Aktion:** SD/Dateisystem pruefen, bei Wiederholung neu starten.

---

## Wichtig

- Gelbe Quit-Taste stoppt den aktiven Fehler-Buzzer.
- Wenn die Ursache weiter besteht, kommt der Alarm erneut.
- Prioritaet im Zweifel: 1) Leckwasser 2) Grenzwert 3) sonstige Fehler.
