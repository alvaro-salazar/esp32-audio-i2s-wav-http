const express = require('express');
const app = express();
const fs = require('fs');
const port = 8888;

app.use(express.raw({type: 'audio/wav', limit: '50mb'}));

// Middleware para registrar detalles de las solicitudes entrantes
app.use((req, res, next) => {
    console.log(`Solicitud recibida en ${new Date().toLocaleString()}`);
    console.log(`Ruta: ${req.path}`);
    console.log(`Método: ${req.method}`);
    console.log(`Tipo de contenido: ${req.get('Content-Type')}`);
    console.log(`Tamaño de la solicitud: ${req.get('Content-Length') || 0} bytes`);
    console.log(`Body (cuerpo) de la solicitud: ${req.body.toString().substring(0, 100)}`); // Limita el registro del cuerpo de la solicitud a los primeros 100 caracteres
    next();
});

app.post('/uploadAudio', (req, res) => {
    const audioData = req.body;
    console.log(`Recibido audio con tamaño: ${audioData.length}`);
    fs.writeFileSync('audio_received.wav', audioData);
    res.send('Audio recibido con éxito');
});

app.listen(port, () => {
    console.log(`Servidor escuchando en http://localhost:${port}`);
});

// Middleware de manejo de errores
app.use((err, req, res, next) => {
    console.error('Se ha producido un error:', err);
    console.error('Ruta: ', req.path);
    console.error('Método: ', req.method);
    console.error('Tipo de contenido: ', req.get('Content-Type'));
    console.error('Tamaño de la solicitud: ', req.get('Content-Length') || 0);
    console.error('Body (cuerpo) de la solicitud: ', req.body.toString().substring(0, 100)); // Limita el registro del cuerpo de la solicitud a los primeros 100 caracteres
    if (err.type === 'entity.too.large') {
        // Error especifico por size de entidad demasiado grande
        return res.status(413).send('Archivo demasiado grande');
    }
    // Para otros tipos de errores, puedes agregar mas condiciones aquí!

    // Si no se ha manejado el error especificamente, envia una respuesta generica
    return res.status(500).send('Ocurrió un error en el servidor');
});
