// JavaScript клиент с поддержкой HTTPS
const https = require('https');
const fs = require('fs');

const options = {
    hostname: 'localhost',
    port: 9443,
    path: '/list',
    method: 'GET',
    rejectUnauthorized: false  // Для самоподписанных сертификатов
};

const req = https.request(options, (res) => {
    let data = '';
    res.on('data', (chunk) => { data += chunk; });
    res.on('end', () => { console.log(data); });
});

req.end();