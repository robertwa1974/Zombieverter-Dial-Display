#include "WebInterface.h"
#include <SPIFFS.h>
#include <driver/twai.h>

WebInterface::WebInterface(CANDataManager* can) 
    : canManager(can), server(80), apMode(false), corsEnabled(true), canLogIndex(0), canLoggingEnabled(true) {
}

bool WebInterface::init() {
    // Initialize SPIFFS for web files
    if (!SPIFFS.begin(true)) {
        Serial.println("[WEB] Failed to mount SPIFFS");
        return false;
    }
    
    // Default: Start as Access Point
    startAccessPoint();
    
    // Setup mDNS
    if (MDNS.begin("zombieverter")) {
        Serial.println("[WEB] mDNS started: zombieverter.local");
        MDNS.addService("http", "tcp", 80);
    }
    
    // Setup HTTP routes
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/json", HTTP_GET, [this]() { handleJSON(); });
    server.on("/cmd", HTTP_GET, [this]() { handleCmd(); });  // jamiejones85 compatibility
    server.on("/get", HTTP_GET, [this]() { handleGet(); });
    server.on("/set", HTTP_GET, [this]() { handleSet(); });
    server.on("/save", HTTP_GET, [this]() { handleSave(); });
    server.on("/load", HTTP_GET, [this]() { handleLoad(); });
    server.on("/spot", HTTP_GET, [this]() { handleSpot(); });
    server.on("/can/send", HTTP_GET, [this]() { handleCanSend(); });
    server.on("/can/log", HTTP_GET, [this]() { handleCanLog(); });
    server.on("/params/upload", HTTP_POST, [this]() { handleParamsUpload(); });
    
    // Enable CORS for all routes if needed
    server.enableCORS(true);
    
    server.onNotFound([this]() { handleNotFound(); });
    
    server.begin();
    Serial.println("[WEB] HTTP server started");
    Serial.printf("[WEB] Access at: http://%s\n", getIPAddress().c_str());
    
    return true;
}

void WebInterface::update() {
    server.handleClient();
    
    // Capture CAN messages for logging (non-blocking)
    if (canLoggingEnabled) {
        twai_message_t msg;
        while (twai_receive(&msg, 0) == ESP_OK) {  // Non-blocking receive
            logCanMessage(msg.identifier, msg.data, msg.data_length_code, true);
        }
    }
}

void WebInterface::startAccessPoint(const char* ssid, const char* password) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    apMode = true;
    
    Serial.println("[WEB] Access Point started");
    Serial.printf("[WEB] SSID: %s\n", ssid);
    Serial.printf("[WEB] IP: %s\n", WiFi.softAPIP().toString().c_str());
}

void WebInterface::connectToWiFi(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    Serial.printf("[WEB] Connecting to %s", ssid);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        apMode = false;
        Serial.println(" Connected!");
        Serial.printf("[WEB] IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println(" Failed!");
        Serial.println("[WEB] Falling back to AP mode");
        startAccessPoint();
    }
}

