Protocolo de Pruebas
Objetivo

Verificar que el sistema sea capaz de medir la concentración de CO₂ en tiempo real y mostrar el estado de la calidad del aire al usuario.

Equipamiento
Arduino UNO R4 WiFi
Sensor MH-Z19
Pantalla TFT ST7735
Dashboard web
Pruebas realizadas
Prueba    Resultado
Encendido del sistema    Correcto
Lectura del sensor CO₂    Correcta
Visualización en pantalla    Correcta
Envío de datos al dashboard    Correcto
Clasificación de calidad del aire    Correcta
Resultados
El sistema detectó correctamente cambios en la concentración de CO₂.
La pantalla mostró el valor en ppm y la clasificación correspondiente.
El dashboard actualizó las mediciones en tiempo real.
Problema encontrado

Durante el desarrollo se presentaron problemas de comunicación entre el sensor y el Arduino.

Solución aplicada

Se revisaron las conexiones UART y se ajustó el código hasta obtener lecturas estables.

Validación

El sistema cumple con el objetivo de informar la calidad del aire en tiempo real y apoyar la decisión de ventilar una sala cuando la concentración de CO₂ supera los niveles recomendados.
