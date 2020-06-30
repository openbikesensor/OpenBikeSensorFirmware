#include "ble.h"

// https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml

BLEServer *pServer;
BLEService *pDeviceInfoService;
BLEService *pBatteryLevelService;
BLEService *pConnectionService;
BLEService *pHeartRateService;
BLEService *pDistanceService;
BLEService *pClosePassService;

BLECharacteristic *pConnectionCharacteristic;
BLECharacteristic *pHeartRateCharacteristic;
BLECharacteristic *pDistanceCharacteristic;
BLECharacteristic *pClosePassCharacteristic;

void initBluetooth() {
    // Create the BLE Device & Server
    BLEDevice::init("Radmesser");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new CustomBTCallback());

    // Create the BLE Services
    pDeviceInfoService= createDeviceInfoService();
    pBatteryLevelService = createBatteryLevelService();
    pConnectionService = createConnectionService();
    pHeartRateService = createHeartRateService();
    pDistanceService = createDistanceService();
    pClosePassService = createClosePassService();

    // Start the service
    pDeviceInfoService->start();
    pBatteryLevelService->start();
    pConnectionService->start();
    pHeartRateService->start();
    pDistanceService->start();
    pClosePassService->start();
}

void activateBluetooth() {
    // Start advertising
    pServer->getAdvertising()->addServiceUUID(pConnectionService->getUUID());
    pServer->getAdvertising()->addServiceUUID(pHeartRateService->getUUID());
    pServer->getAdvertising()->addServiceUUID(pDistanceService->getUUID());
    pServer->getAdvertising()->addServiceUUID(pClosePassService->getUUID());
    pServer->getAdvertising()->start();

    gConnectionStatus = CONN_READY;
}

void deactivateBluetooth() {
    pServer->getAdvertising()->stop();
    gConnectionStatus = CONN_OFF;
}

void disconnectDevice() {
    pServer->disconnect(pServer->getConnId());
}

void writeToConnectionService(uint8_t value) {
//    uint8_t data[1] = {value};

    pConnectionCharacteristic->setValue(String(value).c_str());
    pConnectionCharacteristic->notify();
}

void writeToHeartRateService(uint8_t *data, size_t length) {
    pHeartRateCharacteristic->setValue(data, length);
    pHeartRateCharacteristic->notify();
}

void writeToDistanceService(const char *value) {
    pDistanceCharacteristic->setValue(value);
    pDistanceCharacteristic->notify();
}

void writeToClosePassService(const char *value) {
    pClosePassCharacteristic->setValue(value);
    pClosePassCharacteristic->notify();
}

