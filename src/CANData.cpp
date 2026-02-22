#include "CANData.h"
#include "Config.h"
#include "driver/twai.h"

void CANParameter::toString(char* buffer, size_t bufferSize) {
    switch (dataType) {
        case PARAM_INT8:
        case PARAM_INT16:
        case PARAM_INT32:
            snprintf(buffer, bufferSize, "%d %s", (int)getValueAsInt(), unit);
            break;
        case PARAM_UINT8:
        case PARAM_UINT16:
        case PARAM_UINT32:
            snprintf(buffer, bufferSize, "%u %s", (unsigned)getValueAsInt(), unit);
            break;
        case PARAM_FLOAT:
            dtostrf(value.f32, 0, decimalPlaces, buffer);
            strcat(buffer, " ");
            strcat(buffer, unit);
            break;
    }
}

void CANParameter::setValue(int32_t val) {
    switch (dataType) {
        case PARAM_INT8:   value.i8 = (int8_t)val; break;
        case PARAM_UINT8:  value.u8 = (uint8_t)val; break;
        case PARAM_INT16:  value.i16 = (int16_t)val; break;
        case PARAM_UINT16: value.u16 = (uint16_t)val; break;
        case PARAM_INT32:  value.i32 = val; break;
        case PARAM_UINT32: value.u32 = (uint32_t)val; break;
        case PARAM_FLOAT:  value.f32 = (float)val; break;
    }
    lastUpdateTime = millis();
    dirty = false;
}

int32_t CANParameter::getValueAsInt() {
    switch (dataType) {
        case PARAM_INT8:   return value.i8;
        case PARAM_UINT8:  return value.u8;
        case PARAM_INT16:  return value.i16;
        case PARAM_UINT16: return value.u16;
        case PARAM_INT32:  return value.i32;
        case PARAM_UINT32: return value.u32;
        case PARAM_FLOAT:  return (int32_t)value.f32;
        default: return 0;
    }
}

CANDataManager::CANDataManager() 
    : parameterCount(0), txHead(0), txTail(0), rxHead(0), rxTail(0),
      connected(false), lastMessageTime(0), bmsCellCount(0) {
    // Initialize BMS cell arrays
    for (uint8_t i = 0; i < MAX_BMS_CELLS; i++) {
        bmsCellVoltages[i] = 0;
        bmsCellUpdateTimes[i] = 0;
    }
}

bool CANDataManager::init() {
    // Configure TWAI (CAN) timing for 500kbps
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    
    // Configure TWAI filter to accept all messages
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    
    // Configure TWAI general settings
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_PIN, 
        (gpio_num_t)CAN_RX_PIN, 
        TWAI_MODE_NORMAL
    );
    g_config.rx_queue_len = 10;
    g_config.tx_queue_len = 10;
    
    // Install TWAI driver
    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        #if DEBUG_CAN
        Serial.println("TWAI driver install failed");
        #endif
        return false;
    }
    
    // Start TWAI driver
    if (twai_start() != ESP_OK) {
        #if DEBUG_CAN
        Serial.println("TWAI start failed");
        #endif
        return false;
    }
    
    #if DEBUG_CAN
    Serial.println("TWAI (CAN) initialized");
    #endif
    
    return true;
}

void CANDataManager::update() {
    // Receive messages
    twai_message_t rx_message;
    while (twai_receive(&rx_message, 0) == ESP_OK) {
        #if DEBUG_CAN
        Serial.print("CAN RX: ID=0x");
        Serial.print(rx_message.identifier, HEX);
        Serial.print(" Len=");
        Serial.print(rx_message.data_length_code);
        Serial.print(" Data=[");
        for (int i = 0; i < rx_message.data_length_code; i++) {
            if (i > 0) Serial.print(" ");
            if (rx_message.data[i] < 0x10) Serial.print("0");
            Serial.print(rx_message.data[i], HEX);
        }
        Serial.println("]");
        #endif
        
        CANMessage msg;
        msg.id = rx_message.identifier;
        msg.length = rx_message.data_length_code;
        msg.timestamp = millis();
        
        for (int i = 0; i < rx_message.data_length_code && i < 8; i++) {
            msg.data[i] = rx_message.data[i];
        }
        
        enqueueRx(msg);
        lastMessageTime = millis();
        connected = true;
    }
    
    // Process received messages
    CANMessage rxMsg;
    while (dequeueRx(rxMsg)) {
        processReceivedMessage(rxMsg);
    }
    
    // Send queued messages
    CANMessage txMsg;
    if (dequeueTx(txMsg)) {
        #if DEBUG_CAN
        Serial.print("CAN TX: ID=0x");
        Serial.print(txMsg.id, HEX);
        Serial.print(" Len=");
        Serial.print(txMsg.length);
        Serial.print(" Data=[");
        for (int i = 0; i < txMsg.length; i++) {
            if (i > 0) Serial.print(" ");
            if (txMsg.data[i] < 0x10) Serial.print("0");
            Serial.print(txMsg.data[i], HEX);
        }
        Serial.println("]");
        #endif
        
        twai_message_t tx_message;
        tx_message.identifier = txMsg.id;
        tx_message.data_length_code = txMsg.length;
        tx_message.flags = TWAI_MSG_FLAG_NONE;
        
        for (int i = 0; i < txMsg.length; i++) {
            tx_message.data[i] = txMsg.data[i];
        }
        
        twai_transmit(&tx_message, pdMS_TO_TICKS(100));
    }
    
    // Check connection timeout
    if (connected && (millis() - lastMessageTime > 5000)) {
        connected = false;
    }
}

