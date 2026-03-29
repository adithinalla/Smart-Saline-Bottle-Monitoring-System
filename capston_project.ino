/*
 * SALINE BOTTLE ALERT SYSTEM
 * ESP8266-based liquid level monitoring with modern web UI
 * Components: ESP8266, HC-SR04 Ultrasonic Sensor
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

// REMOVED: ESP8266HTTPUpdateServer - causing errors

// Ultrasonic Sensor Pins
#define TRIG_PIN 5     // GPIO5 - Trigger pin
#define ECHO_PIN 4      // GPIO4 - Echo pin

// Bottle Configuration
#define BOTTLE_HEIGHT_CM 20.0  // Total height of bottle in cm
#define EMPTY_THRESHOLD 2.0     // cm from bottom to consider empty
#define FULL_THRESHOLD 18.0     // cm from bottom to consider full
#define UPDATE_INTERVAL 1000    // Update every 1 second

// WiFi Access Point Configuration
const char* ssid = "SalineBottleAlert";
const char* password = "12345678";

// Create Web Server
ESP8266WebServer server(80);

// System Variables
float distance = 0;
float liquidLevel = 0;
int percentage = 0;
String bottleStatus = "UNKNOWN";
bool isLow = false;
bool alertTriggered = false;
unsigned long lastMeasurement = 0;
int readings[10]; // For averaging
int readIndex = 0;

// Alert History
String historyTimes[20];
int historyPercentages[20];
String historyStatus[20];
int historyCount = 0;
unsigned long startTime;
float alertLevel = 10.0; // Alert at 10%

// HTML/CSS/JavaScript for Modern Web Interface
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Saline Bottle Alert System</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: 'Poppins', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        }

        :root {
            --primary: #8B5CF6;
            --primary-dark: #7C3AED;
            --secondary: #EC4899;
            --success: #10B981;
            --warning: #F59E0B;
            --danger: #EF4444;
            --dark: #1F2937;
            --light: #F9FAFB;
            --glass: rgba(255, 255, 255, 0.95);
            --glass-dark: rgba(31, 41, 55, 0.95);
        }

        body {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
            display: flex;
            justify-content: center;
            align-items: center;
            position: relative;
            overflow-x: hidden;
        }

        body::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: url('data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" opacity="0.1"><path d="M0 0 L100 100 M100 0 L0 100" stroke="white" stroke-width="1"/></svg>');
            pointer-events: none;
        }

        .container {
            max-width: 500px;
            width: 100%;
            background: var(--glass);
            backdrop-filter: blur(20px);
            border-radius: 40px;
            padding: 25px;
            box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5);
            border: 1px solid rgba(255, 255, 255, 0.3);
            animation: slideUp 0.6s cubic-bezier(0.16, 1, 0.3, 1);
            position: relative;
            z-index: 10;
        }

        @keyframes slideUp {
            from {
                opacity: 0;
                transform: translateY(50px);
            }
            to {
                opacity: 1;
                transform: translateY(0);
            }
        }

        .header {
            text-align: center;
            margin-bottom: 25px;
            position: relative;
        }

        .header h1 {
            font-size: 32px;
            font-weight: 700;
            background: linear-gradient(135deg, var(--primary), var(--secondary));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            margin-bottom: 5px;
            letter-spacing: -0.5px;
        }

        .header p {
            color: var(--dark);
            opacity: 0.7;
            font-size: 14px;
            font-weight: 500;
        }

        .bottle-icon {
            width: 80px;
            height: 80px;
            background: linear-gradient(135deg, var(--primary), var(--secondary));
            border-radius: 30px 30px 15px 15px;
            margin: 0 auto 15px;
            display: flex;
            align-items: center;
            justify-content: center;
            position: relative;
            overflow: hidden;
            box-shadow: 0 10px 30px rgba(139, 92, 246, 0.3);
        }

        .bottle-icon::after {
            content: '🧴';
            font-size: 40px;
        }

        .status-badge {
            display: inline-block;
            padding: 8px 20px;
            border-radius: 50px;
            font-weight: 600;
            font-size: 14px;
            margin-top: 10px;
            transition: all 0.3s ease;
        }

        .status-success {
            background: linear-gradient(135deg, var(--success), #059669);
            color: white;
            box-shadow: 0 4px 15px rgba(16, 185, 129, 0.3);
        }

        .status-warning {
            background: linear-gradient(135deg, var(--warning), #D97706);
            color: white;
            box-shadow: 0 4px 15px rgba(245, 158, 11, 0.3);
            animation: pulse 2s infinite;
        }

        .status-danger {
            background: linear-gradient(135deg, var(--danger), #DC2626);
            color: white;
            box-shadow: 0 4px 15px rgba(239, 68, 68, 0.3);
            animation: pulse 1s infinite;
        }

        @keyframes pulse {
            0%, 100% {
                opacity: 1;
            }
            50% {
                opacity: 0.8;
                transform: scale(1.05);
            }
        }

        .level-card {
            background: white;
            border-radius: 25px;
            padding: 25px;
            margin: 20px 0;
            box-shadow: 0 10px 30px rgba(0, 0, 0, 0.1);
        }

        .level-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 15px;
        }

        .level-title {
            font-size: 16px;
            font-weight: 600;
            color: var(--dark);
            opacity: 0.8;
        }

        .level-value {
            font-size: 36px;
            font-weight: 700;
            background: linear-gradient(135deg, var(--primary), var(--secondary));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }

        .progress-container {
            position: relative;
            height: 30px;
            background: #E5E7EB;
            border-radius: 15px;
            overflow: hidden;
            margin: 15px 0;
        }

        .progress-bar {
            height: 100%;
            background: linear-gradient(90deg, var(--primary), var(--secondary));
            border-radius: 15px;
            transition: width 0.5s cubic-bezier(0.4, 0, 0.2, 1);
            position: relative;
            overflow: hidden;
        }

        .progress-bar::after {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: linear-gradient(90deg, transparent, rgba(255, 255, 255, 0.3), transparent);
            animation: shimmer 2s infinite;
        }

        @keyframes shimmer {
            0% {
                transform: translateX(-100%);
            }
            100% {
                transform: translateX(100%);
            }
        }

        .level-stats {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
            margin-top: 15px;
        }

        .stat-item {
            text-align: center;
            padding: 10px;
            background: var(--light);
            border-radius: 15px;
        }

        .stat-label {
            font-size: 12px;
            color: var(--dark);
            opacity: 0.6;
            margin-bottom: 5px;
        }

        .stat-number {
            font-size: 18px;
            font-weight: 700;
            color: var(--primary);
        }

        .alert-panel {
            background: linear-gradient(135deg, #FEF3C7, #FDE68A);
            border-radius: 20px;
            padding: 20px;
            margin: 20px 0;
            border-left: 4px solid var(--warning);
            display: none;
            animation: slideIn 0.5s ease;
        }

        .alert-panel.show {
            display: block;
        }

        .alert-panel.danger {
            background: linear-gradient(135deg, #FEE2E2, #FECACA);
            border-left-color: var(--danger);
        }

        @keyframes slideIn {
            from {
                opacity: 0;
                transform: translateX(-20px);
            }
            to {
                opacity: 1;
                transform: translateX(0);
            }
        }

        .alert-title {
            display: flex;
            align-items: center;
            font-weight: 600;
            margin-bottom: 10px;
        }

        .alert-title svg {
            margin-right: 8px;
        }

        .alert-message {
            font-size: 14px;
            line-height: 1.5;
            margin-bottom: 10px;
        }

        .alert-time {
            font-size: 12px;
            opacity: 0.7;
        }

        .history-panel {
            background: white;
            border-radius: 20px;
            padding: 20px;
            margin: 20px 0;
        }

        .history-title {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: 15px;
        }

        .history-title h3 {
            font-size: 16px;
            font-weight: 600;
            color: var(--dark);
        }

        .history-list {
            max-height: 200px;
            overflow-y: auto;
        }

        .history-item {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 10px;
            border-bottom: 1px solid #E5E7EB;
        }

        .history-item:last-child {
            border-bottom: none;
        }

        .history-time {
            font-size: 12px;
            color: var(--dark);
            opacity: 0.6;
        }

        .history-level {
            font-weight: 600;
            font-size: 14px;
        }

        .history-status {
            padding: 3px 10px;
            border-radius: 50px;
            font-size: 11px;
            font-weight: 600;
        }

        .status-low {
            background: #FEE2E2;
            color: var(--danger);
        }

        .status-medium {
            background: #FEF3C7;
            color: var(--warning);
        }

        .status-high {
            background: #D1FAE5;
            color: var(--success);
        }

        .control-panel {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin: 20px 0;
        }

        .btn {
            padding: 15px;
            border: none;
            border-radius: 15px;
            font-weight: 600;
            font-size: 14px;
            cursor: pointer;
            transition: all 0.3s ease;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 5px;
        }

        .btn-primary {
            background: linear-gradient(135deg, var(--primary), var(--secondary));
            color: white;
        }

        .btn-primary:hover {
            transform: translateY(-2px);
            box-shadow: 0 10px 25px rgba(139, 92, 246, 0.4);
        }

        .btn-secondary {
            background: white;
            color: var(--dark);
            border: 1px solid #E5E7EB;
        }

        .btn-secondary:hover {
            background: var(--light);
        }

        .connection-status {
            display: flex;
            align-items: center;
            justify-content: center;
            margin-top: 20px;
            font-size: 12px;
            color: var(--success);
        }

        .connection-status span {
            width: 8px;
            height: 8px;
            background: var(--success);
            border-radius: 50%;
            margin-right: 5px;
            animation: blink 2s infinite;
        }

        @keyframes blink {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }

        .footer {
            text-align: center;
            margin-top: 20px;
            font-size: 11px;
            color: var(--dark);
            opacity: 0.5;
        }

        .threshold-slider {
            margin: 20px 0;
        }

        .slider-label {
            display: flex;
            justify-content: space-between;
            margin-bottom: 5px;
            font-size: 12px;
            color: var(--dark);
        }

        input[type=range] {
            width: 100%;
            height: 5px;
            border-radius: 5px;
            background: linear-gradient(90deg, var(--danger), var(--warning), var(--success));
            outline: none;
            -webkit-appearance: none;
        }

        input[type=range]::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 20px;
            height: 20px;
            background: white;
            border-radius: 50%;
            box-shadow: 0 2px 10px rgba(0, 0, 0, 0.2);
            cursor: pointer;
            border: 2px solid var(--primary);
        }

        .bottle-visualization {
            display: flex;
            justify-content: center;
            margin: 20px 0;
        }

        .bottle-container {
            width: 120px;
            height: 200px;
            background: linear-gradient(135deg, #E5E7EB, #D1D5DB);
            border-radius: 40px 40px 20px 20px;
            position: relative;
            overflow: hidden;
            box-shadow: 0 10px 30px rgba(0, 0, 0, 0.2);
        }

        .liquid {
            position: absolute;
            bottom: 0;
            left: 0;
            width: 100%;
            background: linear-gradient(135deg, var(--primary), var(--secondary));
            transition: height 0.5s cubic-bezier(0.4, 0, 0.2, 1);
            border-radius: 0 0 20px 20px;
        }

        .liquid::after {
            content: '';
            position: absolute;
            top: -10px;
            left: 0;
            width: 100%;
            height: 20px;
            background: rgba(255, 255, 255, 0.3);
            border-radius: 50%;
            filter: blur(5px);
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <div class="bottle-icon"></div>
            <h1>Saline Bottle Alert</h1>
            <p>Real-time Liquid Level Monitoring</p>
            <div class="status-badge" id="statusBadge">🟢 MONITORING</div>
        </div>

        <div class="level-card">
            <div class="level-header">
                <span class="level-title">Current Level</span>
                <span class="level-value" id="percentage">0%</span>
            </div>
            
            <div class="progress-container">
                <div class="progress-bar" id="progressBar" style="width: 0%"></div>
            </div>

            <div class="level-stats">
                <div class="stat-item">
                    <div class="stat-label">Distance</div>
                    <div class="stat-number" id="distance">0 cm</div>
                </div>
                <div class="stat-item">
                    <div class="stat-label">Liquid</div>
                    <div class="stat-number" id="liquidLevel">0 cm</div>
                </div>
                <div class="stat-item">
                    <div class="stat-label">Status</div>
                    <div class="stat-number" id="levelStatus">OK</div>
                </div>
            </div>
        </div>

        <div class="bottle-visualization">
            <div class="bottle-container">
                <div class="liquid" id="bottleLiquid" style="height: 0%"></div>
            </div>
        </div>

        <div class="alert-panel" id="alertPanel">
            <div class="alert-title">
                <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                    <circle cx="12" cy="12" r="10"></circle>
                    <line x1="12" y1="8" x2="12" y2="12"></line>
                    <line x1="12" y1="16" x2="12.01" y2="16"></line>
                </svg>
                Low Level Alert!
            </div>
            <div class="alert-message" id="alertMessage">
                Bottle is running low. Please refill soon.
            </div>
            <div class="alert-time" id="alertTime">Just now</div>
        </div>

        <div class="history-panel">
            <div class="history-title">
                <h3>📋 Alert History</h3>
                <span class="status-badge" style="padding: 3px 10px; font-size: 11px;">Last 20</span>
            </div>
            <div class="history-list" id="historyList">
                <div class="history-item">
                    <span class="history-time">--:--:--</span>
                    <span class="history-level">--%</span>
                    <span class="history-status status-high">--</span>
                </div>
            </div>
        </div>

        <div class="threshold-slider">
            <div class="slider-label">
                <span>⚠️ Empty Threshold</span>
                <span id="thresholdValue">10%</span>
            </div>
            <input type="range" id="thresholdSlider" min="0" max="30" value="10" step="1">
        </div>

        <div class="control-panel">
            <button class="btn btn-primary" onclick="calibrateSensor()">
                📏 Calibrate
            </button>
            <button class="btn btn-secondary" onclick="clearHistory()">
                🗑️ Clear History
            </button>
            <button class="btn btn-secondary" onclick="refreshData()">
                🔄 Refresh
            </button>
            <button class="btn btn-primary" onclick="testAlert()">
                🧪 Test Alert
            </button>
        </div>

        <div class="connection-status">
            <span></span>
            Connected to SalineBottleAlert
        </div>

        <div class="footer">
            Saline Bottle Alert System v2.0 | Real-time Monitoring
        </div>
    </div>

    <script>
        // Configuration
        let alertThreshold = 10;
        let lastAlert = 0;

        // Initialize slider
        document.getElementById('thresholdSlider').addEventListener('input', function(e) {
            alertThreshold = e.target.value;
            document.getElementById('thresholdValue').textContent = alertThreshold + '%';
            
            // Send to server
            fetch('/threshold?value=' + alertThreshold)
                .then(response => response.text())
                .then(data => console.log('Threshold updated'));
        });

        // Fetch data every second
        setInterval(fetchData, 1000);

        function fetchData() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    updateUI(data);
                })
                .catch(error => console.error('Error:', error));
        }

        function updateUI(data) {
            // Update basic info
            document.getElementById('percentage').textContent = data.percentage + '%';
            document.getElementById('distance').textContent = data.distance.toFixed(1) + ' cm';
            document.getElementById('liquidLevel').textContent = data.liquidLevel.toFixed(1) + ' cm';
            
            // Update progress bar
            document.getElementById('progressBar').style.width = data.percentage + '%';
            document.getElementById('bottleLiquid').style.height = data.percentage + '%';
            
            // Update status
            const statusBadge = document.getElementById('statusBadge');
            const levelStatus = document.getElementById('levelStatus');
            
            if (data.percentage <= 10) {
                statusBadge.className = 'status-badge status-danger';
                statusBadge.innerHTML = '🔴 CRITICAL - REFILL NEEDED';
                levelStatus.textContent = 'CRITICAL';
                levelStatus.style.color = '#EF4444';
            } else if (data.percentage <= 30) {
                statusBadge.className = 'status-badge status-warning';
                statusBadge.innerHTML = '🟡 LOW - Refill Soon';
                levelStatus.textContent = 'LOW';
                levelStatus.style.color = '#F59E0B';
            } else {
                statusBadge.className = 'status-badge status-success';
                statusBadge.innerHTML = '🟢 GOOD - Normal Level';
                levelStatus.textContent = 'GOOD';
                levelStatus.style.color = '#10B981';
            }
            
            // Handle alert
            const alertPanel = document.getElementById('alertPanel');
            if (data.percentage <= alertThreshold) {
                alertPanel.classList.add('show');
                alertPanel.classList.add('danger');
                document.getElementById('alertMessage').innerHTML = 
                    '⚠️ CRITICAL: Bottle is almost empty! Current level: ' + data.percentage + '%';
                
                // Trigger sound if available
                if (lastAlert < Date.now() - 10000) {
                    beep();
                    lastAlert = Date.now();
                }
            } else {
                alertPanel.classList.remove('show');
            }
            
            // Update history
            if (data.history && data.history.length > 0) {
                updateHistory(data.history);
            }
        }

        function updateHistory(history) {
            const historyList = document.getElementById('historyList');
            historyList.innerHTML = '';
            
            history.forEach(item => {
                const div = document.createElement('div');
                div.className = 'history-item';
                
                let statusClass = 'status-high';
                if (item.percentage <= 10) statusClass = 'status-low';
                else if (item.percentage <= 30) statusClass = 'status-medium';
                
                div.innerHTML = `
                    <span class="history-time">${item.time}</span>
                    <span class="history-level">${item.percentage}%</span>
                    <span class="history-status ${statusClass}">${item.status}</span>
                `;
                
                historyList.appendChild(div);
            });
        }

        function calibrateSensor() {
            fetch('/calibrate')
                .then(response => response.text())
                .then(data => {
                    showNotification('Sensor calibrated! Full level: ' + data);
                });
        }

        function clearHistory() {
            fetch('/clear')
                .then(response => response.text())
                .then(data => {
                    showNotification('History cleared!');
                });
        }

        function refreshData() {
            fetchData();
            showNotification('Data refreshed!');
        }

        function testAlert() {
            fetch('/test')
                .then(response => response.text())
                .then(data => {
                    showNotification('Test alert triggered!');
                });
        }

        function beep() {
            // Create a simple beep sound using Web Audio API
            const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
            const oscillator = audioCtx.createOscillator();
            const gainNode = audioCtx.createGain();
            
            oscillator.connect(gainNode);
            gainNode.connect(audioCtx.destination);
            
            oscillator.frequency.setValueAtTime(880, audioCtx.currentTime);
            gainNode.gain.setValueAtTime(0.5, audioCtx.currentTime);
            
            oscillator.start();
            oscillator.stop(audioCtx.currentTime + 0.5);
        }

        function showNotification(message) {
            const notification = document.createElement('div');
            notification.style.cssText = `
                position: fixed;
                bottom: 20px;
                left: 50%;
                transform: translateX(-50%);
                background: var(--dark);
                color: white;
                padding: 10px 20px;
                border-radius: 50px;
                font-size: 14px;
                z-index: 1000;
                animation: slideUp 0.3s ease;
                box-shadow: 0 10px 25px rgba(0,0,0,0.2);
            `;
            notification.textContent = message;
            document.body.appendChild(notification);
            
            setTimeout(() => {
                notification.remove();
            }, 2000);
        }

        // Initial data fetch
        fetchData();
    </script>
</body>
</html>
)rawliteral";

// ============ FUNCTION DECLARATIONS ============
void handleRoot();
void handleData();
void handleThreshold();
void handleCalibrate();
void handleClear();
void handleTest();
void measureLevel();
void addToHistory();

// ============ SETUP FUNCTION ============
void setup() {
  Serial.begin(115200);
  
  // Initialize Ultrasonic Pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Set up WiFi Access Point
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  
  Serial.println();
  Serial.println("=================================");
  Serial.println("SALON BOTTLE ALERT SYSTEM");
  Serial.println("=================================");
  Serial.print("Access Point: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  Serial.print("IP Address: ");
  Serial.println(IP);
  Serial.println("=================================");
  
  // Configure server routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/threshold", handleThreshold);
  server.on("/calibrate", handleCalibrate);
  server.on("/clear", handleClear);
  server.on("/test", handleTest);
  
  // Start server
  server.begin();
  
  // Record start time
  startTime = millis();
  
  // Initialize readings array
  for (int i = 0; i < 10; i++) {
    readings[i] = 0;
  }
  
  Serial.println("Server started! Connect to: http://192.168.4.1");
}

// ============ LOOP FUNCTION ============
void loop() {
  server.handleClient();
  
  // Take measurement at intervals
  if (millis() - lastMeasurement >= UPDATE_INTERVAL) {
    measureLevel();
    lastMeasurement = millis();
  }
  
  delay(10);
}

// ============ MEASUREMENT FUNCTION ============
void measureLevel() {
  // Trigger ultrasonic pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Read echo pulse
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // Timeout after 30ms
  
  // Calculate distance in cm
  if (duration > 0) {
    distance = duration * 0.034 / 2;
  } else {
    distance = BOTTLE_HEIGHT_CM; // Assume empty if timeout
  }
  
  // Constrain distance to bottle height
  distance = constrain(distance, 0, BOTTLE_HEIGHT_CM);
  
  // Calculate liquid level
  liquidLevel = BOTTLE_HEIGHT_CM - distance;
  liquidLevel = constrain(liquidLevel, 0, BOTTLE_HEIGHT_CM);
  
  // Calculate percentage
  percentage = (liquidLevel / BOTTLE_HEIGHT_CM) * 100;
  percentage = constrain(percentage, 0, 100);
  
  // Determine status
  if (percentage <= 10) {
    bottleStatus = "CRITICAL";
    isLow = true;
    
    // Add to history if new alert
    if (!alertTriggered) {
      addToHistory();
      alertTriggered = true;
    }
  } else if (percentage <= 30) {
    bottleStatus = "LOW";
    isLow = true;
    alertTriggered = false;
  } else {
    bottleStatus = "GOOD";
    isLow = false;
    alertTriggered = false;
  }
  
  // Print to Serial
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.print(" cm | Liquid: ");
  Serial.print(liquidLevel);
  Serial.print(" cm | ");
  Serial.print(percentage);
  Serial.print("% | Status: ");
  Serial.println(bottleStatus);
}

// ============ HISTORY FUNCTION ============
void addToHistory() {
  // Get current time
  unsigned long now = millis() / 1000;
  int hours = (now / 3600) % 24;
  int minutes = (now / 60) % 60;
  int seconds = now % 60;
  
  String timeStr = String(hours) + ":" + 
                   (minutes < 10 ? "0" : "") + String(minutes) + ":" + 
                   (seconds < 10 ? "0" : "") + String(seconds);
  
  // Add to circular buffer
  historyTimes[historyCount % 20] = timeStr;
  historyPercentages[historyCount % 20] = percentage;
  historyStatus[historyCount % 20] = bottleStatus;
  historyCount++;
}

// ============ WEB HANDLERS ============
void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleData() {
  StaticJsonDocument<500> doc;
  
  // Calculate uptime
  unsigned long uptime = millis() / 1000;
  
  doc["distance"] = distance;
  doc["liquidLevel"] = liquidLevel;
  doc["percentage"] = percentage;
  doc["status"] = bottleStatus;
  doc["isLow"] = isLow;
  doc["uptime"] = uptime;
  
  // Add history
  JsonArray history = doc.createNestedArray("history");
  int start = max(0, historyCount - 10);
  int end = historyCount;
  
  for (int i = start; i < end; i++) {
    JsonObject item = history.createNestedObject();
    item["time"] = historyTimes[i % 20];
    item["percentage"] = historyPercentages[i % 20];
    item["status"] = historyStatus[i % 20];
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.send(200, "application/json", jsonString);
}

void handleThreshold() {
  if (server.hasArg("value")) {
    int newThreshold = server.arg("value").toInt();
    if (newThreshold >= 0 && newThreshold <= 100) {
      alertLevel = newThreshold;
      server.send(200, "text/plain", "Threshold updated to " + String(newThreshold) + "%");
      
      Serial.print("Alert threshold updated to: ");
      Serial.print(newThreshold);
      Serial.println("%");
    } else {
      server.send(400, "text/plain", "Invalid threshold value");
    }
  } else {
    server.send(400, "text/plain", "Missing threshold value");
  }
}

void handleCalibrate() {
  // Calibrate full level
  float fullLevel = distance;
  server.send(200, "text/plain", String(fullLevel));
  
  Serial.print("Calibrated full level: ");
  Serial.print(fullLevel);
  Serial.println(" cm");
}

void handleClear() {
  historyCount = 0;
  server.send(200, "text/plain", "History cleared");
  
  Serial.println("Alert history cleared");
}

void handleTest() {
  // Trigger test alert
  addToHistory();
  server.send(200, "text/plain", "Test alert triggered");
  
  Serial.println("Test alert triggered");
}

