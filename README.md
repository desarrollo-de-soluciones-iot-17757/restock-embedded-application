# Restock Embedded Application

Restock Embedded Application es el proyecto de firmware para el dispositivo de balanza inteligente utilizado dentro del ecosistema IoT de Restock. Se ejecuta sobre un microcontrolador ESP32 y coordina la lectura de sensores ambientales, medición de peso, visualización local, aprovisionamiento con el Edge Service y publicación de telemetría mediante MQTT.

El dispositivo permite monitorear las condiciones del área de almacenamiento y las mediciones físicas del inventario, recolectando datos en tiempo real sobre peso del producto, temperatura, humedad, unidades convertidas y estado del dispositivo.

## Descripción general

La aplicación embebida está diseñada para un dispositivo de balanza inteligente orientado al monitoreo automático de inventario. Integra sensores y actuadores para capturar datos relevantes del entorno y del stock, mostrar información localmente y comunicar telemetría al Restock Edge Service a través de un broker MQTT.

El dispositivo es responsable de:

* Leer condiciones ambientales.
* Leer el peso del stock mediante celdas de carga.
* Mostrar información local en una pantalla LCD.
* Gestionar eventos del dispositivo mediante el framework ModestIoT.
* Solicitar credenciales MQTT y configuración de visualización al Edge Service.
* Publicar telemetría al broker MQTT.
* Apoyar el monitoreo de inventario en tiempo real.

## Funcionalidades principales

* Monitoreo de temperatura y humedad mediante sensor DHT22.
* Medición de peso mediante integración de HX711 y celdas de carga.
* Visualización local mediante pantalla LCD1602 I2C.
* Comportamiento orientado a eventos usando componentes de ModestIoT.
* Aprovisionamiento inicial con el Edge Service mediante HTTP.
* Publicación de telemetría mediante MQTT autenticado.
* Visualización de unidades de producto según configuración enviada por el Edge.
* Monitoreo serial para depuración.
* Ejecución sobre ESP32.
* Soporte para simulación en Wokwi.
* Flujo compatible con Arduino CLI y Arduino IDE.

## Componentes de hardware

El dispositivo embebido se basa en los siguientes componentes:

| Componente      | Propósito                                 |
| --------------- | ----------------------------------------- |
| ESP32 DevKit    | Microcontrolador principal                |
| DHT22           | Medición de temperatura y humedad         |
| HX711           | Amplificación de señal de celdas de carga |
| Celdas de carga | Medición de peso                          |
| LCD1602 I2C     | Visualización local                       |
| Protoboard      | Prototipado del circuito                  |

## Stack de software

Este proyecto utiliza:

* C++
* Arduino framework
* Arduino CLI
* Arduino IDE
* Wokwi Simulator
* ModestIoT Nano-framework
* ArduinoJson
* PubSubClient
* DHT sensor library
* LiquidCrystal I2C

## Arquitectura

El firmware sigue un diseño orientado a objetos y basado en eventos, tomando como base el ModestIoT Nano-framework. La aplicación se organiza alrededor de abstracciones de dispositivo, sensor, actuador, evento, comando, conectividad y telemetría.

La clase principal del dispositivo coordina el comportamiento de la balanza inteligente y delega responsabilidades a componentes del framework y adaptadores específicos de Restock.

Las responsabilidades principales incluyen:

* Gestión del ciclo de vida del dispositivo.
* Programación de lectura de sensores.
* Manejo de eventos.
* Ejecución de comandos sobre el LCD.
* Preparación de telemetría ambiental.
* Preparación de telemetría de peso.
* Aprovisionamiento con el Edge Service.
* Publicación de telemetría mediante MQTT.

## Uso del framework ModestIoT

Este proyecto utiliza el ModestIoT Nano-framework como capa base para el comportamiento del dispositivo embebido.

La aplicación usa componentes del framework como:

| Componente del framework | Uso                                                              |
| ------------------------ | ---------------------------------------------------------------- |
| `Device`                 | Clase base para el mediador principal de la aplicación embebida  |
| `DhtSensor`              | Lectura de temperatura y humedad                                 |
| `CharacterLcdDisplay`    | Control de la pantalla LCD1602 I2C                               |
| `TelemetryPackage`       | Abstracción base para payloads JSON de telemetría                |
| `WiFiConnectivityDriver` | Gestión de conectividad WiFi del ESP32                           |
| `ConnectivityDriver`     | Abstracción genérica del transporte de comunicación              |
| `Event`                  | Notificación de datos nuevos desde sensores hacia el dispositivo |
| `Sensor`                 | Abstracción base para componentes de sensado programados         |

