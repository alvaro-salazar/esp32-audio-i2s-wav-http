/**
 *  MIT License

    Copyright (c) 2024 Alvaro Salazar

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/


#include "Arduino.h"
#include "driver/i2s.h"
#include "WiFi.h"

// Definición de pines para el micrófono INMP441
#define I2S_SCK_PIN  18             // Pin Serial Clock (SCK) 
#define I2S_WS_PIN   15             // Pin Word Select (WS)
#define I2S_SD_PIN   13             // Pin Serial Data (SD)

// Configuración del driver I2S para el micrófono INMP441
#define I2S_PORT           I2S_NUM_0 // I2S port number (0)
#define SAMPLE_BUFFER_SIZE 1024      // Tamaño del buffer de muestras (1024)
#define I2S_SAMPLE_RATE    (16000)   // Frecuencia de muestreo (16 kHz)
#define RECORD_TIME        (10)      // segundos de grabación 

// Credenciales de la red WiFi
const char* ssid = "virus2";         // Reemplaza con el nombre de tu red WiFi
const char* password = "a1b2c3d4";   // Reemplaza con tu contraseña WiFi

// Servidor HTTP (IP y puerto)
const char* serverName = "192.168.137.1";
const int port = 8888;

// Constantes para la cabecera del archivo WAV
const int HEADERSIZE = 44;

// Cliente WiFi y cliente HTTP
WiFiClient cliente;

// Prototipos de funciones
void micTask(void* parameter);
void setWavHeader(uint8_t* header, int wavSize);

// Buffer de muestras de audio (32 bits) y buffer de muestras de audio (8 bits)
int32_t i2s_read_buffer[SAMPLE_BUFFER_SIZE];
int8_t i2s_read_buff8[SAMPLE_BUFFER_SIZE*sizeof(int32_t)];

// Estructura de parámetros para la tarea del micrófono
struct MicTaskParameters {
    int duracion;
    int frecuencia;
    int bufferSize;
} micParams;



// Implementacion de las funciones


/**
 * @brief Función de configuración del driver I2S para el micrófono INMP441
 */
void i2s_config_setup() {
    const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),  // Modo RX (recepción)
        .sample_rate = I2S_SAMPLE_RATE, // Frecuencia de muestreo
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // Ajustado para alineación de 32 bits
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // El INMP441 es mono, usar solo canal izquierdo
        .communication_format = I2S_COMM_FORMAT_STAND_I2S, // Ajustado para alineación de 32 bits (MSB)
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // Interrupt level 1 (1-7) significa prioridad baja
        .dma_buf_count = 40,    // Número de buffers, 8
        .dma_buf_len = 1024,    // Tamaño de cada buffer
        .use_apll = false,      // No usar APLL para la frecuencia de muestreo (no es necesario, pero es más preciso)
    };

    // Configuración de pines para I2S (I2S0) en el ESP32
    const i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK_PIN,          // Serial Clock (SCK): Señal de reloj de bits
        .ws_io_num = I2S_WS_PIN,            // Word Select (WS): Señal de reloj de muestreo (LRCLK)
        .data_out_num = I2S_PIN_NO_CHANGE,  // No se usa en modo RX (recepción): DOUT
        .data_in_num = I2S_SD_PIN  // DIN   // Serial Data (SD): Datos de audio
    };

    // Instalar el driver de I2S con la configuración anterior y sin buffer de eventos (0) ni callback (NULL)
    if(ESP_OK != i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL)) {
        Serial.println("i2s_driver_install: error");
    }
    
    // Configurar los pines del ESP32 para el I2S (I2S0) con la configuración anterior (pin_config)
    if(ESP_OK != i2s_set_pin(I2S_PORT, &pin_config)) {
        Serial.println("i2s_set_pin: error");
    }
}

/**
 * @brief Función de configuración de la placa ESP32 y conexión a la red WiFi
 * Se ejecuta una sola vez al inicio del programa
 */
