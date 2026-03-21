# Froeling P2 ESP32

## Beep-Signale

> **Unterscheidung:** Die Signale sind in zwei Kategorien eingeteilt:
>
> - **HEIZUNG KRITISCH** (Prioritaet 3-6): Direkte Heizungsanlage betroffen -> sofort handeln
> - **ESP-UEBERWACHUNG** (Prioritaet 1-2): Ueberwachungssystem selbst meldet ein Problem

---

## HEIZUNG KRITISCH

### 6) Dauerton (ohne Pause) - PRIO 6

**Bedeutung:** LECKWASSER erkannt
**Aktion:** Sofort auf Wasserleck pruefen, Bereich sichern, Ursache beheben.

---

### 5) Stotter-Alarm in Bloecken - PRIO 5

**Muster:** 4x kurz (120 ms EIN / 120 ms AUS), dann 1 s Pause, Wiederholung
**Bedeutung:** Grenzwert ueberschritten (z. B. Kesseltemperatur > 90 C)
**Aktion:** Anlage und Messwerte sofort kontrollieren.

---

### 4) Langsames Intervall-Piepen - PRIO 4

**Muster:** ca. 0,7 s EIN / 0,7 s AUS
**Bedeutung:** Heizungs-Fehlermeldung (Error-Alarm der Heizungssteuerung)
**Aktionen:**

1. Fehlertext auf Heizungsdisplay pruefen
2. Ursache beheben und Stoerung **an der Heizungssteuerung** quittieren (Enter-Taste)
3. Danach Buzzer an der ESP-Box mit der **gelben Quit-Taste** stoppen

---

### 3) Morse-S (3 kurze Beeps, Pause) - PRIO 3

**Muster:** · · · (3x 150 ms), dann 4 s Pause, Wiederholung
**Bedeutung:** Heizung sendet seit ca. 10 Minuten keine Daten mehr
**Aktion:** Serielle Verbindung Heizung <-> ESP-Box pruefen (Kabel, Stecker), Heizung manuell auf Fehler kontrollieren.

---

## ESP-UEBERWACHUNG

### 2) Rattern (3 Sekunden, alle 5 Minuten) - PRIO 2

**Muster:** sehr schnelle Pulse (~50 ms), klingt wie Rattern, ca. 3 Sekunden lang, dann 5 Minuten Pause, dann Wiederholung
**Bedeutung:** Dateisystem-/SD-Karten-Problem (ESP-Ueberwachung betroffen, nicht Heizung selbst)
**Aktion:** SD-Karte und Dateisystem pruefen; bei Wiederholung ESP-Geraet neu starten durch Reset-Button.

---

### 1) 2 langsame Beeps (alle 60 Sekunden) - PRIO 1

**Muster:** 2 x 600 ms EIN + 300 ms Pause, dann 60 Sekunden Pause, Wiederholung
**Bedeutung:** WiFi-Verbindung unterbrochen (ESP laeuft weiter, kein MQTT moeglich)
**Aktion:** Router/WLAN pruefen. Die Heizungsueberwachung laeuft lokal weiter und protokolliert auf SD-Karte.

---

## Statusinfo (kein Alarm)

### 5 schnelle Beeps (kurz)

**Bedeutung:** WLAN-Verbindung hergestellt
**Aktion:** Keine
_(Hinweis: Die Beeps entfallen, wenn gleichzeitig ein Heizungsalarm aktiv ist.)_

### 3 mittlere Beeps (500 ms, einmalig)

**Bedeutung:** ESP32 wird neu gestartet (Exception/Systemfehler)
**Aktion:** MQTT-Syslog pruefen. Wenn Neustarts sich wiederholen -> Geraet physisch inspizieren.

---

## Quit-Taste (gelbe Taste an ESP-Box)

- Stoppt den aktiven Buzzer sofort.
- Bei **Heizungsalarmen (Prio 3-6):** Buzzer pausiert fuer **1 Minute**. Danach kommt der Alarm erneut, solange die Ursache noch besteht.
- Alarme der Heizungssteuerung (Prio 4) muessen zudem **ZUERST an der Heizung selbst** quittiert werden.
- WLAN-Ausfall hat einen eigenen zyklischen Buzzer (Prio 1) (Quit-Taste ohne Wirkung)

---

## Prioritaetsreihenfolge (hoechste zuerst)

| Prio | Signal             | Bedeutung                    |
| ---- | ------------------ | ---------------------------- |
| 6    | Dauerton           | Leckwasser                   |
| 5    | Stotter-Alarm      | Grenzwert ueberschritten     |
| 4    | Intervall-Piepen   | Heizungs-Fehlermeldung       |
| 3    | Morse-S (···)      | Heizung still (kein Signal)  |
| 2    | Rattern (zyklisch) | SD/Dateisystem-Problem (ESP) |
| 1    | 2 langsame Beeps   | WiFi unterbrochen (ESP)      |