bool CANDataManager::loadParametersFromJSON(const char* jsonString) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        #if DEBUG_SERIAL
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
        #endif
        return false;
    }
    
    parameterCount = 0;
    JsonArray params = doc["parameters"].as<JsonArray>();
    
    for (JsonObject param : params) {
        if (parameterCount >= MAX_PARAMETERS) break;
        
        CANParameter& p = parameters[parameterCount];
        p.id = param["id"];
        strncpy(p.name, param["name"] | "Unknown", 31);
        
        String type = param["type"] | "int16";
        if (type == "int8") p.dataType = PARAM_INT8;
        else if (type == "uint8") p.dataType = PARAM_UINT8;
        else if (type == "int16") p.dataType = PARAM_INT16;
        else if (type == "uint16") p.dataType = PARAM_UINT16;
        else if (type == "int32") p.dataType = PARAM_INT32;
        else if (type == "uint32") p.dataType = PARAM_UINT32;
        else if (type == "float") p.dataType = PARAM_FLOAT;
        
        p.editable = param["editable"] | false;
        p.minValue = param["min"] | 0;
        p.maxValue = param["max"] | 100;
        strncpy(p.unit, param["unit"] | "", 7);
        p.decimalPlaces = param["decimals"] | 0;
        p.dirty = true;
        
        parameterCount++;
    }
    
    #if DEBUG_SERIAL
    Serial.printf("Loaded %d parameters\n", parameterCount);
    #endif
    
    return true;
}

CANParameter* CANDataManager::getParameter(uint16_t id) {
    for (uint16_t i = 0; i < parameterCount; i++) {
        if (parameters[i].id == id) {
            return &parameters[i];
        }
    }
    return nullptr;
}

CANParameter* CANDataManager::getParameterByIndex(uint8_t index) {
    if (index < parameterCount) {
        return &parameters[index];
    }
    return nullptr;
}

void CANDataManager::requestParameter(uint16_t paramId) {
    // SDO Read Request - Correct CANopen format with index 0x2100
    CANMessage msg;
    msg.id = 0x600 + CAN_NODE_ID;
    msg.length = 8;
    msg.data[0] = 0x40;  // SDO read request
    msg.data[1] = 0x00;  // Index low byte (0x2100 & 0xFF)
    msg.data[2] = 0x21;  // Index high byte (0x2100 >> 8)
    msg.data[3] = paramId & 0xFF;  // Subindex = parameter ID
    msg.data[4] = 0x00;
    msg.data[5] = 0x00;
    msg.data[6] = 0x00;
    msg.data[7] = 0x00;
    msg.timestamp = millis();
    
    enqueueTx(msg);
}

