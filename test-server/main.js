import { WebSocketServer } from 'ws'
import https from 'https';
import fs from 'fs';

const options = {
    key: fs.readFileSync('key.pem'),
    cert: fs.readFileSync('cert.pem')
};

const httpsServer = https.createServer(options);
const ws = new WebSocketServer({ server: httpsServer });

ws.on('connection', (socket) => {
    console.log('Client connected');
    socket.send(`hi im the server how do you do`);

    socket.on('message', (message) => {
        console.log(`Received: ${message}`);
        socket.send(`you said: ${message}`)
    });

    socket.on('close', () => {
        console.log('Client disconnected');
    });
});

httpsServer.listen(8080, () => {
    console.log('WebSocket server is running on wss://localhost:8080');
});
