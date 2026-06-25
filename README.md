# Detector-CO2-
Sensor Inteligente de Monitoreo de CO₂ para Salas de Clases
Integrantes
Alonso Beltrán
Franka Geisse
Antonio Seguel
Vicente Torrealba

---

Descripción
Este proyecto consiste en un sistema IoT capaz de medir en tiempo real la concentración de dióxido de carbono (CO₂) en salas de clases mediante un sensor MH-Z19 conectado a un Arduino UNO R4 WiFi.

La información se muestra tanto en una pantalla integrada como en un dashboard web, permitiendo conocer el estado del aire y decidir cuándo ventilar el espacio.

---

Problema
En salas cerradas el CO₂ puede aumentar rápidamente, afectando la concentración y el bienestar de los estudiantes.

Nuestro sistema entrega información inmediata para mejorar la calidad del aire.

---

Hardware utilizado
Arduino UNO R4 WiFi
Sensor MH-Z19
Pantalla TFT ST7735
Comunicación WiFi
Fuente de alimentación

---

Software
El sistema:

mide CO₂
clasifica la calidad del aire
publica los datos mediante WiFi
muestra la información en un dashboard web
registra histórico
permite realizar pruebas de una hora
exporta resultados en CSV

---

Dashboard
El dashboard permite visualizar:

concentración actual de CO₂
calidad del aire
gráfico histórico
métricas de una prueba
exportación de resultados

---

Instalación
Abrir detector_co2_wifi4.ino en Arduino IDE.
Configurar SSID y contraseña WiFi.
Cargar el programa al Arduino.
Abrir dashboard co2.html.
Ingresar la IP mostrada por el Arduino.

---

Estructura
firmware/

dashboard/

hardware/

diseno-3d/

testing/

docs/