void CANDataManager::setParameter(uint16_t paramId, int32_t value) {
    CANMessage msg;
    msg.timestamp = millis();
    
    // Use direct CAN mappings instead of SDO for control parameters
    switch(paramId) {
        case 27:  // Gear
            msg.id = 0x300;
            msg.length = 8;
            msg.data[0] = (uint8_t)value;  // 0-3
            msg.data[1] = 0x00;
            msg.data[2] = 0x00;
            msg.data[3] = 0x00;
            msg.data[4] = 0x00;
            msg.data[5] = 0x00;
            msg.data[6] = 0x00;
            msg.data[7] = 0x00;
            #if DEBUG_SERIAL
            Serial.printf("Sending Gear change to 0x300: value=%d\n", value);
            #endif
            // Optimistic update - immediately update local value
            updateParameterIfExists(27, value);
            break;
            
        case 129:  // MotActive
            msg.id = 0x301;
            msg.length = 8;
            msg.data[0] = (uint8_t)value;  // 0-3
            msg.data[1] = 0x00;
            msg.data[2] = 0x00;
            msg.data[3] = 0x00;
            msg.data[4] = 0x00;
            msg.data[5] = 0x00;
            msg.data[6] = 0x00;
            msg.data[7] = 0x00;
            #if DEBUG_SERIAL
            Serial.printf("Sending Motor change to 0x301: value=%d\n", value);
            #endif
            // Optimistic update - immediately update local value
            updateParameterIfExists(129, value);
            break;
            
        case 61:  // regenmax
            msg.id = 0x302;
            msg.length = 8;
            msg.data[0] = value & 0xFF;         // Low byte
            msg.data[1] = (value >> 8) & 0xFF;  // High byte (sign)
            msg.data[2] = 0x00;
            msg.data[3] = 0x00;
            msg.data[4] = 0x00;
            msg.data[5] = 0x00;
            msg.data[6] = 0x00;
            msg.data[7] = 0x00;
            #if DEBUG_SERIAL
            Serial.printf("Sending Regen change to 0x302: value=%d\n", value);
            #endif
            // Optimistic update - immediately update local value
            updateParameterIfExists(61, value);
            break;
            
        default:
            // For other parameters, use SDO Write - Correct CANopen format
            msg.id = 0x600 + CAN_NODE_ID;
            msg.length = 8;
            msg.data[0] = 0x23;  // SDO write 4 bytes
            msg.data[1] = 0x00;  // Index low byte (0x2100 & 0xFF)
            msg.data[2] = 0x21;  // Index high byte (0x2100 >> 8)
            msg.data[3] = paramId & 0xFF;  // Subindex = parameter ID
            msg.data[4] = value & 0xFF;
            msg.data[5] = (value >> 8) & 0xFF;
            msg.data[6] = (value >> 16) & 0xFF;
            msg.data[7] = (value >> 24) & 0xFF;
            #if DEBUG_SERIAL
            Serial.printf("Sending SDO write for param %d = %d\n", paramId, value);
            #endif
            break;
    }
    
    enqueueTx(msg);
}

void CANDataManager::processReceivedMessage(CANMessage& msg) {
    // Log ALL received messages for debugging
    #if DEBUG_CAN
    Serial.print("Processing CAN ID: 0x");
    Serial.print(msg.id, HEX);
    Serial.print(" (");
    Serial.print(msg.id);
    Serial.println(")");
    #endif
    
    // Check for BMS cell voltage messages (0x400-0x4FF or 0x600-0x6FF)
    if ((msg.id >= 0x400 && msg.id < 0x500) || (msg.id >= 0x600 && msg.id < 0x700)) {
        #if DEBUG_CAN
        Serial.println("  -> BMS cell voltage message detected");
        #endif
        handleBMSCellVoltage(msg);
        return;
    }
    
    // Check if it's an SDO response (0x580 + node_id)
    if (msg.id == (0x580 + CAN_NODE_ID)) {
        #if DEBUG_CAN
        Serial.println("  -> SDO Response detected");
        #endif
        handleSDOResponse(msg);
    }
    // Check if it's a PDO message from Node 3
    // TPDO1: 0x180 + node_id = 0x183
    // TPDO2: 0x280 + node_id = 0x283
    // TPDO3: 0x380 + node_id = 0x383
    // TPDO4: 0x480 + node_id = 0x483
    else if (msg.id == (0x180 + CAN_NODE_ID) || 
             msg.id == (0x280 + CAN_NODE_ID) ||
             msg.id == (0x380 + CAN_NODE_ID) ||
             msg.id == (0x480 + CAN_NODE_ID)) {
        #if DEBUG_CAN
        Serial.print("  -> PDO message detected from Node ");
        Serial.println(CAN_NODE_ID);
        #endif
        handlePDOMessage(msg);
    }
    // Try to parse 0x521-0x528 (ZombieVerter 6-byte format)
    else if (msg.id >= 0x521 && msg.id <= 0x528) {
        #if DEBUG_CAN
        Serial.println("  -> ZombieVerter 0x5XX message, parsing");
        #endif
        handleGenericMessage(msg);
    }
    // Try to parse 0x500-0x5FF range (non-standard format)
    else if (msg.id >= 0x500 && msg.id <= 0x5FF) {
        #if DEBUG_CAN
        Serial.println("  -> Non-standard ID in 0x5XX range, attempting to parse");
        #endif
        handleGenericMessage(msg);
    }
    // Try ANY message that has 8 bytes of data
    else if (msg.length == 8) {
        #if DEBUG_CAN
        Serial.println("  -> 8-byte message, attempting generic parse");
        #endif
        handleGenericMessage(msg);
    }
    else {
        #if DEBUG_CAN
        Serial.println("  -> Unrecognized message format");
        #endif
    }
}

