# ESP_IBS_Gateway
Forward data from Hella IBS 200 to MQTT Broker

Das Modul liest die Daten des Batteriesensors aus und schickt es an einen MQTT/Broker.
Die Verbindung zum IBS wird über die LIN Schnittstelle realisiert, zum MQTT-Broker via WLAN.

Damit der Stromverbrauch gesenkt wird, verwendet der ESP DeepSleep.
In der passiven Phase kommt man somit auf ca. 3mA.

Das Modul soll als Basis für weitere Sensoren dienen, z.b. Ultraschallmessung des Frischwassertanks im Wohnmobil.
Somit entfällt die Verlegung von Daten- oder Steuerleitungen.

MQTT erlautb eine asynchrone Anbindung, so dass die zentrale Bedieneinheit sich die Daten abholen kann, wenn sie es braucht.
Weiterhin lassen sich die Nachrichten im MQTT-Broker abgreifen, um das System zu analysieren.

ToDo:
  Das Modul soll sich drei WLAN-Netzwerke merken.
  Damit könnten sich die Sensoren mit dem Heimnetzwerk verbinden und die Daten zu Hause ohne Bedieneinheit ausgelesen werden.