Las clases específicas de Restock extienden o complementan el framework cuando la versión actual de ModestIoT no cubre una funcionalidad requerida por el proyecto.

## Componentes específicos de Restock

| Archivo                            | Responsabilidad                                                      |
| ---------------------------------- | -------------------------------------------------------------------- |
| `SuppliesKeeperDevice.ino`         | Sketch principal de Arduino y coordinador del dispositivo            |
| `Config.h`                         | Configuración del dispositivo, WiFi, Edge, MQTT, sensores y pantalla |
| `DisplayMode.h`                    | Definición de modos de visualización en LCD                          |
| `EnvironmentTelemetryPackage.h`    | Payload de telemetría de temperatura y humedad                       |
| `WeightTelemetryPackage.h`         | Payload de telemetría de peso y unidades convertidas                 |
| `EdgeProvisioningClient.h`         | Solicita credenciales MQTT y configuración al Edge Service           |
| `AuthenticatedMqttGatewayClient.h` | Publica telemetría al broker MQTT usando credenciales del Edge       |
| `diagram.json`                     | Definición del circuito en Wokwi                                     |
| `libraries.txt`                    | Lista de librerías externas requeridas por Wokwi                     |

## Flujo de aprovisionamiento con Edge

Cuando el ESP32 inicia, el dispositivo se conecta a WiFi y envía una solicitud de aprovisionamiento al Restock Edge Service.

La solicitud identifica al dispositivo usando:

* Device ID
* Dirección MAC
* API key enviada mediante un header HTTP

Ejemplo de solicitud:

```http
POST /api/v1/edge/devices/provision
Content-Type: application/json
X-API-Key: dev-restock-api-key

{
  "device_id": "supplies-keeper-001",
  "mac_address": "24:0A:C4:00:01:10"
}
```

El Edge Service valida el dispositivo y devuelve los datos de conexión MQTT y la configuración de visualización.

Ejemplo de respuesta:

```json
{
  "authorized": true,
  "mqtt_client_id": "supplies-keeper-001",
  "mqtt_user": "device_3498",
  "mqtt_pass": "secure_token",
  "mqtt_broker_host": "broker.hivemq.com",
  "mqtt_broker_port": 1883,
  "telemetry_topic": "restock/edges/edge-001/devices/supplies-keeper-001/telemetry",
  "display_mode": "converted_units",
  "product_unit_label": "botellas",
  "converted_quantity": 4
}
```

Si el Edge Service no se encuentra disponible durante el desarrollo, el dispositivo puede usar una configuración local de fallback para MQTT.

## Flujo de telemetría MQTT

Después del aprovisionamiento, el dispositivo se conecta al broker MQTT usando las credenciales devueltas por el Edge Service.

El dispositivo publica telemetría en el topic configurado:

```txt
stores/supplies-keeper-001/telemetry/environment
stores/supplies-keeper-001/telemetry/weight
stores/supplies-keeper-001/health
```

El Edge Service se suscribe a ese topic y consume la telemetría generada por el dispositivo embebido.

Ejemplo de telemetría ambiental:

```json
{
  "device_id": "supplies-keeper-001",
  "temperature": 24,
  "humidity": 66,
  "created_at": "2026-08-14T06:19:12Z"
}
```

Ejemplo de telemetría de peso:

```json
{
  "device_id": "supplies-keeper-001",
  "raw_weight": 500.0,
  "created_at": "2026-08-14T06:19:12Z"
}
```

## Comportamiento del display

La pantalla LCD muestra información local del dispositivo según el modo de visualización devuelto por el Edge Service.

Los modos de visualización soportados son:

| Modo              | Descripción                                                                            |
| ----------------- | -------------------------------------------------------------------------------------- |
| `environment`     | Muestra temperatura y humedad                                                          |
| `temperature`     | Muestra solo temperatura                                                               |
| `humidity`        | Muestra solo humedad                                                                   |
| `weight`          | Muestra el peso del stock                                                              |
| `converted_units` | Muestra unidades convertidas del producto, como botellas, cajas, paquetes o kilogramos |

La temperatura siempre se muestra en Celsius. Las unidades de producto no se usan para convertir temperatura, sino para representar la unidad del producto monitoreado en inventario.

Ejemplo de salida en pantalla:

```txt
4 botellas
T24.0C H66%
```

## Estructura del proyecto