void CANDataManager::handleSDOResponse(CANMessage& msg) {
    if (msg.length < 8) {
        #if DEBUG_CAN
        Serial.println("  -> SDO message too short");
        #endif
        return;
    }
    
    uint8_t cmd = msg.data[0];
    uint8_t indexLow = msg.data[1];   // Should be 0x00
    uint8_t indexHigh = msg.data[2];  // Should be 0x21
    uint8_t paramId = msg.data[3];    // Subindex = parameter ID
    
    // Validate this is a response for index 0x2100
    uint16_t index = indexLow | (indexHigh << 8);
    if (index != 0x2100) {
        #if DEBUG_CAN
        Serial.printf("  -> Wrong index 0x%04X (expected 0x2100)\n", index);
        #endif
        return;
    }
    
    #if DEBUG_CAN
    Serial.print("  -> CMD=0x");
    Serial.print(cmd, HEX);
    Serial.print(" ParamID=");
    Serial.println(paramId);
    #endif
    
    CANParameter* param = getParameter(paramId);
    if (!param) {
        #if DEBUG_CAN
        Serial.print("  -> Parameter ID ");
        Serial.print(paramId);
        Serial.println(" not found in params list!");
        #endif
        return;
    }
    
    // SDO Read response: 0x43 or 0x4B
    // 0x80 = Abort (skip these - parameter doesn't exist)
    if (cmd == 0x43 || cmd == 0x4B) {
        int32_t value = msg.data[4] | (msg.data[5] << 8) | 
                       (msg.data[6] << 16) | (msg.data[7] << 24);
        param->setValue(value);
        
        #if DEBUG_CAN
        Serial.print("  -> Updated ");
        Serial.print(param->name);
        Serial.print(" = ");
        Serial.println(value);
        #endif
    } else if (cmd == 0x80) {
        #if DEBUG_CAN
        uint32_t abortCode = msg.data[4] | (msg.data[5] << 8) | 
                            (msg.data[6] << 16) | (msg.data[7] << 24);
        Serial.printf("  -> SDO Abort for param %d: 0x%08X\n", paramId, abortCode);
        #endif
        // Don't update parameter value on abort
    } else {
        #if DEBUG_CAN
        Serial.print("  -> Unexpected SDO command: 0x");
        Serial.println(cmd, HEX);
        #endif
    }
}

