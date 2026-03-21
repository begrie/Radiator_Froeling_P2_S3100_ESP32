# Froeling P2 ESP32

## Praxis-Testplan (Kurzcheck vor Ort)

1. **WLAN-Ausfall testen (Prio 1):** WLAN am Router kurz deaktivieren oder ESP aus dem WLAN nehmen -> nach kurzer Zeit muss das Prio-1-Muster zyklisch kommen; nach Wiederverbindung muessen die Alarme verschwinden.
2. **Dateisystem-Hinweis pruefen (Prio 2):** Nur kontrolliert im Servicefall ausloesen (z. B. mit bekannter Testprozedur) -> Rattern muss zyklisch wiederkehren.
3. **Heizung still pruefen (Prio 3):** Serielle Verbindung testweise trennen (Servicefenster) -> nach Timeout muss Morse-S aktiv werden; nach Wiederverbindung wieder aus.
4. **Heizungsfehler pruefen (Prio 4):** Bei realer Heizungsstoerung oder Service-Simulation pruefen -> Intervall-Piepen aktiv; erst an Heizung quittieren, dann gelbe Quit-Taste an ESP testen.
5. **Grenzwertalarm pruefen (Prio 5):** Mit sicherem Testwert/Simulation ueber Schwellwert gehen -> Stotter-Alarm muss sofort aktiv werden und bei Normalwert wieder enden.
6. **Leckwasser pruefen (Prio 6):** Leckwassersensor gemaess Wartungsanleitung ausloesen -> Dauerton muss Prioritaet haben und andere Signale ueberlagern.
7. **Quit-Taste pruefen:** Bei aktivem Prio-3 bis Prio-6 Alarm gelbe Taste druecken -> sofort stumm, danach erneutes Signal nach ca. 1 Minute, falls Ursache weiter aktiv ist.
8. **Restart-Signal pruefen:** Kontrollierten Neustart ausloesen -> einmalig 3 mittlere Beeps; bei wiederholten Neustarts Syslog/MQTT und Stromversorgung pruefen.
