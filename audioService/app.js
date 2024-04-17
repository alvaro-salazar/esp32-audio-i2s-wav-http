const express = require('express');
const app = express();
const fs = require('fs');
const port = 8888;

app.use(express.raw({type: 'audio/wav', limit: '50mb'}));

app.post('/uploadAudio', (req, res) => {
    const audioData = req.body;
    console.log(`Recibido audio con tama$o: ${audioData.length}`);
    fs.writeFileSync('audio_received.wav', audioData);
    res.send('Audio recibido con exito');
});

app.listen(port, () => {
    console.log(`Servidor escuchando en http://localhost:${port}`);
});
 
// Middleware de manejo de errores
app.use((err, req, res, next) => {
    console.error('Se ha producido un error:', err);
    if (err.type === 'entity.too.large') {
        // Error especifico por size de entidad demasiado grande
        return res.status(413).send('Archivo demasiado grande');
    }
    // Para otros tipos de errores, puedes agregar mas condiciones aqu!

    // Si no se ha manejado el error especificamente, envia una respuesta generica
    return res.status(500).send('Ocurrio un error en el servidor');
});