void CANDataManager::handlePDOMessage(CANMessage& msg) {
    // ZombieVerter PDO format (typical):
    // PDO messages contain packed parameter data
    // Format varies by configuration, but commonly:
    // - 2 bytes per int16 parameter
    // - 4 parameters per 8-byte message
    
    #if DEBUG_CAN
    Serial.print("  -> Parsing PDO, length=");
    Serial.println(msg.length);
    #endif
    
    // Determine which PDO this is
    uint8_t pdoNum = 0;
    if (msg.id == (0x180 + CAN_NODE_ID)) pdoNum = 1;
    else if (msg.id == (0x280 + CAN_NODE_ID)) pdoNum = 2;
    else if (msg.id == (0x380 + CAN_NODE_ID)) pdoNum = 3;
    else if (msg.id == (0x480 + CAN_NODE_ID)) pdoNum = 4;
    
    // TPDO1 (0x183): Typically motor speed, voltage, current, power
    if (pdoNum == 1 && msg.length >= 8) {
        // Extract 4x int16 values
        int16_t val0 = msg.data[0] | (msg.data[1] << 8);
        int16_t val1 = msg.data[2] | (msg.data[3] << 8);
        int16_t val2 = msg.data[4] | (msg.data[5] << 8);
        int16_t val3 = msg.data[6] | (msg.data[7] << 8);
        
        // Map to parameter IDs (adjust these based on your ZombieVerter config)
        // Common mapping: ID1=Speed, ID2=Voltage, ID3=Current, ID4=Power
        updateParameterIfExists(1, val0);  // Speed/RPM
        updateParameterIfExists(3, val1);  // Voltage
        updateParameterIfExists(4, val2);  // Current  
        updateParameterIfExists(2, val3);  // Power
        
        #if DEBUG_CAN
        Serial.printf("  -> TPDO1: %d, %d, %d, %d\n", val0, val1, val2, val3);
        #endif
    }
    
    // TPDO2 (0x283): Typically temperatures, throttle, etc.
    else if (pdoNum == 2 && msg.length >= 8) {
        int16_t val0 = msg.data[0] | (msg.data[1] << 8);
        int16_t val1 = msg.data[2] | (msg.data[3] << 8);
        int16_t val2 = msg.data[4] | (msg.data[5] << 8);
        int16_t val3 = msg.data[6] | (msg.data[7] << 8);
        
        // Common mapping: ID5=MotorTemp, ID6=InverterTemp, ID7=SOC, etc.
        updateParameterIfExists(5, val0);  // Motor temp
        updateParameterIfExists(6, val1);  // Inverter temp
        updateParameterIfExists(7, val2);  // SOC / Battery info
        updateParameterIfExists(8, val3);  // Other
        
        #if DEBUG_CAN
        Serial.printf("  -> TPDO2: %d, %d, %d, %d\n", val0, val1, val2, val3);
        #endif
    }
    
    // TPDO3 and TPDO4 can be added similarly if needed
}

bool CANDataManager::enqueueTx(CANMessage& msg) {
    uint8_t next = (txHead + 1) % TX_QUEUE_SIZE;
    if (next == txTail) return false;
    
    txQueue[txHead] = msg;
    txHead = next;
    return true;
}

bool CANDataManager::dequeueTx(CANMessage& msg) {
    if (txHead == txTail) return false;
    
    msg = txQueue[txTail];
    txTail = (txTail + 1) % TX_QUEUE_SIZE;
    return true;
}

bool CANDataManager::enqueueRx(CANMessage& msg) {
    uint8_t next = (rxHead + 1) % RX_QUEUE_SIZE;
    if (next == rxTail) return false;
    
    rxQueue[rxHead] = msg;
    rxHead = next;
    return true;
}

bool CANDataManager::dequeueRx(CANMessage& msg) {
    if (rxHead == rxTail) return false;
    
    msg = rxQueue[rxTail];
    rxTail = (rxTail + 1) % RX_QUEUE_SIZE;
    return true;
}

void CANDataManager::updateParameterIfExists(uint16_t paramId, int32_t value) {
    CANParameter* param = getParameter(paramId);
    if (param) {
        param->setValue(value);
        #if DEBUG_CAN
        Serial.print("    -> Updated param ");
        Serial.print(paramId);
        Serial.print(" (");
        Serial.print(param->name);
        Serial.print(") = ");
        Serial.println(value);
        #endif
    }
}

