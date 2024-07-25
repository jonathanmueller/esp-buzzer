#include "bluetooth.h"
#include "esp32-hal-log.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <FastLED.h>

// Generated using https://www.uuidgenerator.net/
#define SERVICE_UUID        "20d86bb5-f515-4671-8a88-32fddb20920c"
#define CHARACTERISTIC_UUID "4d3c98dc-2970-496a-bc20-c1295abc9730"

BLEServer *btServer;
BLEService *pService;
BLECharacteristic *pCharacteristic;
BLEAdvertising *pAdvertising;

void bluetooth_loop();
static void bt_task(void *pvParameter) {
    while (true) {
        bluetooth_loop();
    }

    vTaskDelete(NULL);
}

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        // deviceConnected = true;
        BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer *pServer) {
        // deviceConnected = false;
    }
};
void bluetooth_init() {
    BLEDevice::init("Buzzer Controller");

    btServer = BLEDevice::createServer();
    btServer->setCallbacks(new MyServerCallbacks());
    pService = btServer->createService(SERVICE_UUID);

    pCharacteristic =
        pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);

    pCharacteristic->setValue("Hello World");
    // pCharacteristic->addDescriptor(new BLE2902());
    pService->start();
    // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    log_i("Characteristic defined!");

    xTaskCreate(&bt_task, "bt_task", 2400, NULL, 5, NULL);
}

void bluetooth_loop() {
    // EVERY_N_SECONDS(10) {
    //     pCharacteristic->setValue("lalala");
    //     pCharacteristic->notify();
    // }
}
