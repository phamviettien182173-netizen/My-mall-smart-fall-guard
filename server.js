const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const mqtt = require('mqtt');
const { Pool } = require('pg');

const app = express();
const server = http.createServer(app);
const io = new Server(server, { cors: { origin: '*' } });

// 1. Kết nối PostgreSQL
const pool = new Pool({
  user: 'fallguard',
  password: 'postgrespassword',
  host: 'localhost',
  database: 'fallguard_db',
  port: 5432,
});

// 2. Kết nối MQTT Broker
const mqttClient = mqtt.connect('mqtt://localhost:1883');

mqttClient.on('connect', () => {
  console.log('✅ Connected to MQTT Broker');
  mqttClient.subscribe('fallguard/v1/devices/+/+'); // Subcribe toàn bộ topic
});

// 3. Xử lý tin nhắn MQTT nhận được từ ESP8266
mqttClient.on('message', async (topic, message) => {
  const payloadStr = message.toString();
  console.log(`📩 MQTT [${topic}]:`, payloadStr);

  try {
    const data = JSON.parse(payloadStr);

    // Kịch bản A: Nhận Telemetry -> Lưu DB -> Đẩy Web vẽ đồ thị
    if (topic.includes('/telemetry')) {
      await pool.query(
        'INSERT INTO telemetry_logs (device_id, svm) VALUES ($1, $2)',
        [data.device_id, data.svm]
      );
      io.emit('telemetry_data', data);
    }

    // Kịch bản B: Nhận Cảnh báo ngã / SOS -> Lưu DB -> Đẩy Web chớp đỏ -> Bắn Telegram
    if (topic.includes('/alert')) {
      await pool.query(
        'INSERT INTO fall_events (device_id, event_type, svm_peak) VALUES ($1, $2, $3)',
        [data.device_id, data.event, data.svm_peak]
      );
      io.emit('emergency_alert', data); // Bắn tín hiệu nổ chuông trang Web
      sendTelegramAlert(data);         // Gửi tin nhắn đến điện thoại người thân
    }
  } catch (err) {
    // Xử lý dữ liệu chuỗi đơn giản nếu không phải JSON (như status ONLINE/OFFLINE)
    if (topic.includes('/status')) {
      io.emit('device_status', payloadStr);
    }
  }
});

// 4. Gửi cảnh báo Telegram khẩn cấp
async function sendTelegramAlert(alertData) {
  const BOT_TOKEN = 'YOUR_TELEGRAM_BOT_TOKEN'; // Điền Token Bot của bạn
  const CHAT_ID = 'YOUR_TELEGRAM_CHAT_ID';     // Điền Chat ID của bạn
  
  if (BOT_TOKEN === 'YOUR_TELEGRAM_BOT_TOKEN') return; // Bỏ qua nếu chưa cấu hình

  const text = `🚨 CANH BAO KHAN CAP! 🚨\n\nThiết bị: ${alertData.device_id}\nSự kiện: ${alertData.event}\nLực va chạm (SVM): ${alertData.svm_peak}g`;
  const url = `https://api.telegram.org/bot${BOT_TOKEN}/sendMessage?chat_id=${CHAT_ID}&text=${encodeURIComponent(text)}`;
  
  try {
    const fetch = (await import('node-fetch')).default;
    await fetch(url);
  } catch (e) {
    console.error('Lỗi gửi Telegram:', e);
  }
}

// Serves file giao diện HTML
app.get('/', (req, res) => res.sendFile(__dirname + '/index.html'));

server.listen(3000, () => {
  console.log('🚀 Server running on http://localhost:3000');
});