void CANDataManager::handleGenericMessage(CANMessage& msg) {
    // Parse based on user's specific CAN mapping configuration
    
    // Direct CAN-mapped control parameters (user-configured in ZombieVerter)
    // 0x300 (768): Gear parameter (ID 27)
    if (msg.id == 0x300 && msg.length >= 1) {
        uint8_t gear = msg.data[0];
        updateParameterIfExists(27, gear);
        #if DEBUG_CAN
        Serial.printf("  -> 0x300: Gear=%d\n", gear);
        #endif
        return;
    }
    
    // 0x301 (769): Motor Active parameter (ID 129)
    if (msg.id == 0x301 && msg.length >= 1) {
        uint8_t motor = msg.data[0];
        updateParameterIfExists(129, motor);
        #if DEBUG_CAN
        Serial.printf("  -> 0x301: MotActive=%d\n", motor);
        #endif
        return;
    }
    
    // 0x302 (770): Regen Max parameter (ID 61)
    if (msg.id == 0x302 && msg.length >= 2) {
        int16_t regen = msg.data[0] | (msg.data[1] << 8);
        updateParameterIfExists(61, regen);
        #if DEBUG_CAN
        Serial.printf("  -> 0x302: RegenMax=%d\n", regen);
        #endif
        return;
    }
    
    // IVT-S (Isabellenhutte) Shunt Messages - 6 byte format
    // Format: [ID, Reserved, Data_Low, Data_Mid, Data_High, Reserved]
    // Value is 32-bit signed integer in bytes 2-4 (little-endian, 24-bit actually used)
    
    // 0x522 (1314): Voltage 2 (U2) in mV - THIS IS THE MAIN BATTERY VOLTAGE
    if (msg.id == 0x522 && msg.length == 6) {
        int32_t voltage_mv = msg.data[2] | (msg.data[3] << 8) | (msg.data[4] << 16);
        // Sign extend if bit 23 is set
        if (voltage_mv & 0x800000) voltage_mv |= 0xFF000000;
        
        // Convert mV to decivolts (for display with 1 decimal place)
        // 311059 mV / 1000 = 311 decivolts (displays as 311.0V with 1 decimal)
        int32_t voltage_dv = voltage_mv / 1000;
        
        Serial.println("=== IVT-S VOLTAGE (U2) ===");
        Serial.printf("Raw: %d mV, Scaled: %d dV (%.1f V)\n", voltage_mv, voltage_dv, voltage_mv/1000.0);
        Serial.println("==========================");
        
        updateParameterIfExists(3, voltage_dv);  // ID 3 = Voltage
        return;
    }
    
    // 0x521 (1313): Voltage 1 (U1) in mV - Optional/unused channel
    if (msg.id == 0x521 && msg.length == 6) {
        int32_t voltage_mv = msg.data[2] | (msg.data[3] << 8) | (msg.data[4] << 16);
        if (voltage_mv & 0x800000) voltage_mv |= 0xFF000000;
        
        Serial.printf("IVT-S U1 (optional): %d mV (%.1f V)\n", voltage_mv, voltage_mv/1000.0);
        // Don't update main voltage parameter - this is a secondary channel
        return;
    }
    
    // 0x411 (1041): Current (I) in mA  
    if (msg.id == 0x411 && msg.length == 6) {
        int32_t current_ma = msg.data[2] | (msg.data[3] << 8) | (msg.data[4] << 16);
        // Sign extend if bit 23 is set
        if (current_ma & 0x800000) current_ma |= 0xFF000000;
        
        // Convert mA to deciamps (for display with 1 decimal place)
        // 1340 mA / 1000 = 1 deciamp (displays as 1.3A with 1 decimal)
        int32_t current_da = current_ma / 1000;
        
        Serial.println("=== IVT-S CURRENT ===");
        Serial.printf("Raw: %d mA, Scaled: %d dA (%.2f A)\n", current_ma, current_da, current_ma/1000.0);
        Serial.println("=====================");
        
        updateParameterIfExists(4, current_da);  // ID 4 = Current
        return;
    }
    
    // 0x527 (1319): Power (P) in W
    if (msg.id == 0x527 && msg.length == 6) {
        int32_t power_w = msg.data[2] | (msg.data[3] << 8) | (msg.data[4] << 16);
        // Sign extend if bit 23 is set  
        if (power_w & 0x800000) power_w |= 0xFF000000;
        
        // Convert W to decikilowatts (for display as kW with 1 decimal)
        // 1753 W / 1000 = 1 deci-kW (displays as 1.7-1.8 kW with 1 decimal)
        int32_t power_dkw = power_w / 1000;
        
        Serial.println("=== IVT-S POWER ===");
        Serial.printf("Raw: %d W, Scaled: %d dkW (%.1f kW)\n", power_w, power_dkw, power_w/1000.0);
        Serial.println("===================");
        
        updateParameterIfExists(2, power_dkw);  // ID 2 = Power
        return;
    }
    
    // 0x528 (1320): Charge (As) in Ampere-seconds
    if (msg.id == 0x528 && msg.length == 6) {
        int32_t charge_as = msg.data[2] | (msg.data[3] << 8) | (msg.data[4] << 16);
        // Sign extend if bit 23 is set
        if (charge_as & 0x800000) charge_as |= 0xFF000000;
        
        // Convert As to Ah
        int32_t charge_ah = charge_as / 3600;
        
        Serial.println("=== IVT-S CHARGE ===");
        Serial.printf("Raw: %d As, Scaled: %d Ah\n", charge_as, charge_ah);
        Serial.println("====================");
        
        updateParameterIfExists(15, charge_ah);  // ID 15 = Charge counter
        return;
    }
    
    // 0x523: Voltage 3 (U3) - Optional/unused
    if (msg.id == 0x523 && msg.length == 6) {
        int32_t voltage_mv = msg.data[2] | (msg.data[3] << 8) | (msg.data[4] << 16);
        if (voltage_mv & 0x800000) voltage_mv |= 0xFF000000;
        
        Serial.printf("IVT-S U3 (optional): %d mV (%.1f V)\n", voltage_mv, voltage_mv/1000.0);
        return;
    }
    
    // 0x526: Temperature
    if (msg.id == 0x526 && msg.length == 6) {
        int32_t temp_raw = msg.data[2] | (msg.data[3] << 8) | (msg.data[4] << 16);
        if (temp_raw & 0x800000) temp_raw |= 0xFF000000;
        
        // Temperature in 0.1°C
        int32_t temp_c = temp_raw / 10;
        
        Serial.println("=== IVT-S TEMP ===");
        Serial.printf("Raw: %d, Scaled: %d °C\n", temp_raw, temp_c);
        Serial.println("==================");
        
        updateParameterIfExists(14, temp_c);  // ID 14 = Shunt temperature
        return;
    }
    
    // CAN ID 0x356 (854): ZombieVerter broadcasts udc, idc, tmpm
    // But ISA shunt also uses this ID, so we get a conflict
    // We'll parse motor temp (tmpm) from bytes 4-5 if it looks valid
    if (msg.id == 0x356 && msg.length >= 6) {
        int16_t tmpm_raw = msg.data[4] | (msg.data[5] << 8);
        
        // Motor temp should be reasonable (0-100°C typically)
        // The raw value we're seeing is around 24 (0x18)
        if (tmpm_raw >= 0 && tmpm_raw <= 150) {
            Serial.printf("0x356: tmpm=%d°C (motor temp from ZombieVerter)\n", tmpm_raw);
            updateParameterIfExists(5, tmpm_raw);  // ID 5 = Motor Temp
        }
        return;
    }
    
    // CAN ID 0x373 (883): Victron/REC BMS - Min/Max cell voltages and temps
    // Format: [MinVolt_L, MinVolt_H, MaxVolt_L, MaxVolt_H, MaxTemp_L, MaxTemp_H, ?, ?]
    // Voltages in mV, temps in 0.1°C
    if (msg.id == 0x373 && msg.length >= 6) {
        uint16_t minVolt = msg.data[0] | (msg.data[1] << 8);  // mV
        uint16_t maxVolt = msg.data[2] | (msg.data[3] << 8);  // mV
        uint16_t maxTemp = msg.data[4] | (msg.data[5] << 8);  // 0.1°C
        
        // Convert temp from 0.1°C to °C
        int16_t maxTempC = maxTemp / 10;
        
        #if DEBUG_CAN
        Serial.println("=== VICTRON BMS 0x373 ===");
        Serial.printf("Min Cell: %d mV (%.3f V)\n", minVolt, minVolt/1000.0);
        Serial.printf("Max Cell: %d mV (%.3f V)\n", maxVolt, maxVolt/1000.0);
        Serial.printf("Max Temp: %d C\n", maxTempC);
        Serial.println("=========================");
        #endif
        
        updateParameterIfExists(20, maxVolt);   // Max cell voltage in mV (param 20)
        updateParameterIfExists(21, minVolt);   // Min cell voltage in mV (param 21)
        updateParameterIfExists(24, maxTempC);  // Max cell temp in °C (param 24)
        return;
    }
    
    // If we got here, it's not an IVT-S message we recognize
    // Try old generic parsing for 0x5XX messages (for other devices)
    if (msg.id >= 0x500 && msg.id <= 0x5FF && msg.length == 6) {
        Serial.printf("Unknown 0x5XX message: ID=0x%03X\n", msg.id);
        return;
    }
    
    // CAN ID 0x356 (854): Ignore - this is being overwritten by ISA shunt
    if (msg.id == 0x356) {
        // Just ignore it
        return;
    }
    
    // CAN ID 0x355 (853): SOC (bytes 0-1)
    if (msg.id == 0x355 && msg.length >= 2) {
        int16_t soc_raw = msg.data[0] | (msg.data[1] << 8);
        
        #if DEBUG_CAN
        Serial.printf("  -> 0x355: SOC=%d%%\n", soc_raw);
        #endif
        
        updateParameterIfExists(7, soc_raw);  // SOC (ID 7)
        return;
    }
    
    // CAN ID 0x126 (294): tmphs at bit 32 (bytes 4-5)
    if (msg.id == 0x126 && msg.length >= 6) {
        int16_t tmphs_raw = msg.data[4] | (msg.data[5] << 8);
        
        #if DEBUG_CAN
        Serial.printf("  -> 0x126: tmphs=%d°C\n", tmphs_raw);
        #endif
        
        updateParameterIfExists(6, tmphs_raw);  // Inverter temp (ID 6)
        return;
    }
    
    // CAN ID 0x257 (599): speed (bytes 0-1)
    if (msg.id == 0x257 && msg.length >= 2) {
        int16_t speed_raw = msg.data[0] | (msg.data[1] << 8);
        
        // Apply gain: 0.09
        int32_t speed = (int32_t)(speed_raw * 0.09);
        
        #if DEBUG_CAN
        Serial.printf("  -> 0x257: speed=%d rpm (raw=%d)\n", speed, speed_raw);
        #endif
        
        updateParameterIfExists(1, speed);  // Speed (ID 1)
        return;
    }
    
    // CAN ID 0x210 (528): U12V at bit 32 (bytes 4-5)
    if (msg.id == 0x210 && msg.length >= 6) {
        int16_t u12v_raw = msg.data[4] | (msg.data[5] << 8);
        
        // Apply gain: 0.09
        int32_t u12v = (int32_t)(u12v_raw * 0.09 * 10);  // In decivolts
        
        #if DEBUG_CAN
        Serial.printf("  -> 0x210: U12V=%d (raw=%d)\n", u12v, u12v_raw);
        #endif
        
        updateParameterIfExists(8, u12v);  // U12V (ID 8 or whichever is free)
        return;
    }
    
    // Fallback: try generic 4x int16 parsing for unknown messages
    if (msg.length >= 8) {
        int16_t val0 = msg.data[0] | (msg.data[1] << 8);
        int16_t val1 = msg.data[2] | (msg.data[3] << 8);
        int16_t val2 = msg.data[4] | (msg.data[5] << 8);
        int16_t val3 = msg.data[6] | (msg.data[7] << 8);
        
        #if DEBUG_CAN
        Serial.printf("  -> Unknown format (4x int16): %d, %d, %d, %d\n", val0, val1, val2, val3);
        #endif
    }
}

