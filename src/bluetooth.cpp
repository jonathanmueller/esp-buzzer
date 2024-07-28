#include "bluetooth.h"
#include "_config.h"
#include "battery.h"
#include "comm.h"
#include "esp32-hal-log.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLE2904.h>

#include <FastLED.h>

// Generated using https://www.uuidgenerator.net/
#define UUID_SERVICE                     "20d86bb5-f515-4671-8a88-32fddb20920c"
#define UUID_CHARACTERISTIC_VERSION      "4d3c98dc-2970-496a-bc20-c1295abc9730"
#define UUID_CHARACTERISTIC_EXEC_COMMAND "d384392d-e53e-4c21-a598-f7bf8ccfcb66"
#define UUID_CHARACTERISTIC_PEER_LIST    "f7551fb0-05c3-4dff-a944-4980f40779e1"

#define UUID_SERVICE_BATTERY             "180f"
#define UUID_CHARACTERISTIC_BATTERY      "2a19"

BLEServer *btServer;
BLEService *pService;
BLEAdvertising *pAdvertising;

BLECharacteristic *characteristicVersion;
BLECharacteristic *characteristicExecCommand;
BLECharacteristic *characteristicPeerList;
BLECharacteristic *characteristicBattery;

void bluetooth_loop();
static void bt_task(void *pvParameter) {
    while (true) {
        bluetooth_loop();
    }

    vTaskDelete(NULL);
}

void bluetooth_notify_peer_list_changed() {
    if (characteristicPeerList != nullptr) {
        characteristicPeerList->notify();
    }
}

static uint8_t connected_clients = 0;

class BTServerCallbacks : public BLEServerCallbacks {
  public:
    void onConnect(BLEServer *pServer) {
        connected_clients++;
        BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer *pServer) {
        if (connected_clients > 0) {
            connected_clients--;
        }
    }
};

class BTExecCommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic, esp_ble_gatts_cb_param_t *param) {
        log_d("Received command via bluetooth (%d bytes)...", param->write.len);

        if (param->write.len > 6) {
            struct {
                uint8_t dst_mac_addr[6];
                payload_command_t command;
            } __attribute__((packed)) *value = (decltype(value))param->write.value;

            executeCommand(value->dst_mac_addr, &value->command, param->write.len - sizeof(value->dst_mac_addr));
        }
    }
};
class BTPeerListCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *pCharacteristic, esp_ble_gatts_cb_param_t *param) {
        update_my_info();
        pCharacteristic->setValue((uint8_t *)peer_data_table, sizeof(peer_data_table));
    }
};

static const uint8_t _VERSION_CODE = VERSION_CODE;

inline BLEDescriptor *userDescription(const std::string &description) {
    BLEDescriptor *d = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    d->setValue(description);
    return d;
}

inline BLEDescriptor *formatDescriptor(uint8_t format) {
    BLE2904 *d = new BLE2904();
    d->setFormat(format);
    return d;
}

void bluetooth_init() {
    BLEDevice::init("Buzzer Controller");

    btServer = BLEDevice::createServer();
    btServer->setCallbacks(new BTServerCallbacks());

    pService = btServer->createService(UUID_SERVICE);

    characteristicVersion = pService->createCharacteristic(UUID_CHARACTERISTIC_VERSION, BLECharacteristic::PROPERTY_READ);
    characteristicVersion->addDescriptor(userDescription("Communication Version"));
    characteristicVersion->addDescriptor(formatDescriptor(BLE2904::FORMAT_UINT8));
    characteristicVersion->setValue((uint8_t *)&_VERSION_CODE, 1);

    characteristicExecCommand = pService->createCharacteristic(UUID_CHARACTERISTIC_EXEC_COMMAND, BLECharacteristic::PROPERTY_WRITE);
    characteristicExecCommand->addDescriptor(userDescription("Execute Command"));
    characteristicExecCommand->addDescriptor(formatDescriptor(BLE2904::FORMAT_OPAQUE));
    characteristicExecCommand->setCallbacks(new BTExecCommandCallbacks());

    characteristicPeerList = pService->createCharacteristic(UUID_CHARACTERISTIC_PEER_LIST, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    characteristicPeerList->addDescriptor(userDescription("Peer List"));
    characteristicPeerList->addDescriptor(formatDescriptor(BLE2904::FORMAT_OPAQUE));
    characteristicPeerList->setCallbacks(new BTPeerListCallbacks());

    pService->start();

    BLEService *batService = btServer->createService(UUID_SERVICE_BATTERY);
    characteristicBattery  = batService->createCharacteristic(UUID_CHARACTERISTIC_BATTERY, BLECharacteristic::PROPERTY_READ);
    characteristicBattery->addDescriptor(userDescription("Battery Level"));
    characteristicBattery->addDescriptor(formatDescriptor(BLE2904::FORMAT_UINT8));
    characteristicBattery->setValue((uint8_t *)&battery_percent_rounded, 1);
    batService->start();

    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(UUID_SERVICE);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);

    BLEDevice::startAdvertising();

    xTaskCreate(&bt_task, "bt_task", 2400, NULL, 5, NULL);
}

void bluetooth_loop() {
    // EVERY_N_SECONDS(10) {
    //     pCharacteristic->setValue("lalala");
    //     pCharacteristic->notify();
    // }

    if (connected_clients > 0) {
        reset_shutdown_timer();
    }
}