```txt
restock-embedded-application/
├── README.md
├── .gitignore
└── SuppliesKeeperDevice/
    ├── AuthenticatedMqttGatewayClient.h
    ├── Config.h
    ├── DisplayMode.h
    ├── EdgeProvisioningClient.h
    ├── EnvironmentTelemetryPackage.h
    ├── WeightTelemetryPackage.h
    ├── SuppliesKeeperDevice.ino
    ├── diagram.json
    ├── libraries.txt
    └── wokwi-project.txt
```

## Configuración de Arduino CLI

Instalar Arduino CLI y configurar el soporte para placas ESP32:

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

Instalar las librerías requeridas:

```bash
arduino-cli lib install ArduinoJson
arduino-cli lib install PubSubClient
arduino-cli lib install "DHT sensor library"
arduino-cli lib install "Adafruit Unified Sensor"
arduino-cli lib install "LiquidCrystal I2C"
arduino-cli lib install ESP32Servo
```

El ModestIoT Nano-framework debe estar disponible como librería de Arduino. Para compilación local, colocarlo dentro de la carpeta de librerías de Arduino, por ejemplo:

```txt
Documents/Arduino/libraries/Modest-IoT-Nano-Framework
```

La carpeta debe contener directamente los archivos fuente del framework y su metadata de librería Arduino.

## Compilación

Para compilar el firmware con Arduino CLI, ingresar a la carpeta del sketch:

```bash
cd SuppliesKeeperDevice
```

Luego ejecutar:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 .
```

## Carga a ESP32 físico

Para cargar el firmware a un ESP32 físico, conectar la placa por USB e identificar el puerto serial.

Luego ejecutar:

```bash
arduino-cli upload -p COM_PORT --fqbn esp32:esp32:esp32 .
```

Reemplazar `COM_PORT` por el puerto real usado por el ESP32, por ejemplo:

```bash
arduino-cli upload -p COM5 --fqbn esp32:esp32:esp32 .
```

## Monitor serial

Para monitorear los logs del dispositivo, ejecutar:

```bash
arduino-cli monitor -p COM_PORT -c baudrate=115200
```

Ejemplo:

```bash
arduino-cli monitor -p COM5 -c baudrate=115200
```

## Simulación en Wokwi

El proyecto incluye soporte para Wokwi mediante:

* `diagram.json`
* `libraries.txt`
* `wokwi-project.txt`

La simulación en Wokwi incluye ESP32, sensor DHT22, integración con HX711 y celdas de carga, pantalla LCD1602 I2C y cableado sobre protoboard.

El circuito actual utiliza el pin `3V3` del ESP32 como riel principal de alimentación para mantener compatibilidad eléctrica con el ESP32.

Flujo recomendado:

1. Abrir el proyecto en Wokwi.
2. Verificar que todos los archivos de `SuppliesKeeperDevice` estén incluidos.
3. Confirmar que `libraries.txt` contenga las librerías externas requeridas.
4. Iniciar la simulación.
5. Revisar la salida del LCD y los logs del monitor serial.
6. Confirmar la publicación de telemetría MQTT.

Librerías requeridas en Wokwi:

```txt
ArduinoJson
PubSubClient
DHT sensor library
Adafruit Unified Sensor
LiquidCrystal I2C
ESP32Servo
Modest-IoT-Nano-Framework@wokwi:62daf647225b408c88122c9dae03ff171e0a2600
```

## Notas de desarrollo

La implementación actual incluye adaptadores específicos de Restock debido a que la versión actual del ModestIoT Nano-framework no cubre completamente todos los comportamientos de comunicación requeridos por el proyecto.

Estos adaptadores son:

* `EdgeProvisioningClient.h`
* `AuthenticatedMqttGatewayClient.h`

Estos archivos podrían simplificarse o eliminarse en el futuro si el framework ModestIoT agrega soporte para:

* Aprovisionamiento Edge-Device.
* Autenticación MQTT con usuario y contraseña.
* Modelo genérico de respuesta de aprovisionamiento.
* Soporte nativo para configuración de unidades de inventario de Restock.

## Propósito del proyecto

Esta aplicación embebida forma parte de la solución Restock Smart Inventory. Su propósito es conectar el entorno físico del inventario con la plataforma de software, recolectando datos ambientales y de peso, mostrando estado local y enviando telemetría al Restock Edge Service para procesamiento, monitoreo y apoyo en la toma de decisiones.

## Créditos

Este proyecto utiliza el ModestIoT Nano-framework creado por Angel Velasquez como framework base para abstracciones de dispositivo, sensor, actuador, comando, evento, conectividad y telemetría.

Los componentes específicos de Restock y el comportamiento de la aplicación embebida son desarrollados por el equipo Restock.

## Licencia

Este proyecto está destinado a uso académico como parte de la solución IoT de Restock.