// ============================================================================
// BMS Cell Voltage Functions
// ============================================================================

void CANDataManager::handleBMSCellVoltage(CANMessage& msg) {
    // SimpBMS broadcasts cell voltages on sequential CAN IDs
    // Common ranges: 0x400-0x4XX or 0x600-0x6XX
    // Format: 2-byte (16-bit) integers in millivolts per cell
    // Each message typically contains 4 cells (8 bytes = 4 x 16-bit values)
    
    uint8_t baseCell = 0;
    
    // Determine base cell index from CAN ID
    if (msg.id >= 0x400 && msg.id < 0x500) {
        // 0x400 range: each ID has 4 cells
        baseCell = (msg.id - 0x400) * 4;
    } else if (msg.id >= 0x600 && msg.id < 0x700) {
        // 0x600 range: each ID has 4 cells
        baseCell = (msg.id - 0x600) * 4;
    } else {
        return;  // Not a BMS cell voltage message
    }
    
    // Parse up to 4 cell voltages from this message
    uint8_t cellsInMessage = msg.length / 2;  // 2 bytes per cell
    if (cellsInMessage > 4) cellsInMessage = 4;
    
    for (uint8_t i = 0; i < cellsInMessage; i++) {
        uint8_t cellIndex = baseCell + i;
        
        if (cellIndex < MAX_BMS_CELLS) {
            // Read 16-bit voltage in mV (little-endian)
            uint16_t voltage = msg.data[i*2] | (msg.data[i*2 + 1] << 8);
            
            bmsCellVoltages[cellIndex] = voltage;
            bmsCellUpdateTimes[cellIndex] = millis();
            
            // Track highest cell number seen
            if (cellIndex >= bmsCellCount) {
                bmsCellCount = cellIndex + 1;
            }
            
            #if DEBUG_CAN
            Serial.printf("BMS Cell %d: %d mV (%.3f V)\n", cellIndex, voltage, voltage/1000.0);
            #endif
        }
    }
}

uint16_t CANDataManager::getCellVoltage(uint8_t cellIndex) {
    if (cellIndex < MAX_BMS_CELLS) {
        return bmsCellVoltages[cellIndex];
    }
    return 0;
}

uint32_t CANDataManager::getCellLastUpdate(uint8_t cellIndex) {
    if (cellIndex < MAX_BMS_CELLS) {
        return bmsCellUpdateTimes[cellIndex];
    }
    return 0;
}