BLEService* createDeviceInfoService() {
    BLEService *service = pServer->createService(SERVICE_DEVICE_UUID);

    BLECharacteristic *pSystemIDCharcateristic = service->createCharacteristic(SERVICE_DEVICE_CHAR_SYSTEMID_UUID, BLECharacteristic::PROPERTY_READ);
    uint8_t systemIDValueHex[SERVICE_DEVICE_CHAR_SYSTEMID_VALUE_HEX_LEN] = SERVICE_DEVICE_CHAR_SYSTEMID_VALUE_HEX;
    pSystemIDCharcateristic->setValue(systemIDValueHex, SERVICE_DEVICE_CHAR_SYSTEMID_VALUE_HEX_LEN);

    BLECharacteristic *pModelNumberStringCharacteristic = service->createCharacteristic(SERVICE_DEVICE_CHAR_MODELNUMBER_STRING_UUID, BLECharacteristic::PROPERTY_READ);
    pModelNumberStringCharacteristic->setValue(SERVICE_DEVICE_CHAR_MODELNUMBER_STRING_VALUE);

    BLECharacteristic *pSerialNumberStringCharacteristic = service->createCharacteristic(SERVICE_DEVICE_CHAR_SERIALNUMBER_STRING_UUID, BLECharacteristic::PROPERTY_READ);
    pSerialNumberStringCharacteristic->setValue(SERVICE_DEVICE_CHAR_SERIALNUMBER_STRING_VALUE);

    BLECharacteristic *pFirmwareRevisionCharacteristic = service->createCharacteristic(SERVICE_DEVICE_CHAR_FIRMWAREREVISON_UUID, BLECharacteristic::PROPERTY_READ);
    pFirmwareRevisionCharacteristic->setValue(SERVICE_DEVICE_CHAR_FIRMWAREREVISON_VALUE);

    BLECharacteristic *pHardwareRevisionCharacteristic = service->createCharacteristic(SERVICE_DEVICE_CHAR_HARDWAREREVISION_UUID, BLECharacteristic::PROPERTY_READ);
    pHardwareRevisionCharacteristic->setValue(SERVICE_DEVICE_CHAR_HARDWAREREVISION_VALUE);

    BLECharacteristic *pSoftwareRevisionCharacteristic = service->createCharacteristic(SERVICE_DEVICE_CHAR_SOFTWAREREVISION_UUID, BLECharacteristic::PROPERTY_READ);
    pSoftwareRevisionCharacteristic->setValue(SERVICE_DEVICE_CHAR_SOFTWAREREVISION_VALUE);

    BLECharacteristic *pManufacturerNameCharacteristic = service->createCharacteristic(SERVICE_DEVICE_CHAR_MANUFACTURERNAME_STRING_UUID, BLECharacteristic::PROPERTY_READ);
    pManufacturerNameCharacteristic->setValue(SERVICE_DEVICE_CHAR_MANUFACTURERNAME_STRING_VALUE);

    return service;
}

BLEService *createBatteryLevelService() {
    BLEService *service = pServer->createService(SERVICE_BATTERY_UUID);

    BLECharacteristic *pBatteryCharacteristics = service->createCharacteristic(SERVICE_BATTERY_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    pBatteryCharacteristics->setValue("90");

    return service;
}

BLEService *createConnectionService() {
    BLEService *service = pServer->createService(SERVICE_CONNECTION_UUID);

    pConnectionCharacteristic = service->createCharacteristic(SERVICE_CONNECTION_CHAR_CONNECTED_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
//    uint8_t value = SERVICE_CONNECTION_CHAR_CONNECTED_VALUE;
//    pConnectionCharacteristic->setValue(&value, 1);

    return service;
}

BLEService *createHeartRateService() {
    BLEService *service = pServer->createService(SERVICE_HEARTRATE_UUID);

    pHeartRateCharacteristic = service->createCharacteristic(SERVICE_HEARTRATE_CHAR_HEARTRATE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

    auto *pDescriptor = new BLEDescriptor(SERVICE_HEARTRATE_DESCRIPTOR_UUID);
    pHeartRateCharacteristic->addDescriptor(pDescriptor);
    uint8_t descriptorbuffer;
    descriptorbuffer = 1;
    pDescriptor->setValue(&descriptorbuffer, 1);

    BLECharacteristic *pSensorLocationCharacteristic = service->createCharacteristic(SERVICE_HEARTRATE_CHAR_SENSORLOCATION_UUID, BLECharacteristic::PROPERTY_READ);
    uint8_t locationValue = SERVICE_HEARTRATE_CHAR_SENSORLOCATION_VALUE;
    pSensorLocationCharacteristic->setValue(&locationValue, 1);

    return service;
}

BLEService* createDistanceService() {
    BLEService *service = pServer->createService(SERVICE_DISTANCE_UUID);

    pDistanceCharacteristic = service->createCharacteristic(SERVICE_DISTANCE_CHAR_50MS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

    return service;
}

BLEService* createClosePassService() {
    BLEService *service = pServer->createService(SERVICE_CLOSEPASS_UUID);

    pClosePassCharacteristic = service->createCharacteristic(SERVICE_CLOSEPASS_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

    return service;
}
