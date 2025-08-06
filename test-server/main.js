import { WebSocketServer } from 'ws'

const ws = new WebSocketServer({ port: 8080 });

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

console.log('WebSocket server is running on ws://localhost:8080')