void setup() {

    Serial.begin(230400);                   // Iniciar el puerto serie a 230400 baudios

    WiFi.begin(ssid, password);             // Conectar a la red WiFi con las credenciales proporcionadas
    Serial.println("Conectando a WiFi");

    while (WiFi.status() != WL_CONNECTED) { // Esperar a que se conecte a la red WiFi
        delay(1000);                        // Esperar un segundo
        Serial.println(".");                // Imprimir un punto cada segundo
    }
    Serial.println("Conectado a WiFi");     // Imprimir mensaje de conexión exitosa
    Serial.print("IP local: ");             // Imprimir mensaje de dirección IP local
    Serial.println(WiFi.localIP());         

    if(!cliente.connect(serverName, port)){ // Conectar al servidor HTTP en el puerto 8888 (HTTP)
     Serial.println("Conexion fallida!");   // Imprimir mensaje de error si la conexión falla
     delay(10000);                          // Esperar 10 segundos
     return;                                // Salir de la función setup()
   }
   Serial.println("Conectado al servidor"); // Imprimir mensaje de conexión exitosa

   // Crear una estructura de parámetros para la tarea del micrófono
    
    micParams.duracion = RECORD_TIME;  // Duración de la grabación en segundos
    micParams.frecuencia = I2S_SAMPLE_RATE;  // Frecuencia de muestreo de 16 kHz (16000 muestras por segundo)
    micParams.bufferSize = SAMPLE_BUFFER_SIZE;  // Tamaño del buffer de muestras (1024 muestras)
    Serial.println("Configuracion de la tarea del microfono");
    Serial.println("Duracion: " + String(micParams.duracion));
    Serial.println("Frecuencia: " + String(micParams.frecuencia));
    Serial.println("Tamaño del buffer: " + String(micParams.bufferSize));
   // Crea tarea para leer muestras de audio del micrófono y enviarlas al servidor
   // - Asigna 10000 bytes de stack, los bytes de stack deben ser suficientes para la tarea y sirven 
   //   para guardar las variables locales de la tarea
   // - Los parámetros de la tarea se pasan como un puntero en la estructura micParams
   // - La prioridad 1 es la más baja (0-24), y el núcleo asignado a la tarea es el núcleo 1 del ESP32. 
   //   (La prioridad 0 se reserva para el sistema)
   xTaskCreatePinnedToCore(micTask, "micTask", 10000, &micParams, 1, NULL, 1);

}


/**
 * @brief Función principal del programa (bucle infinito)
 */
void loop() {
  // No se hace nada en el loop
}


/**
 * @brief Tarea para leer muestras de audio del micrófono I2S y enviarlas a un servidor HTTP
 * @param parameter Puntero a los parámetros de la tarea (no se usa)
 */
void micTask(void* parameter) {
    struct MicTaskParameters {
        int duracion;
        int frecuencia;
        int bufferSize;
    };
    // Convertir el puntero a los parámetros de la tarea a un puntero de tipo MicTaskParameters
    MicTaskParameters * params = (MicTaskParameters *) parameter;
    int duracion = params->duracion;        // Duración de la grabación en segundos
    int frecuencia = params->frecuencia;    // Frecuencia de muestreo de 16 kHz (16000 muestras por segundo)
    int bufferSize = params->bufferSize;    // Tamaño del buffer de muestras (1024 muestras)
    int numBuffers = (int)(params->duracion * params->frecuencia / bufferSize);  // Número de buffers a enviar al servidor
    int recordBytes = numBuffers * params->bufferSize * sizeof(int16_t);  
    
    i2s_config_setup();             // Configurar el I2S para la lectura de muestras de audio
    
    size_t elements_read=0;         // Número de elementos leídos del I2S
    uint8_t header[HEADERSIZE];     // Buffer para la cabecera del archivo WAV (44 bytes)

    setWavHeader(header, recordBytes); // Configurar la cabecera del archivo WAV (44 bytes)

    // Enviar la petición HTTP POST al servicio indicando el tamaño del archivo WAV
    cliente.println("POST /uploadAudio HTTP/1.1");                          // Método POST y ruta del servidor 
    cliente.println("Content-Type: audio/wav");                             // Tipo de contenido de audio WAV
    cliente.println("Content-Length: " + String(recordBytes+HEADERSIZE));   // Usa el tamaño del archivo aquí
    cliente.println("Host: " + String(serverName));
    cliente.println("Connection: keep-alive");
    cliente.println();                 // Línea en blanco para indicar el fin de las cabeceras HTTP
    cliente.write(header, HEADERSIZE); // Este es el body de la petición HTTP POST (la cabecera del archivo WAV)
    
    //Eliminamos los primeros 8 buffers para que no se envie ruido (se descartan los primeros 512ms de grabacion)
    for(int i=0; i<8; i++){
        i2s_read(I2S_PORT, &i2s_read_buffer, SAMPLE_BUFFER_SIZE*sizeof(int32_t), &elements_read, portMAX_DELAY);
    }
    
    // Leer muestras de audio del micrófono y enviarlas al servidor
    for(int j = 0; j < numBuffers; j++ ){
        // Leer muestras de audio del micrófono (I2S) y enviarlas al servidor (HTTP) 
        // un total de SAMPLE_BUFFER_SIZE veces es decir 1024 veces ya que el buffer es de 1024 muestras de 32 bits
        // cada una y un portMAX_DELAY para que espere indefinidamente hasta que se llene el buffer de muestras
        esp_err_t result = i2s_read(I2S_PORT, &i2s_read_buffer, SAMPLE_BUFFER_SIZE*sizeof(int32_t), &elements_read, portMAX_DELAY);
        if (result != ESP_OK) {
            Serial.println("Error en la lectura de I2S");
            while(1);
        } else {
            for(int i=0; i<SAMPLE_BUFFER_SIZE; i++){                            // Convertimos los datos de 32 bits a 16 bits (2 bytes por muestra)
                i2s_read_buff8[2*i]   = (int8_t) (i2s_read_buffer[i]>>24&0xFF); // Convertimos los datos de 32 bits (8 bits mas altos) a 8 bits MSB
                i2s_read_buff8[2*i+1] = (int8_t) (i2s_read_buffer[i]>>16&0xFF); // Convertimos los datos de 32 bits (8 bits del bit 23 al 16) a 8 bits LSB
            }
            cliente.write((uint8_t *) i2s_read_buff8, SAMPLE_BUFFER_SIZE*sizeof(int16_t));
        }
    }   
    cliente.flush();

    Serial.println("Se enviaron " + String(recordBytes+HEADERSIZE) + " bytes al servicio");

    unsigned long tiempoInicial = millis();     // Guardar el tiempo actual en milisegundos
    while(cliente.available()==0){              // Mientras no haya datos disponibles
        if(millis() - tiempoInicial > 5000){    // Reviso si ya pasaron mas de 5 segundos
            Serial.println("Expiro el tiempo de espera");
            cliente.stop();                     // Detengo la conexion al servicio
            while(1);                           // Me quedo en un bucle infinito
        }
    }

    while(cliente.available()){                         // Mientras haya datos disponibles
        String linea = cliente.readStringUntil('\r');   // Leer una línea de texto hasta el enter
        Serial.println(linea);                          // Imprimir la línea de texto
    }
    Serial.println("Fin de conexion");
    cliente.stop();                             // Detener la conexión al servidor
    while(1);                                   // Bucle infinito para detener la tarea
}