String WebInterface::getIPAddress() {
    return apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

bool WebInterface::isConnected() {
    return WiFi.status() == WL_CONNECTED || apMode;
}

void WebInterface::setHostname(const char* hostname) {
    WiFi.setHostname(hostname);
    MDNS.begin(hostname);
}

void WebInterface::enableCORS(bool enable) {
    corsEnabled = enable;
}

// ============================================================================
// HTTP Handlers
// ============================================================================

void WebInterface::handleRoot() {
    // Serve enhanced dashboard with CAN Monitor - NO EMOJIS
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>M5Dial - ZombieVerter</title><style>:root{--bg-primary:#1a1a1a;--bg-secondary:#2a2a2a;--bg-tertiary:#333;--accent-primary:#00d4ff;--accent-secondary:#00ff88;--text-primary:#fff;--text-secondary:#aaa}*{margin:0;padding:0;box-sizing:border-box}body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:var(--bg-primary);color:var(--text-primary)}.header{background:var(--bg-secondary);padding:15px 20px;border-bottom:2px solid var(--accent-primary);display:flex;justify-content:space-between;align-items:center}.header h1{font-size:1.5em;color:var(--accent-primary)}.status-dot{width:12px;height:12px;border-radius:50%;background:#ff4444;animation:pulse 2s infinite}.status-dot.online{background:var(--accent-secondary)}@keyframes pulse{0%,100%{opacity:1}50%{opacity:.5}}.tabs{background:var(--bg-secondary);display:flex;border-bottom:1px solid #444;overflow-x:auto}.tab{padding:15px 25px;cursor:pointer;border-bottom:3px solid transparent;transition:all .3s;white-space:nowrap}.tab:hover{background:var(--bg-tertiary)}.tab.active{border-bottom-color:var(--accent-primary);color:var(--accent-primary)}.tab-content{display:none;padding:20px}.tab-content.active{display:block}.gauge-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px;margin-bottom:30px}.gauge-card{background:var(--bg-secondary);border-radius:12px;padding:20px;position:relative}.gauge-card::before{content:'';position:absolute;top:0;left:0;right:0;height:3px;background:linear-gradient(90deg,var(--accent-primary),var(--accent-secondary))}.gauge-header{display:flex;justify-content:space-between;margin-bottom:15px}.gauge-title{font-size:.9em;color:var(--text-secondary);text-transform:uppercase}.circular-gauge{position:relative;width:200px;height:200px;margin:20px auto}.gauge-svg{transform:rotate(-90deg)}.gauge-value-display{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);text-align:center}.gauge-value{font-size:2.5em;font-weight:700;color:var(--accent-primary)}.linear-gauge{height:40px;background:var(--bg-tertiary);border-radius:20px;overflow:hidden;position:relative}.linear-fill{height:100%;background:linear-gradient(90deg,var(--accent-primary),var(--accent-secondary));transition:width .5s ease;display:flex;align-items:center;justify-content:flex-end;padding-right:15px}.linear-value{color:#fff;font-weight:700;text-shadow:0 0 10px rgba(0,0,0,.5)}.btn{background:var(--accent-primary);color:var(--bg-primary);border:none;padding:10px 20px;border-radius:5px;cursor:pointer;font-weight:700;transition:all .3s;margin:5px}.btn:hover{background:var(--accent-secondary);transform:translateY(-2px)}.btn-small{padding:5px 10px;font-size:0.9em}.can-log{background:var(--bg-tertiary);padding:15px;border-radius:8px;height:400px;overflow-y:auto;font-family:monospace;font-size:0.85em;margin-bottom:15px}.can-msg{padding:5px;margin:2px 0;border-left:3px solid var(--accent-primary);background:var(--bg-secondary)}.can-msg.rx{border-left-color:var(--accent-secondary)}.can-msg.tx{border-left-color:var(--accent-primary)}.input-group{display:grid;grid-template-columns:100px 1fr;gap:10px;margin-bottom:10px;align-items:center}.input-group label{color:var(--text-secondary)}.input-group input{background:var(--bg-tertiary);border:1px solid var(--border-color);border-radius:5px;padding:8px;color:var(--text-primary);font-family:monospace}.file-input{display:none}.param-row{display:grid;grid-template-columns:200px 100px 80px 120px;gap:10px;padding:10px;border-bottom:1px solid #333;align-items:center}.param-row:hover{background:var(--bg-tertiary)}.param-name{font-weight:bold;color:var(--accent-primary)}.param-value{font-family:monospace}.param-unit{color:var(--text-secondary);font-size:0.9em}.param-actions{display:flex;gap:5px}.param-edit-input{width:100px;padding:5px;background:var(--bg-tertiary);border:1px solid var(--accent-primary);border-radius:3px;color:var(--text-primary);font-family:monospace}@media (max-width:768px){.gauge-grid{grid-template-columns:1fr}.param-row{grid-template-columns:1fr;gap:5px}}</style></head><body><div class='header'><h1>M5Dial ZombieVerter</h1><div style='display:flex;align-items:center;gap:10px'><div class='status-dot' id='statusDot'></div><span id='statusText'>Connecting...</span></div></div><div class='tabs'><div class='tab active' onclick=\"switchTab('monitor')\">Monitor</div><div class='tab' onclick=\"switchTab('params')\">Parameters</div><div class='tab' onclick=\"switchTab('can')\">CAN Traffic</div></div><div id='monitor' class='tab-content active'><div style='padding:10px'><button class='btn' onclick='refreshData()'>Refresh</button><button class='btn' onclick='toggleAuto()' id='autoBtn'>Pause</button></div><div class='gauge-grid'><div class='gauge-card'><div class='gauge-header'><span class='gauge-title'>Motor Speed</span><span>RPM</span></div><div class='circular-gauge'><svg class='gauge-svg' width='200' height='200'><circle cx='100' cy='100' r='90' fill='none' stroke='#333' stroke-width='12'/><circle id='rpmG' cx='100' cy='100' r='90' fill='none' stroke='url(#g1)' stroke-width='12' stroke-dasharray='565' stroke-dashoffset='565' stroke-linecap='round'/><defs><linearGradient id='g1'><stop offset='0%' stop-color='#00d4ff'/><stop offset='100%' stop-color='#00ff88'/></linearGradient></defs></svg><div class='gauge-value-display'><div class='gauge-value' id='rpmVal'>0</div><div style='font-size:.9em;color:#aaa'>x100</div></div></div></div><div class='gauge-card'><div class='gauge-header'><span class='gauge-title'>Power</span><span>kW</span></div><div class='circular-gauge'><svg class='gauge-svg' width='200' height='200'><circle cx='100' cy='100' r='90' fill='none' stroke='#333' stroke-width='12'/><circle id='pwrG' cx='100' cy='100' r='90' fill='none' stroke='url(#g2)' stroke-width='12' stroke-dasharray='565' stroke-dashoffset='565' stroke-linecap='round'/><defs><linearGradient id='g2'><stop offset='0%' stop-color='#00ff88'/><stop offset='100%' stop-color='#ffaa00'/></linearGradient></defs></svg><div class='gauge-value-display'><div class='gauge-value' id='pwrVal'>0.0</div></div></div></div><div class='gauge-card'><div class='gauge-header'><span class='gauge-title'>Voltage</span><span>V</span></div><div class='linear-gauge'><div class='linear-fill' id='vFill' style='width:0%'><span class='linear-value' id='vVal'>0V</span></div></div></div><div class='gauge-card'><div class='gauge-header'><span class='gauge-title'>Current</span><span>A</span></div><div class='linear-gauge'><div class='linear-fill' id='cFill' style='width:0%'><span class='linear-value' id='cVal'>0A</span></div></div></div><div class='gauge-card'><div class='gauge-header'><span class='gauge-title'>Motor Temp</span><span>C</span></div><div class='linear-gauge'><div class='linear-fill' id='tFill' style='width:0%'><span class='linear-value' id='tVal'>0C</span></div></div></div><div class='gauge-card'><div class='gauge-header'><span class='gauge-title'>Battery SOC</span><span>%</span></div><div class='linear-gauge'><div class='linear-fill' id='sFill' style='width:0%'><span class='linear-value' id='sVal'>0%</span></div></div></div></div></div><div id='params' class='tab-content'><div style='padding:10px'><button class='btn' onclick='loadParams()'>Refresh (slow!)</button><button class='btn' onclick='downloadParams()'>Download JSON</button><button class='btn' onclick='document.getElementById(\"fileInput\").click()'>Upload JSON</button><button class='btn' onclick='save()'>Save to Flash</button><input type='file' id='fileInput' class='file-input' accept='.json' onchange='uploadParams(event)'></div><div style='padding:10px;color:var(--text-secondary);font-size:0.9em'>Tip: Refresh takes ~30s to query all 256 parameters. Editable params show input field.</div><div id='paramList' style='padding:20px;color:#aaa'>Click Refresh to load parameters...</div></div><div id='can' class='tab-content'><div style='padding:10px'><button class='btn btn-small' onclick='clearLog()'>Clear Log</button><button class='btn btn-small' onclick='toggleCanLog()' id='canLogBtn'>Pause Log</button></div><div class='can-log' id='canLog'>Waiting for CAN messages...</div><div style='background:var(--bg-secondary);padding:20px;border-radius:12px'><h3 style='color:var(--accent-primary);margin-bottom:15px'>Send CAN Message</h3><div class='input-group'><label>CAN ID (hex):</label><input type='text' id='canId' placeholder='0x603' value='0x603'></div><div class='input-group'><label>Data (hex):</label><input type='text' id='canData' placeholder='40 01 00 00 00 00 00 00' value='40 01 00 00 00 00 00 00'></div><button class='btn' onclick='sendCan()'>Send CAN Message</button><div style='margin-top:10px;padding:10px;background:var(--bg-tertiary);border-radius:5px;font-size:0.85em'><strong>Examples:</strong><br>Read param 1: 40 01 00 00 00 00 00 00 (to 0x603)<br>Write param 21: 23 15 00 00 F4 01 00 00 (500A)<br>SDO save: 23 00 00 00 06 00 00 00</div></div></div><script>const API='http://" + getIPAddress() + "';let auto=true,iv,canLog=true,canMsgs=[],allParams={};function switchTab(t){document.querySelectorAll('.tab').forEach(e=>e.classList.remove('active'));document.querySelectorAll('.tab-content').forEach(e=>e.classList.remove('active'));event.target.classList.add('active');document.getElementById(t).classList.add('active');if(t==='params'&&Object.keys(allParams).length===0)loadParams()}function updateCirc(id,v,max){const c=document.getElementById(id);const p=Math.min(v/max,1);const o=565-(p*565);c.style.strokeDashoffset=o}function updateLin(fid,vid,v,min,max,u){const p=((v-min)/(max-min))*100;const cp=Math.max(0,Math.min(100,p));document.getElementById(fid).style.width=cp+'%';document.getElementById(vid).textContent=v+u}async function updateData(){try{const r=await fetch(API+'/spot');if(!r.ok)throw new Error('fail');const d=await r.json();const rpm=(d.speed||0)/100;updateCirc('rpmG',rpm,80);document.getElementById('rpmVal').textContent=Math.round(rpm);const pwr=d.power||0;updateCirc('pwrG',Math.abs(pwr),100);document.getElementById('pwrVal').textContent=pwr.toFixed(1);updateLin('vFill','vVal',d.udc||0,0,400,'V');updateLin('cFill','cVal',Math.abs(d.idc||0),0,500,'A');updateLin('tFill','tVal',d.tmpm||0,0,150,'C');updateLin('sFill','sVal',d.soc||0,0,100,'%');document.getElementById('statusDot').classList.add('online');document.getElementById('statusText').textContent='Connected'}catch(e){console.error(e);document.getElementById('statusDot').classList.remove('online');document.getElementById('statusText').textContent='Offline'}}async function loadParams(){document.getElementById('paramList').innerHTML='<div style=\"padding:20px;text-align:center;color:var(--accent-primary)\">Querying 256 parameters... this takes ~30-60 seconds...<br><br>Check Serial Monitor for progress!</div>';try{const r=await fetch(API+'/json');console.log('Response status:',r.status);const text=await r.text();console.log('Response text:',text.substring(0,200));if(!text||text.length<2){throw new Error('Empty response from server')}allParams=JSON.parse(text);console.log('Parsed params:',Object.keys(allParams).length,'parameters');displayParams()}catch(e){console.error('Load error:',e);document.getElementById('paramList').innerHTML='<div style=\"padding:20px;color:#ff4444\">Error: '+e.message+'<br><br>Check:<br>1. ZombieVerter is powered on<br>2. CAN bus is connected<br>3. Serial Monitor shows progress<br>4. Try again (may take 30-60s)</div>'}}function displayParams(){let h='<div style=\"display:grid;gap:5px\">';const keys=Object.keys(allParams).sort();if(keys.length===0){h='<div style=\"padding:20px;text-align:center\">No parameters found. Check CAN connection.</div>'}else{keys.forEach(k=>{const v=allParams[k];const isEditable=v.isparam===true;h+='<div class=\"param-row\">';h+='<div class=\"param-name\">'+k+'</div>';h+='<div class=\"param-value\">'+v.value+'</div>';h+='<div class=\"param-unit\">'+( v.unit||'')+'</div>';h+='<div class=\"param-actions\">';if(isEditable){h+='<input type=\"number\" class=\"param-edit-input\" id=\"edit_'+v.i+'\" value=\"'+v.value+'\" '+(v.minimum!==undefined?'min=\"'+v.minimum+'\" max=\"'+v.maximum+'\"':'')+'>';h+='<button class=\"btn btn-small\" onclick=\"setParam('+v.i+',document.getElementById(\\'edit_'+v.i+'\\').value)\">Set</button>'}h+='</div>';h+='</div>'})}h+='</div>';document.getElementById('paramList').innerHTML=h}async function setParam(id,val){try{const r=await fetch(API+'/set?param='+id+'&value='+val);if(r.ok){alert('Parameter '+id+' set to '+val);loadParams()}else{alert('Failed to set parameter')}}catch(e){alert('Error: '+e.message)}}async function downloadParams(){try{const params=Object.keys(allParams).length>0?allParams:await(await fetch(API+'/json')).json();const blob=new Blob([JSON.stringify(params,null,2)],{type:'application/json'});const url=URL.createObjectURL(blob);const a=document.createElement('a');a.href=url;a.download='zombieverter_params_'+new Date().toISOString().split('T')[0]+'.json';document.body.appendChild(a);a.click();document.body.removeChild(a);URL.revokeObjectURL(url);alert('Parameters downloaded!')}catch(e){alert('Error: '+e.message)}}async function uploadParams(e){const file=e.target.files[0];if(!file)return;try{const text=await file.text();const params=JSON.parse(text);const r=await fetch(API+'/params/upload',{method:'POST',headers:{'Content-Type':'application/json'},body:text});if(r.ok){alert('Parameters uploaded! Reboot to apply.');loadParams()}else{alert('Failed to upload')}}catch(e){alert('Error: '+e.message)}e.target.value=''}async function save(){try{const r=await fetch(API+'/save');alert(r.ok?'Saved to flash!':'Failed')}catch(e){alert('Error: '+e.message)}}function refreshData(){updateData()}function toggleAuto(){auto=!auto;document.getElementById('autoBtn').textContent=auto?'Pause':'Resume';if(auto){updateData();iv=setInterval(updateData,1000)}else{clearInterval(iv)}}function addCanMsg(msg,type){if(!canLog)return;canMsgs.unshift({msg:msg,type:type,time:new Date().toLocaleTimeString()});if(canMsgs.length>100)canMsgs.pop();updateCanLog()}function updateCanLog(){const log=document.getElementById('canLog');log.innerHTML=canMsgs.map(m=>'<div class=\"can-msg '+m.type+'\">'+m.time+' ['+m.type.toUpperCase()+'] '+m.msg+'</div>').join('')||'No messages yet...'}function clearLog(){canMsgs=[];updateCanLog()}function toggleCanLog(){canLog=!canLog;document.getElementById('canLogBtn').textContent=canLog?'Pause Log':'Resume Log'}async function sendCan(){const id=document.getElementById('canId').value.trim();const data=document.getElementById('canData').value.trim();try{const canId=id.startsWith('0x')?parseInt(id,16):parseInt(id,10);const bytes=data.split(' ').map(b=>parseInt(b,16));if(isNaN(canId)||bytes.some(isNaN)){alert('Invalid format');return}const r=await fetch(API+'/can/send?id='+canId+'&data='+bytes.join(','));if(r.ok){addCanMsg('ID:0x'+canId.toString(16).toUpperCase().padStart(3,'0')+' Data:['+bytes.map(b=>'0x'+b.toString(16).toUpperCase().padStart(2,'0')).join(' ')+']','tx');updateCanLog();alert('Sent!')}else{alert('Failed')}}catch(e){alert('Error: '+e.message)}}updateData();iv=setInterval(updateData,1000);setInterval(()=>{if(document.getElementById('can').classList.contains('active')){fetch(API+'/can/log').then(r=>r.json()).then(msgs=>{msgs.forEach(m=>addCanMsg('ID:0x'+m.id.toString(16).toUpperCase().padStart(3,'0')+' Data:['+m.data.join(' ')+']','rx'));updateCanLog()}).catch(e=>console.error(e))}},500)</script></body></html>";
    
    server.send(200, "text/html", html);
}

void WebInterface::handleJSON() {
    if (corsEnabled) addCORSHeaders();
    
    Serial.println("[WEB] /json endpoint called - starting full parameter query");
    
    // Send headers first to keep connection alive
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/json", "");
    
    bool includeHidden = server.hasArg("hidden");
    String response = buildJSONResponse(includeHidden);
    
    Serial.printf("[WEB] Sending JSON response (%d bytes)\n", response.length());
    
    // Send the actual content
    server.sendContent(response);
    server.sendContent("");  // End of content
    
    Serial.println("[WEB] /json response complete");
}

void WebInterface::handleCmd() {
    if (corsEnabled) addCORSHeaders();
    
    // jamiejones85 compatibility: /cmd?cmd=json or /cmd?cmd=save
    if (!server.hasArg("cmd")) {
        server.send(400, "text/plain", "Missing cmd argument");
        return;
    }
    
    String cmd = server.arg("cmd");
    Serial.printf("[WEB] /cmd request: %s\n", cmd.c_str());
    
    if (cmd == "json") {
        // Redirect to /json endpoint
        handleJSON();
    } else if (cmd == "save") {
        // Save to flash
        handleSave();
    } else {
        server.send(400, "text/plain", "Unknown command: " + cmd);
    }
}

void WebInterface::handleGet() {
    if (corsEnabled) addCORSHeaders();
    
    if (!server.hasArg("param")) {
        server.send(400, "text/plain", "Missing param argument");
        return;
    }
    
    int paramId = server.arg("param").toInt();
    int32_t value = getParameterValue(paramId);
    
    server.send(200, "text/plain", String(value));
}

void WebInterface::handleSet() {
    if (corsEnabled) addCORSHeaders();
    
    if (!server.hasArg("param") || !server.hasArg("value")) {
        server.send(400, "text/plain", "Missing param or value argument");
        return;
    }
    
    int paramId = server.arg("param").toInt();
    int32_t value = server.arg("value").toInt();
    
    if (setParameterValue(paramId, value)) {
        server.send(200, "text/plain", "OK");
    } else {
        server.send(500, "text/plain", "Failed to set parameter");
    }
}

void WebInterface::handleSave() {
    if (corsEnabled) addCORSHeaders();
    
    // Send SDO save command to ZombieVerter
    // opmode = 6 (save to flash)
    twai_message_t msg;
    msg.identifier = 0x601;
    msg.extd = 0;
    msg.rtr = 0;
    msg.data_length_code = 8;
    
    msg.data[0] = 0x23;  // Write 4 bytes
    msg.data[1] = 0x00;  // Parameter 0 (opmode)
    msg.data[2] = 0x00;
    msg.data[3] = 0x00;
    msg.data[4] = 0x06;  // Value 6 = save
    msg.data[5] = 0x00;
    msg.data[6] = 0x00;
    msg.data[7] = 0x00;
    
    if (twai_transmit(&msg, pdMS_TO_TICKS(100)) == ESP_OK) {
        server.send(200, "text/plain", "Parameters saved to flash");
    } else {
        server.send(500, "text/plain", "Failed to send save command");
    }
}

void WebInterface::handleLoad() {
    if (corsEnabled) addCORSHeaders();
    
    // ZombieVerter loads from flash on boot
    // This would typically trigger a reboot
    server.send(200, "text/plain", "Reload on next boot");
}

void WebInterface::handleSpot() {
    if (corsEnabled) addCORSHeaders();
    
    // Return current "spot" values (real-time data)
    // This is similar to JSON but only non-parameter values
    
    JsonDocument doc;  // Use JsonDocument instead of DynamicJsonDocument
    
    // Add key real-time parameters
    CANParameter* rpm = canManager->getParameter(1);
    if (rpm) doc["speed"] = rpm->getValueAsInt();
    
    CANParameter* voltage = canManager->getParameter(3);
    if (voltage) doc["udc"] = voltage->getValueAsInt();
    
    CANParameter* current = canManager->getParameter(4);
    if (current) doc["idc"] = current->getValueAsInt();
    
    CANParameter* power = canManager->getParameter(2);
    if (power) doc["power"] = power->getValueAsInt() / 10.0;
    
    CANParameter* motorTemp = canManager->getParameter(5);
    if (motorTemp) doc["tmpm"] = motorTemp->getValueAsInt();
    
    CANParameter* invTemp = canManager->getParameter(6);
    if (invTemp) doc["tmphs"] = invTemp->getValueAsInt();
    
    CANParameter* soc = canManager->getParameter(7);
    if (soc) doc["soc"] = soc->getValueAsInt();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void WebInterface::handleNotFound() {
    String message = "File Not Found\n\n";
    message += "URI: " + server.uri() + "\n";
    message += "Method: " + String((server.method() == HTTP_GET) ? "GET" : "POST") + "\n";
    message += "Arguments: " + String(server.args()) + "\n";
    
    server.send(404, "text/plain", message);
}

void WebInterface::addCORSHeaders() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ============================================================================
// Helper Functions
// ============================================================================

// Determines if a parameter uses raw values (no fixed-point encoding)
bool WebInterface::usesRawValue(const char* paramName) {
    // Configuration enums - use raw values
    if (strcmp(paramName, "Inverter") == 0) return true;
    if (strcmp(paramName, "Vehicle") == 0) return true;
    if (strcmp(paramName, "GearLvr") == 0) return true;
    if (strcmp(paramName, "Transmission") == 0) return true;
    if (strcmp(paramName, "interface") == 0) return true;
    if (strcmp(paramName, "chargemodes") == 0) return true;
    if (strcmp(paramName, "BMS_Mode") == 0) return true;
    if (strcmp(paramName, "ShuntType") == 0) return true;
    if (strcmp(paramName, "Gear") == 0) return true;
    if (strcmp(paramName, "potmode") == 0) return true;
    if (strcmp(paramName, "dirmode") == 0) return true;
    if (strcmp(paramName, "reversemotor") == 0) return true;
    if (strcmp(paramName, "revRegen") == 0) return true;
    if (strcmp(paramName, "cruiselight") == 0) return true;
    if (strcmp(paramName, "errlights") == 0) return true;
    if (strcmp(paramName, "Chgctrl") == 0) return true;
    if (strcmp(paramName, "ConfigFoccci") == 0) return true;
    if (strcmp(paramName, "DCdc_Type") == 0) return true;
    if (strcmp(paramName, "Heater") == 0) return true;
    if (strcmp(paramName, "Control") == 0) return true;
    if (strcmp(paramName, "IsaInit") == 0) return true;
    
    // CAN bus assignments - use raw values
    if (strcmp(paramName, "InverterCan") == 0) return true;
    if (strcmp(paramName, "VehicleCan") == 0) return true;
    if (strcmp(paramName, "ShuntCan") == 0) return true;
    if (strcmp(paramName, "LimCan") == 0) return true;
    if (strcmp(paramName, "ChargerCan") == 0) return true;
    if (strcmp(paramName, "BMSCan") == 0) return true;
    if (strcmp(paramName, "OBD2Can") == 0) return true;
    if (strcmp(paramName, "CanMapCan") == 0) return true;
    if (strcmp(paramName, "DCDCCan") == 0) return true;
    if (strcmp(paramName, "HeaterCan") == 0) return true;
    if (strcmp(paramName, "CAN3Speed") == 0) return true;
    
    // GPIO function assignments - use raw values
    if (strcmp(paramName, "MotActive") == 0) return true;
    if (strcmp(paramName, "PumpPWM") == 0) return true;
    if (strcmp(paramName, "Out1Func") == 0) return true;
    if (strcmp(paramName, "Out2Func") == 0) return true;
    if (strcmp(paramName, "Out3Func") == 0) return true;
    if (strcmp(paramName, "SL1Func") == 0) return true;
    if (strcmp(paramName, "SL2Func") == 0) return true;
    if (strcmp(paramName, "PWM1Func") == 0) return true;
    if (strcmp(paramName, "PWM2Func") == 0) return true;
    if (strcmp(paramName, "PWM3Func") == 0) return true;
    if (strcmp(paramName, "GP12VInFunc") == 0) return true;
    if (strcmp(paramName, "HVReqFunc") == 0) return true;
    if (strcmp(paramName, "PB1InFunc") == 0) return true;
    if (strcmp(paramName, "PB2InFunc") == 0) return true;
    if (strcmp(paramName, "PB3InFunc") == 0) return true;
    if (strcmp(paramName, "GPA1Func") == 0) return true;
    if (strcmp(paramName, "GPA2Func") == 0) return true;
    
    // Digital ADC values - use raw values
    if (strcmp(paramName, "potmin") == 0) return true;
    if (strcmp(paramName, "potmax") == 0) return true;
    if (strcmp(paramName, "pot2min") == 0) return true;
    if (strcmp(paramName, "pot2max") == 0) return true;
    if (strcmp(paramName, "ppthresh") == 0) return true;
    if (strcmp(paramName, "BrkVacThresh") == 0) return true;
    if (strcmp(paramName, "BrkVacHyst") == 0) return true;
    if (strcmp(paramName, "DigiPot1Step") == 0) return true;
    if (strcmp(paramName, "DigiPot2Step") == 0) return true;
    
    // Time values - use raw values
    if (strcmp(paramName, "Set_Day") == 0) return true;
    if (strcmp(paramName, "Set_Hour") == 0) return true;
    if (strcmp(paramName, "Set_Min") == 0) return true;
    if (strcmp(paramName, "Set_Sec") == 0) return true;
    if (strcmp(paramName, "Chg_Hrs") == 0) return true;
    if (strcmp(paramName, "Chg_Min") == 0) return true;
    if (strcmp(paramName, "Chg_Dur") == 0) return true;
    if (strcmp(paramName, "Pre_Hrs") == 0) return true;
    if (strcmp(paramName, "Pre_Min") == 0) return true;
    if (strcmp(paramName, "Pre_Dur") == 0) return true;
    if (strcmp(paramName, "BMS_Timeout") == 0) return true;
    
    // PWM timer registers - use raw values
    if (strcmp(paramName, "Tim3_Presc") == 0) return true;
    if (strcmp(paramName, "Tim3_Period") == 0) return true;
    if (strcmp(paramName, "Tim3_1_OC") == 0) return true;
    if (strcmp(paramName, "Tim3_2_OC") == 0) return true;
    if (strcmp(paramName, "Tim3_3_OC") == 0) return true;
    if (strcmp(paramName, "CP_PWM") == 0) return true;
    
    // Other special cases
    if (strcmp(paramName, "regenlevel") == 0) return true;
    if (strcmp(paramName, "TachoPPR") == 0) return true;
    
    // All others use fixed-point encoding (×32)
    return false;
}

String WebInterface::buildJSONResponse(bool includeHidden) {
    JsonDocument doc;
    
    Serial.println("[WEB] Building JSON response - using hardcoded parameters (no SPIFFS)");
    
    // Hardcoded parameters to avoid SPIFFS partition issues
    struct ParamDef {
        const char* name;
        int id;
        const char* unit;
        const char* category;
        bool isEditable;
    };
    
    ParamDef params[] = {
        {"Inverter", 0, "0=None, 1=Leaf_Gen1, 2=GS450h, 3=UserCAN, 4=OpenI, 5=Prius_Gen3, 6=RearAC, 7=E65_MT, 8=CanOI, 9=E65_BMW", "General Setup", true},
        {"Vehicle", 1, "0=BMW_E46, 1=BMW_E6x+, 2=Classic, 3=None, 4=User, 5=CPC", "General Setup", true},
        {"GearLvr", 2, "0=None, 1=BMW_F30, 2=JLR_G1, 3=JLR_G2, 4=VAG, 5=Outlander", "General Setup", true},
        {"Transmission", 3, "0=Manual, 1=Auto", "General Setup", true},
        {"interface", 4, "0=Unused, 1=i3LIM, 2=Chademo, 3=CPC", "General Setup", true},
        {"chargemodes", 5, "0=Off, 1=EXT_DIGI, 2=Volt_A, 3=Volt_A+EXT, 4=i3LIM, 5=i3LIM+EXT, 6=Focci, 7=Leaf_PDM, 8=Outlander, 9=CPC, 10=Leaf_Inverter, 11=Tesla_M3_OBC", "General Setup", true},
        {"BMS_Mode", 6, "0=Off, 1=SimpBMS, 2=TiDaisyChain, 3=BMWi3LIM, 4=OutlanderFront, 5=VWeBMS, 6=OutlanderFront+Rear, 7=Volt, 8=LeafGen2, 9=CPC, 10=RenaultZoe, 11=TeslaM3, 12=VictronCAN", "General Setup", true},
        {"ShuntType", 7, "0=None, 1=ISA, 2=SBOX, 3=VAG", "General Setup", true},
        {"InverterCan", 8, "0=CAN1, 1=CAN2", "General Setup", true},
        {"VehicleCan", 9, "0=CAN1, 1=CAN2", "General Setup", true},
        {"ShuntCan", 10, "0=CAN1, 1=CAN2", "General Setup", true},
        {"LimCan", 11, "0=CAN1, 1=CAN2", "General Setup", true},
        {"ChargerCan", 12, "0=CAN1, 1=CAN2", "General Setup", true},
        {"BMSCan", 13, "0=CAN1, 1=CAN2", "General Setup", true},
        {"OBD2Can", 14, "0=CAN1, 1=CAN2", "General Setup", true},
        {"CanMapCan", 15, "0=CAN1, 1=CAN2", "General Setup", true},
        {"DCDCCan", 16, "0=CAN1, 1=CAN2", "General Setup", true},
        {"HeaterCan", 17, "0=CAN1, 1=CAN2", "General Setup", true},
        {"MotActive", 18, "0=Mg1and2, 1=Mg1, 2=Mg2, 3=Both", "General Setup", true},
        {"potmin", 19, "dig", "Throttle", true},
        {"potmax", 20, "dig", "Throttle", true},
        {"pot2min", 21, "dig", "Throttle", true},
        {"pot2max", 22, "dig", "Throttle", true},
        {"regenrpm", 23, "rpm", "Throttle", true},
        {"regenendrpm", 24, "rpm", "Throttle", true},
        {"regenmax", 25, "%", "Throttle", true},
        {"regenBrake", 26, "%", "Throttle", true},
        {"regenramp", 27, "%/10ms", "Throttle", true},
        {"potmode", 28, "0=SingleChannel, 1=DualChannel", "Throttle", true},
        {"dirmode", 29, "0=Button, 1=Switch, 2=ButtonReversed, 3=SwitchReversed, 4=DefaultForward", "Throttle", true},
        {"reversemotor", 30, "0=Off, 1=On, 2=na", "Throttle", true},
        {"throtramp", 31, "%/10ms", "Throttle", true},
        {"throtramprpm", 32, "rpm", "Throttle", true},
        {"revlim", 33, "rpm", "Throttle", true},
        {"revRegen", 34, "0=Off, 1=On, 2=na", "Throttle", true},
        {"udcmin", 35, "V", "Throttle", true},
        {"udclim", 36, "V", "Throttle", true},
        {"idcmax", 37, "A", "Throttle", true},
        {"idcmin", 38, "A", "Throttle", true},
        {"tmphsmax", 39, "°C", "Throttle", true},
        {"tmpmmax", 40, "°C", "Throttle", true},
        {"throtmax", 41, "%", "Throttle", true},
        {"throtmin", 42, "%", "Throttle", true},
        {"throtmaxRev", 43, "%", "Throttle", true},
        {"throtdead", 44, "%", "Throttle", true},
        {"RegenBrakeLight", 45, "%", "Throttle", true},
        {"throtrpmfilt", 46, "rpm/10ms", "Throttle", true},
        {"Gear", 47, "0=LOW, 1=HIGH, 2=AUTO, 3=HILLHOLD", "Gearbox Control", true},
        {"OilPump", 48, "%", "Gearbox Control", true},
        {"cruisestep", 49, "rpm", "Cruise Control", true},
        {"cruiseramp", 50, "rpm/100ms", "Cruise Control", true},
        {"regenlevel", 51, "", "Cruise Control", true},
        {"udcsw", 52, "V", "Contactor Control", true},
        {"cruiselight", 53, "0=Off, 1=On, 2=na", "Contactor Control", true},
        {"errlights", 54, "0=Off, 4=EPC, 8=engine", "Contactor Control", true},
        {"CAN3Speed", 55, "0=k33.3, 1=k500, 2=k100", "Communication", true},
        {"BattCap", 56, "kWh", "Charger Control", true},
        {"Voltspnt", 57, "V", "Charger Control", true},
        {"Pwrspnt", 58, "W", "Charger Control", true},
        {"IdcTerm", 59, "A", "Charger Control", true},
        {"CCS_ICmd", 60, "A", "Charger Control", true},
        {"CCS_ILim", 61, "A", "Charger Control", true},
        {"CCS_SOCLim", 62, "%", "Charger Control", true},
        {"SOCFC", 63, "%", "Charger Control", true},
        {"Chgctrl", 64, "0=Enable, 1=Disable, 2=Timer", "Charger Control", true},
        {"ChgAcVolt", 65, "Vac", "Charger Control", true},
        {"ChgEff", 66, "%", "Charger Control", true},
        {"ConfigFoccci", 67, "0=Off, 1=On, 2=na", "Charger Control", true},
        {"DCdc_Type", 68, "0=NoDCDC, 1=TeslaG2", "DC-DC Converter", true},
        {"DCSetPnt", 69, "V", "DC-DC Converter", true},
        {"BMS_Timeout", 70, "sec", "Battery Management", true},
        {"BMS_VminLimit", 71, "V", "Battery Management", true},
        {"BMS_VmaxLimit", 72, "V", "Battery Management", true},
        {"BMS_TminLimit", 73, "°C", "Battery Management", true},
        {"BMS_TmaxLimit", 74, "°C", "Battery Management", true},
        {"Heater", 75, "0=None, 1=Ampera, 2=VW, 3=OpenI, 4=TeslaRear, 5=Outlander, 6=i3", "Heater Module", true},
        {"Control", 76, "0=Disable, 1=Enable, 2=Timer", "Heater Module", true},
        {"HeatPwr", 77, "W", "Heater Module", true},
        {"HeatPercnt", 78, "%", "Heater Module", true},
        {"Set_Day", 79, "0=Sun, 1=Mon, 2=Tue, 3=Wed, 4=Thu, 5=Fri, 6=Sat", "RTC Module", true},
        {"Set_Hour", 80, "Hours", "RTC Module", true},
        {"Set_Min", 81, "Mins", "RTC Module", true},
        {"Set_Sec", 82, "Secs", "RTC Module", true},
        {"Chg_Hrs", 83, "Hours", "RTC Module", true},
        {"Chg_Min", 84, "Mins", "RTC Module", true},
        {"Chg_Dur", 85, "Mins", "RTC Module", true},
        {"Pre_Hrs", 86, "Hours", "RTC Module", true},
        {"Pre_Min", 87, "Mins", "RTC Module", true},
        {"Pre_Dur", 88, "Mins", "RTC Module", true},
        {"PumpPWM", 89, "0=GS450hOil, 1=TachoOut", "General Purpose I/O", true},
        {"Out1Func", 90, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"Out2Func", 91, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"Out3Func", 92, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"SL1Func", 93, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"SL2Func", 94, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"PWM1Func", 95, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"PWM2Func", 96, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"PWM3Func", 97, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"GP12VInFunc", 98, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"HVReqFunc", 99, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"PB1InFunc", 100, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"PB2InFunc", 101, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"PB3InFunc", 102, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"GPA1Func", 103, "0=None, 1=ProxPilot, 2=BrakePres, 3=BrakeVacuum, 4=DigiPot1, 5=DigiPot2, 6=TempSensor, 7=ISA_CFG", "General Purpose I/O", true},
        {"GPA2Func", 104, "0=None, 1=ProxPilot, 2=BrakePres, 3=BrakeVacuum, 4=DigiPot1, 5=DigiPot2, 6=TempSensor, 7=ISA_CFG", "General Purpose I/O", true},
        {"ppthresh", 105, "dig", "General Purpose I/O", true},
        {"BrkVacThresh", 106, "dig", "General Purpose I/O", true},
        {"BrkVacHyst", 107, "dig", "General Purpose I/O", true},
        {"DigiPot1Step", 108, "dig", "General Purpose I/O", true},
        {"DigiPot2Step", 109, "dig", "General Purpose I/O", true},
        {"FanTemp", 110, "°C", "General Purpose I/O", true},
        {"TachoPPR", 111, "PPR", "General Purpose I/O", true},
        {"IsaInit", 112, "0=Off, 1=On, 2=na", "ISA Shunt Control", true},
        {"Tim3_Presc", 113, "", "PWM Control", true},
        {"Tim3_Period", 114, "", "PWM Control", true},
        {"Tim3_1_OC", 115, "", "PWM Control", true},
        {"Tim3_2_OC", 116, "", "PWM Control", true},
        {"Tim3_3_OC", 117, "", "PWM Control", true},
        {"CP_PWM", 118, "", "PWM Control", true}
    };
    
    int paramCount = sizeof(params) / sizeof(params[0]);
    Serial.printf("[WEB] Querying %d parameters via SDO\n", paramCount);
    
    for (int idx = 0; idx < paramCount; idx++) {
        ParamDef& p = params[idx];
        int32_t value = 0;
        bool gotValue = false;
        
        Serial.printf("[WEB] Querying param %d (%s)...\n", p.id, p.name);
        
        // Flush RX buffer
        twai_message_t dummyMsg;
        while (twai_receive(&dummyMsg, 0) == ESP_OK) {}
        
        // Send SDO Upload request
        twai_message_t txMsg;
        txMsg.identifier = 0x603;
        txMsg.extd = 0;
        txMsg.rtr = 0;
        txMsg.data_length_code = 8;
        txMsg.data[0] = 0x40;
        txMsg.data[1] = 0x00;
        txMsg.data[2] = 0x21;
        txMsg.data[3] = p.id & 0xFF;
        txMsg.data[4] = 0x00;
        txMsg.data[5] = 0x00;
        txMsg.data[6] = 0x00;
        txMsg.data[7] = 0x00;
        
        if (twai_transmit(&txMsg, pdMS_TO_TICKS(10)) == ESP_OK) {
            uint32_t startTime = millis();
            int messagesChecked = 0;
            while (millis() - startTime < 500 && messagesChecked < 50) {
                twai_message_t rxMsg;
                if (twai_receive(&rxMsg, pdMS_TO_TICKS(10)) == ESP_OK) {
                    messagesChecked++;
                    
                    if (rxMsg.identifier == 0x583 &&
                        rxMsg.data[1] == 0x00 &&
                        rxMsg.data[2] == 0x21 &&
                        rxMsg.data[3] == (p.id & 0xFF)) {
                        
                        if (rxMsg.data[0] == 0x43 || rxMsg.data[0] == 0x4B) {
                            value = rxMsg.data[4] | 
                                   (rxMsg.data[5] << 8) | 
                                   (rxMsg.data[6] << 16) | 
                                   (rxMsg.data[7] << 24);
                            gotValue = true;
                            Serial.printf("[WEB] ✓ Got value for %s: %d (checked %d msgs)\n", p.name, value, messagesChecked);
                            break;
                        } else if (rxMsg.data[0] == 0x80) {
                            Serial.printf("[WEB] ✗ Param %s doesn't exist (abort)\n", p.name);
                            break;
                        }
                    }
                }
            }
            
            if (!gotValue && messagesChecked > 0) {
                Serial.printf("[WEB] ✗ No response for %s after %d messages\n", p.name, messagesChecked);
            }
        }
        
        // Build JSON response
        JsonObject paramObj = doc.createNestedObject(p.name);
        
        // Apply fixed-point conversion (divide by 32) for parameters that need it
        float displayValue = value;
        if (!usesRawValue(p.name)) {
            displayValue = value / 32.0f;
        }
        
        paramObj["value"] = displayValue;
        paramObj["unit"] = p.unit;
        paramObj["isparam"] = p.isEditable;
        paramObj["i"] = p.id;
        paramObj["category"] = p.category;
        
        delay(5);
    }
    
    Serial.printf("[WEB] ========================================\n");
    Serial.printf("[WEB] Finished querying %d parameters\n", paramCount);
    Serial.printf("[WEB] ========================================\n");
    
    String response;
    serializeJson(doc, response);
    
    if (response.length() < 5) {
        response = "{}";
    }
    
    Serial.printf("[WEB] JSON response ready (%d bytes)\n", response.length());
    return response;
}


bool WebInterface::setParameterValue(int paramId, int32_t value) {
    Serial.printf("[WEB] Setting param %d to %d\n", paramId, value);
    
    // Find parameter name from ID
    const char* paramName = nullptr;
    
    // Use the hardcoded params array to look up name
    struct ParamDef {
        const char* name;
        int id;
        const char* unit;
        const char* category;
        bool isEditable;
    };
    
    ParamDef params[] = {
        {"Inverter", 0, "0=None, 1=Leaf_Gen1, 2=GS450h, 3=UserCAN, 4=OpenI, 5=Prius_Gen3, 6=RearAC, 7=E65_MT, 8=CanOI, 9=E65_BMW", "General Setup", true},
        {"Vehicle", 1, "0=BMW_E46, 1=BMW_E6x+, 2=Classic, 3=None, 4=User, 5=CPC", "General Setup", true},
        {"GearLvr", 2, "0=None, 1=BMW_F30, 2=JLR_G1, 3=JLR_G2, 4=VAG, 5=Outlander", "General Setup", true},
        {"Transmission", 3, "0=Manual, 1=Auto", "General Setup", true},
        {"interface", 4, "0=Unused, 1=i3LIM, 2=Chademo, 3=CPC", "General Setup", true},
        {"chargemodes", 5, "0=Off, 1=EXT_DIGI, 2=Volt_A, 3=Volt_A+EXT, 4=i3LIM, 5=i3LIM+EXT, 6=Focci, 7=Leaf_PDM, 8=Outlander, 9=CPC, 10=Leaf_Inverter, 11=Tesla_M3_OBC", "General Setup", true},
        {"BMS_Mode", 6, "0=Off, 1=SimpBMS, 2=TiDaisyChain, 3=BMWi3LIM, 4=OutlanderFront, 5=VWeBMS, 6=OutlanderFront+Rear, 7=Volt, 8=LeafGen2, 9=CPC, 10=RenaultZoe, 11=TeslaM3, 12=VictronCAN", "General Setup", true},
        {"ShuntType", 7, "0=None, 1=ISA, 2=SBOX, 3=VAG", "General Setup", true},
        {"InverterCan", 8, "0=CAN1, 1=CAN2", "General Setup", true},
        {"VehicleCan", 9, "0=CAN1, 1=CAN2", "General Setup", true},
        {"ShuntCan", 10, "0=CAN1, 1=CAN2", "General Setup", true},
        {"LimCan", 11, "0=CAN1, 1=CAN2", "General Setup", true},
        {"ChargerCan", 12, "0=CAN1, 1=CAN2", "General Setup", true},
        {"BMSCan", 13, "0=CAN1, 1=CAN2", "General Setup", true},
        {"OBD2Can", 14, "0=CAN1, 1=CAN2", "General Setup", true},
        {"CanMapCan", 15, "0=CAN1, 1=CAN2", "General Setup", true},
        {"DCDCCan", 16, "0=CAN1, 1=CAN2", "General Setup", true},
        {"HeaterCan", 17, "0=CAN1, 1=CAN2", "General Setup", true},
        {"MotActive", 18, "0=Mg1and2, 1=Mg1, 2=Mg2, 3=Both", "General Setup", true},
        {"potmin", 19, "dig", "Throttle", true},
        {"potmax", 20, "dig", "Throttle", true},
        {"pot2min", 21, "dig", "Throttle", true},
        {"pot2max", 22, "dig", "Throttle", true},
        {"regenrpm", 23, "rpm", "Throttle", true},
        {"regenendrpm", 24, "rpm", "Throttle", true},
        {"regenmax", 25, "%", "Throttle", true},
        {"regenBrake", 26, "%", "Throttle", true},
        {"regenramp", 27, "%/10ms", "Throttle", true},
        {"potmode", 28, "0=SingleChannel, 1=DualChannel", "Throttle", true},
        {"dirmode", 29, "0=Button, 1=Switch, 2=ButtonReversed, 3=SwitchReversed, 4=DefaultForward", "Throttle", true},
        {"reversemotor", 30, "0=Off, 1=On, 2=na", "Throttle", true},
        {"throtramp", 31, "%/10ms", "Throttle", true},
        {"throtramprpm", 32, "rpm", "Throttle", true},
        {"revlim", 33, "rpm", "Throttle", true},
        {"revRegen", 34, "0=Off, 1=On, 2=na", "Throttle", true},
        {"udcmin", 35, "V", "Throttle", true},
        {"udclim", 36, "V", "Throttle", true},
        {"idcmax", 37, "A", "Throttle", true},
        {"idcmin", 38, "A", "Throttle", true},
        {"tmphsmax", 39, "°C", "Throttle", true},
        {"tmpmmax", 40, "°C", "Throttle", true},
        {"throtmax", 41, "%", "Throttle", true},
        {"throtmin", 42, "%", "Throttle", true},
        {"throtmaxRev", 43, "%", "Throttle", true},
        {"throtdead", 44, "%", "Throttle", true},
        {"RegenBrakeLight", 45, "%", "Throttle", true},
        {"throtrpmfilt", 46, "rpm/10ms", "Throttle", true},
        {"Gear", 47, "0=LOW, 1=HIGH, 2=AUTO, 3=HILLHOLD", "Gearbox Control", true},
        {"OilPump", 48, "%", "Gearbox Control", true},
        {"cruisestep", 49, "rpm", "Cruise Control", true},
        {"cruiseramp", 50, "rpm/100ms", "Cruise Control", true},
        {"regenlevel", 51, "", "Cruise Control", true},
        {"udcsw", 52, "V", "Contactor Control", true},
        {"cruiselight", 53, "0=Off, 1=On, 2=na", "Contactor Control", true},
        {"errlights", 54, "0=Off, 4=EPC, 8=engine", "Contactor Control", true},
        {"CAN3Speed", 55, "0=k33.3, 1=k500, 2=k100", "Communication", true},
        {"BattCap", 56, "kWh", "Charger Control", true},
        {"Voltspnt", 57, "V", "Charger Control", true},
        {"Pwrspnt", 58, "W", "Charger Control", true},
        {"IdcTerm", 59, "A", "Charger Control", true},
        {"CCS_ICmd", 60, "A", "Charger Control", true},
        {"CCS_ILim", 61, "A", "Charger Control", true},
        {"CCS_SOCLim", 62, "%", "Charger Control", true},
        {"SOCFC", 63, "%", "Charger Control", true},
        {"Chgctrl", 64, "0=Enable, 1=Disable, 2=Timer", "Charger Control", true},
        {"ChgAcVolt", 65, "Vac", "Charger Control", true},
        {"ChgEff", 66, "%", "Charger Control", true},
        {"ConfigFoccci", 67, "0=Off, 1=On, 2=na", "Charger Control", true},
        {"DCdc_Type", 68, "0=NoDCDC, 1=TeslaG2", "DC-DC Converter", true},
        {"DCSetPnt", 69, "V", "DC-DC Converter", true},
        {"BMS_Timeout", 70, "sec", "Battery Management", true},
        {"BMS_VminLimit", 71, "V", "Battery Management", true},
        {"BMS_VmaxLimit", 72, "V", "Battery Management", true},
        {"BMS_TminLimit", 73, "°C", "Battery Management", true},
        {"BMS_TmaxLimit", 74, "°C", "Battery Management", true},
        {"Heater", 75, "0=None, 1=Ampera, 2=VW, 3=OpenI, 4=TeslaRear, 5=Outlander, 6=i3", "Heater Module", true},
        {"Control", 76, "0=Disable, 1=Enable, 2=Timer", "Heater Module", true},
        {"HeatPwr", 77, "W", "Heater Module", true},
        {"HeatPercnt", 78, "%", "Heater Module", true},
        {"Set_Day", 79, "0=Sun, 1=Mon, 2=Tue, 3=Wed, 4=Thu, 5=Fri, 6=Sat", "RTC Module", true},
        {"Set_Hour", 80, "Hours", "RTC Module", true},
        {"Set_Min", 81, "Mins", "RTC Module", true},
        {"Set_Sec", 82, "Secs", "RTC Module", true},
        {"Chg_Hrs", 83, "Hours", "RTC Module", true},
        {"Chg_Min", 84, "Mins", "RTC Module", true},
        {"Chg_Dur", 85, "Mins", "RTC Module", true},
        {"Pre_Hrs", 86, "Hours", "RTC Module", true},
        {"Pre_Min", 87, "Mins", "RTC Module", true},
        {"Pre_Dur", 88, "Mins", "RTC Module", true},
        {"PumpPWM", 89, "0=GS450hOil, 1=TachoOut", "General Purpose I/O", true},
        {"Out1Func", 90, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"Out2Func", 91, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"Out3Func", 92, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"SL1Func", 93, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"SL2Func", 94, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"PWM1Func", 95, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"PWM2Func", 96, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"PWM3Func", 97, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"GP12VInFunc", 98, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"HVReqFunc", 99, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"PB1InFunc", 100, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"PB2InFunc", 101, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"PB3InFunc", 102, "0=None, 1=ChaDeMoAlw, 2=OBCEnable, 3=OBCWakeup, 4=ChargeLight, 5=NegContactorOut", "General Purpose I/O", true},
        {"GPA1Func", 103, "0=None, 1=ProxPilot, 2=BrakePres, 3=BrakeVacuum, 4=DigiPot1, 5=DigiPot2, 6=TempSensor, 7=ISA_CFG", "General Purpose I/O", true},
        {"GPA2Func", 104, "0=None, 1=ProxPilot, 2=BrakePres, 3=BrakeVacuum, 4=DigiPot1, 5=DigiPot2, 6=TempSensor, 7=ISA_CFG", "General Purpose I/O", true},
        {"ppthresh", 105, "dig", "General Purpose I/O", true},
        {"BrkVacThresh", 106, "dig", "General Purpose I/O", true},
        {"BrkVacHyst", 107, "dig", "General Purpose I/O", true},
        {"DigiPot1Step", 108, "dig", "General Purpose I/O", true},
        {"DigiPot2Step", 109, "dig", "General Purpose I/O", true},
        {"FanTemp", 110, "°C", "General Purpose I/O", true},
        {"TachoPPR", 111, "PPR", "General Purpose I/O", true},
        {"IsaInit", 112, "0=Off, 1=On, 2=na", "ISA Shunt Control", true},
        {"Tim3_Presc", 113, "", "PWM Control", true},
        {"Tim3_Period", 114, "", "PWM Control", true},
        {"Tim3_1_OC", 115, "", "PWM Control", true},
        {"Tim3_2_OC", 116, "", "PWM Control", true},
        {"Tim3_3_OC", 117, "", "PWM Control", true},
        {"CP_PWM", 118, "", "PWM Control", true}
    };
    
    int paramCount = sizeof(params) / sizeof(params[0]);
    for (int i = 0; i < paramCount; i++) {
        if (params[i].id == paramId) {
            paramName = params[i].name;
            break;
        }
    }
    
    // Apply fixed-point encoding (multiply by 32) for parameters that need it
    int32_t encodedValue = value;
    
    if (paramName && !usesRawValue(paramName)) {
        encodedValue = value * 32;
        Serial.printf("[WEB] Encoded %d -> %d (x32) for %s\n", value, encodedValue, paramName);
    } else if (paramName) {
        Serial.printf("[WEB] Using raw value %d for %s (no encoding)\n", value, paramName);
    }
    
    // Use CANDataManager's setParameter method which handles:
    // - Special CAN mappings (Gear=0x300, MotActive=0x301, etc.)
    // - SDO writes for regular parameters
    // - Proper message queuing
    canManager->setParameter(paramId, encodedValue);
    
    Serial.printf("[WEB] ✓ Parameter change queued via CANDataManager\n");
    
    return true;
}

int32_t WebInterface::getParameterValue(int paramId) {
    CANParameter* param = canManager->getParameter(paramId);
    if (param) {
        return param->getValueAsInt();
    }
    return 0;
}

void WebInterface::handleCanSend() {
    if (corsEnabled) addCORSHeaders();
    
    if (!server.hasArg("id") || !server.hasArg("data")) {
        server.send(400, "text/plain", "Missing id or data argument");
        return;
    }
    
    uint32_t canId = server.arg("id").toInt();
    String dataStr = server.arg("data");
    
    // Parse comma-separated data bytes
    twai_message_t msg;
    msg.identifier = canId;
    msg.extd = 0;
    msg.rtr = 0;
    msg.data_length_code = 0;
    
    int byteIndex = 0;
    int startIdx = 0;
    for (int i = 0; i <= dataStr.length() && byteIndex < 8; i++) {
        if (i == dataStr.length() || dataStr.charAt(i) == ',') {
            if (i > startIdx) {
                String byteStr = dataStr.substring(startIdx, i);
                msg.data[byteIndex++] = byteStr.toInt();
            }
            startIdx = i + 1;
        }
    }
    msg.data_length_code = byteIndex;
    
    esp_err_t result = twai_transmit(&msg, pdMS_TO_TICKS(100));
    
    if (result == ESP_OK) {
        // Log the transmitted message
        logCanMessage(canId, msg.data, byteIndex, false);
        
        Serial.printf("[WEB] CAN TX: ID=0x%03X Data=[", canId);
        for (int i = 0; i < byteIndex; i++) {
            Serial.printf("%02X ", msg.data[i]);
        }
        Serial.println("]");
        server.send(200, "text/plain", "CAN message sent");
    } else {
        server.send(500, "text/plain", "Failed to send CAN message");
    }
}

void WebInterface::handleCanLog() {
    if (corsEnabled) addCORSHeaders();
    
    // Build JSON array of recent CAN messages
    JsonDocument doc;
    JsonArray messages = doc.to<JsonArray>();
    
    // Return messages in reverse order (newest first)
    for (int i = 0; i < CAN_LOG_SIZE; i++) {
        int idx = (canLogIndex - 1 - i + CAN_LOG_SIZE) % CAN_LOG_SIZE;
        CANLogMessage& msg = canLogBuffer[idx];
        
        // Skip empty slots
        if (msg.timestamp == 0) continue;
        
        JsonObject msgObj = messages.createNestedObject();
        msgObj["id"] = msg.id;
        msgObj["rx"] = msg.isRx;
        
        JsonArray dataArray = msgObj.createNestedArray("data");
        for (int j = 0; j < msg.len; j++) {
            char hexStr[8];
            sprintf(hexStr, "0x%02X", msg.data[j]);
            dataArray.add(hexStr);
        }
        
        msgObj["time"] = msg.timestamp;
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void WebInterface::logCanMessage(uint32_t id, uint8_t* data, uint8_t len, bool isRx) {
    CANLogMessage& msg = canLogBuffer[canLogIndex];
    msg.id = id;
    msg.len = len;
    msg.isRx = isRx;
    msg.timestamp = millis();
    memcpy(msg.data, data, len);
    
    canLogIndex = (canLogIndex + 1) % CAN_LOG_SIZE;
}

void WebInterface::handleParamsUpload() {
    if (corsEnabled) addCORSHeaders();
    
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "No JSON body provided");
        return;
    }
    
    String jsonBody = server.arg("plain");
    
    // Save to SPIFFS as params.json
    File file = SPIFFS.open("/params.json", "w");
    if (!file) {
        server.send(500, "text/plain", "Failed to open file for writing");
        return;
    }
    
    file.print(jsonBody);
    file.close();
    
    Serial.println("[WEB] Parameters uploaded to /params.json");
    Serial.printf("[WEB] Size: %d bytes\n", jsonBody.length());
    
    server.send(200, "text/plain", "Parameters uploaded. Reboot to apply.");
}
