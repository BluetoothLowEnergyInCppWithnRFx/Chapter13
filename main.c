#include "mbed.h"
#include "ble/BLE.h"

/** User interface I/O **/

// instantiate USB Serial
Serial serial(USBTX, USBRX);

// Status LED
DigitalOut statusLed(LED1, 0);

/** Bluetooth Peripheral Properties **/

// Broadcast name
const static char BROADCAST_NAME[] = "RemoteLed";

// Automation IO Service UUID
static const uint16_t customServiceUuid  = 0x1815;

// array of all Service UUIDs
static const uint16_t uuid16_list[] = { customServiceUuid };

// Number of bytes in Characteristic
static const uint8_t characteristicLength = 2;

// Write commands to this Characteristic
uint16_t commandCharacteristicUuid = 0x2A56;

// Respond to connected Central from this Characteristic
uint16_t responseCharacteristicUuid = 0x2A57;

/** Commands **/

static const uint8_t bleCommandFooterPosition = 1;
static const uint8_t bleCommandDataPosition = 0;

static const uint8_t bleCommandFooter = 1;

static const uint8_t bleCommandLedOn = 1;
static const uint8_t bleCommandLedOff = 2;

/** Response **/
static const uint8_t bleResponseFooterPosition = 1;
static const uint8_t bleResponseDataPosition = 0;

static const uint8_t bleResponseErrorFooter = 0;
static const uint8_t bleResponseConfirmationFooter = 1;

static const uint8_t bleResponseLedError = 0;
static const uint8_t bleResponseLedOn = 1;
static const uint8_t bleResponseLedOff = 2;

/** State **/

// flag when Central has written to a Characteristic
bool bleDataWritten = false; // true if data has been written to the characteristic

// incoming command
uint8_t bleCommandValue[characteristicLength] = {0};

/** Functions **/

/**
 * Callback triggered when the ble initialization process has finished
 *
 * @param[in] params Information about the initialized Peripheral
 */
void onBluetoothInitialized(BLE::InitializationCompleteCallbackContext *params);

/**
 * This callback allows the LEDService to receive updates to the ledState Characteristic.
 *
 * @param[in] params
 *     Information about the characterisitc being updated.
 */
void onDataWrittenCallback(const GattWriteCallbackParams *params);

/**
 * Notify connected Central of changed LED state
 *
 * @param[in] ledState
 *      The LED state
 */
void sendBleResponse(uint8_t ledState);

/**
 * Callback handler when a Central has disconnected
 * 
 * @param[i] params Information about the connection
 */
void onCentralDisconnected(const Gap::DisconnectionCallbackParams_t *params);


/** Build Service and Characteristic Relationships **/
// Set Up custom Characteristics
static char commandValue[characteristicLength] = {0};
WriteOnlyArrayGattCharacteristic<uint8_t, sizeof(commandValue)> commandCharacteristic(
    commandCharacteristicUuid, 
    (uint8_t *)commandValue,
    GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE);
    
static char responseValue[characteristicLength] = {0};
ReadOnlyArrayGattCharacteristic<uint8_t, sizeof(responseValue)> responseCharacteristic(
    responseCharacteristicUuid, 
    (uint8_t *)responseValue,
    GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY);

// Set up custom service
GattCharacteristic *characteristics[] = {&commandCharacteristic, &responseCharacteristic};
GattService        customService(customServiceUuid, characteristics, sizeof(characteristics) / sizeof(GattCharacteristic *));

/**
 * Main program and loop
 */
int main(void) {
    serial.baud(9600);
    serial.printf("Starting LedRemote\r\n");

    // initialized Bluetooth Radio
    BLE &ble = BLE::Instance(BLE::DEFAULT_INSTANCE);
    ble.init(onBluetoothInitialized);

    // wait for Bluetooth Radio to be initialized
    while (ble.hasInitialized()  == false);

    while (1) {
        if (bleDataWritten) {
            bleDataWritten = false; // ensure only happens once
            // do something with the bleCharacteristicValue
            serial.printf("responding to command\r\n");
            for (int i=0; i<characteristicLength; i++) {
              serial.printf("0x%x ", bleCommandValue[i]);   
            }
            serial.printf("\r\n");
            
            if (bleCommandValue[bleCommandFooterPosition] == bleCommandFooter) {
                serial.printf("command in footer\r\n");
                switch (bleCommandValue[bleCommandDataPosition]) {
                    case bleCommandLedOn:
                        serial.printf("Led on\r\n");
                        statusLed.write(0);
                        sendBleResponse(bleResponseLedOn);
                    break;
                    case bleCommandLedOff:
                        serial.printf("led off\r\n");
                        statusLed.write(1);
                        sendBleResponse(bleResponseLedOff);
                    break;
                    default:
                        // handle unknown condition
                        serial.printf("Unknown command\r\n");
                }   
            }
        }
        ble.waitForEvent();
    }
}


void onBluetoothInitialized(BLE::InitializationCompleteCallbackContext *params) {
    BLE&        ble   = params->ble;
    ble_error_t error = params->error;

    // quit if there's a problem
    if (error != BLE_ERROR_NONE) {
        return;
    }

    // Ensure that it is the default instance of BLE 
    if(ble.getInstanceID() != BLE::DEFAULT_INSTANCE) {
        return;
    }

    serial.printf("Describing Peripheral...");
    
    // attach Services
    ble.addService(customService);
    
    // if a Central writes tho a Characteristic, handle event with a callback
    ble.gattServer().onDataWritten(onDataWrittenCallback);
 
    // process disconnections with a callback
    ble.gap().onDisconnection(onCentralDisconnected);

    // advertising parametirs
    ble.gap().accumulateAdvertisingPayload(
        GapAdvertisingData::BREDR_NOT_SUPPORTED |   // Device is Peripheral only
        GapAdvertisingData::LE_GENERAL_DISCOVERABLE); // always discoverable
    // broadcast name
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME, (uint8_t *)BROADCAST_NAME, sizeof(BROADCAST_NAME));
    //  advertise services
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, (uint8_t *)uuid16_list, sizeof(uuid16_list));
    // allow connections
    ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    // advertise every 1000ms
    ble.gap().setAdvertisingInterval(1000); // 1000ms
    // begin advertising
    ble.gap().startAdvertising();

    serial.printf(" done\r\n");
}

void onDataWrittenCallback(const GattWriteCallbackParams *params) {
    serial.printf("command written");
    if (params->handle == commandCharacteristic.getValueHandle()) {
        bleDataWritten = true;
        memcpy(bleCommandValue, (char*) params->data, sizeof(bleCommandValue) * params->len);
    }
}

void sendBleResponse(uint8_t ledState) {
    serial.printf("writing response\r\n");
    uint8_t responseValue[characteristicLength];
    responseValue[bleResponseFooterPosition] = bleResponseConfirmationFooter;
    responseValue[bleResponseDataPosition] = ledState;
    BLE::Instance(BLE::DEFAULT_INSTANCE).gattServer().write(
        responseCharacteristic.getValueHandle(), 
        (const uint8_t *)responseValue,  
        characteristicLength);
}

void onCentralDisconnected(const Gap::DisconnectionCallbackParams_t *params) {
    BLE::Instance().gap().startAdvertising();
    serial.printf("Central disconnected\r\n");
}