/**
 * @brief Función para configurar el encabezado de un archivo WAV
 * @param header Puntero a la cabecera del archivo WAV (un arreglo de 44 bytes)
 * @param wavSize Tamaño del archivo WAV en bytes (sin incluir la cabecera)
 */
void setWavHeader(uint8_t* header, int wavSize){
  int fileSize = wavSize + HEADERSIZE - 8; // Tamaño del archivo WAV en bytes (sin incluir los primeros 8 bytes)
  Serial.println("Tamaño del archivo: " + String(fileSize));
  // Formato de la cabecera del archivo WAV (44 bytes)
  header[0] = 'R';  // RIFF: Marca el archivo como archivo riff (Resource Interchange File Format)
  header[1] = 'I';  // |
  header[2] = 'F';  // |
  header[3] = 'F';  // |______
  header[4] = (uint8_t)(fileSize & 0xFF);         // Tamaño de todo el archivo - 8 bytes, en bytes (32-bit integer)
  header[5] = (uint8_t)((fileSize >> 8) & 0xFF);  // |
  header[6] = (uint8_t)((fileSize >> 16) & 0xFF); // |
  header[7] = (uint8_t)((fileSize >> 24) & 0xFF); // |______   
  header[8] = 'W';  // WAVE: Cabecera del tipo de archivo
  header[9] = 'A';  // |
  header[10] = 'V'; // |
  header[11] = 'E'; // |______
  header[12] = 'f'; // fmt: Marcador de fragmento de formato. Incluye null al final
  header[13] = 'm'; // |
  header[14] = 't'; // |
  header[15] = ' '; // |______
  header[16] = 0x10; // Longitud del formato de los datos (16 bytes)
  header[17] = 0x00; // |
  header[18] = 0x00; // |
  header[19] = 0x00; // |______
  header[20] = 0x01; // Tipo de formato 1 (PCM: Pulse Code Modulation)
  header[21] = 0x00; // |______
  header[22] = 0x01; // Numero de canales: 1
  header[23] = 0x00; // |______
  header[24] = 0x80; // Frecuencia de muestreo: 00003E80 (16000 Hz)
  header[25] = 0x3E; // |
  header[26] = 0x00; // |
  header[27] = 0x00; // |______
  header[28] = 0x00; // (Frecuencia de muestreo * (Numero de Bits por muestra) * Numero de Canales) / 8.
  header[29] = 0x7D; // | 0x00007D00 = 32000 = 16000 * 16 * 1 / 8
  header[30] = 0x00; // | 
  header[31] = 0x00; // |______
  header[32] = 0x02; // (Numero de bits por muestra * Numero de canales) / 8 = (1: 8 bit mono) (2: 8 bit stereo o 16 bit mono) (4: 16 bit stereo)
  header[33] = 0x00; // |______
  header[34] = 0x10; // Bits por muestra: 16
  header[35] = 0x00; // |______
  header[36] = 'd';  // Data: Marca el comienzo de una seccion de datos
  header[37] = 'a';  // |
  header[38] = 't';  // |
  header[39] = 'a';  // |______
  header[40] = (uint8_t)(wavSize & 0xFF);         // Tamaño de la seccion de datos, en bytes (32-bit integer)
  header[41] = (uint8_t)((wavSize >> 8) & 0xFF);  // |
  header[42] = (uint8_t)((wavSize >> 16) & 0xFF); // |
  header[43] = (uint8_t)((wavSize >> 24) & 0xFF); // |______
  
}