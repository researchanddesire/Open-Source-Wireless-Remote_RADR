// Lovense BLE toy emulator.
//
// Advertises as one Lovense model at a time, answers the queries a real toy
// answers (DeviceType;, Battery;, Status:1;) and logs every write it receives
// as one JSON line on USB serial:
//
//   LVSEMU {"event":"write","t":1234,"profile":"lush","ascii":"Vibrate:8;",...}
//
// Serial commands (one per line): list, status, profile <id>, stop.
//
// It changes nothing outside RAM: no Wi-Fi, no filesystem, no NVS writes of
// its own and no bonding.

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/queue.h>

namespace {

constexpr const char* FIRMWARE = "lovense-emulator 2";

struct Profile {
    const char* id;
    const char* model;       // Human name of the emulated toy
    const char* name;        // Advertised BLE name
    const char* service;     // GATT service (also advertised unless hidden)
    const char* tx;          // Host writes commands here
    const char* rx;          // Toy notifies replies here
    const char* deviceType;  // Reply to "DeviceType;" ("" = never reply)
    bool advertiseService;
};

// Service UUIDs and identifiers come from the RADR's bundled Buttplug
// lovense.json, so the RADR treats each profile as that model.
const Profile PROFILES[] = {
    {"lush", "Lush (S, single vibe)", "LVS-Lush",
     "5a300001-0024-4bd4-bbd5-a6920e4c5653", "5a300002-0024-4bd4-bbd5-a6920e4c5653",
     "5a300003-0024-4bd4-bbd5-a6920e4c5653", "S:11:0082059AD3BD;", true},
    {"edge", "Edge (P, dual vibe)", "LVS-Edge",
     "50300001-0024-4bd4-bbd5-a6920e4c5653", "50300002-0024-4bd4-bbd5-a6920e4c5653",
     "50300003-0024-4bd4-bbd5-a6920e4c5653", "P:11:0082059AD3BE;", true},
    {"nora", "Nora (A, vibe + rotate)", "LVS-Nora",
     "4f300001-0023-4bd4-bbd5-a6920e4c5653", "4f300002-0023-4bd4-bbd5-a6920e4c5653",
     "4f300003-0023-4bd4-bbd5-a6920e4c5653", "A:11:0082059AD3BF;", true},
    {"domi", "Domi (W, single vibe)", "LVS-Domi",
     "57300001-0023-4bd4-bbd5-a6920e4c5653", "57300002-0023-4bd4-bbd5-a6920e4c5653",
     "57300003-0023-4bd4-bbd5-a6920e4c5653", "W:11:0082059AD3C0;", true},
    {"max", "Max (B, vibe + constrict)", "LVS-Max",
     "58300001-0023-4bd4-bbd5-a6920e4c5653", "58300002-0023-4bd4-bbd5-a6920e4c5653",
     "58300003-0023-4bd4-bbd5-a6920e4c5653", "B:11:0082059AD3C1;", true},
    {"hush1", "Hush gen 1 (Z, Nordic UART service)", "LVS-Hush",
     "6e400001-b5a3-f393-e0a9-e50e24dcca9e", "6e400002-b5a3-f393-e0a9-e50e24dcca9e",
     "6e400003-b5a3-f393-e0a9-e50e24dcca9e", "Z:11:0082059AD3C2;", true},
    {"gush-hidden", "Gush (ED) advertising its name only, no service UUID",
     "LVS-Gush", "53440001-0023-4bd4-bbd5-a6920e4c5653",
     "53440002-0023-4bd4-bbd5-a6920e4c5653", "53440003-0023-4bd4-bbd5-a6920e4c5653",
     "ED:11:0082059AD3C3;", false},
    {"mute", "Lovense that never answers DeviceType;", "LVS-Mute",
     "53300001-0023-4bd4-bbd5-a6920e4c5653", "53300002-0023-4bd4-bbd5-a6920e4c5653",
     "53300003-0023-4bd4-bbd5-a6920e4c5653", "", true},
};
constexpr size_t PROFILE_COUNT = sizeof(PROFILES) / sizeof(PROFILES[0]);

struct Endpoint {
    NimBLECharacteristic* tx = nullptr;
    NimBLECharacteristic* rx = nullptr;
};
Endpoint endpoints[PROFILE_COUNT];

NimBLEServer* server = nullptr;
size_t active = 0;
volatile bool connected = false;
volatile uint16_t connHandle = 0;
volatile uint32_t writes = 0;
uint32_t battery = 85;

struct Reply {
    size_t profile;
    char text[40];
};
QueueHandle_t replies = nullptr;

// Serial output from several tasks must not interleave mid-line.
SemaphoreHandle_t serialLock = nullptr;

void emit(const String& json) {
    xSemaphoreTake(serialLock, portMAX_DELAY);
    Serial.print("\nLVSEMU ");
    Serial.println(json);
    xSemaphoreGive(serialLock);
}

String jsonEscape(const std::string& value) {
    String out;
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c >= 32 && c < 127) {
            out += static_cast<char>(c);
        } else {
            char buffer[8];
            snprintf(buffer, sizeof(buffer), "\\u%04x", c);
            out += buffer;
        }
    }
    return out;
}

String hex(const std::string& value) {
    String out;
    char buffer[3];
    for (unsigned char c : value) {
        snprintf(buffer, sizeof(buffer), "%02x", c);
        out += buffer;
    }
    return out;
}

String head(const char* event) {
    return String("{\"event\":\"") + event + "\",\"t\":" + String(millis()) +
           ",\"profile\":\"" + PROFILES[active].id + "\"";
}

// What a real Lovense toy answers. Commands are acknowledged with "OK;".
String answer(const Profile& profile, const std::string& command) {
    if (command == "DeviceType;") return profile.deviceType;
    if (command == "Battery;") return String(battery) + ";";
    if (command == "Status:1;") return "2;";
    if (command == "GetBatch;") return "0;";
    if (command.rfind("Vibrate", 0) == 0 || command.rfind("Rotate", 0) == 0 ||
        command.rfind("Air:", 0) == 0 || command.rfind("Mply:", 0) == 0 ||
        command.rfind("PowerOff", 0) == 0 || command.rfind("RotateChange", 0) == 0)
        return "OK;";
    return "ERR;";
}

class TxCallbacks : public NimBLECharacteristicCallbacks {
  public:
    explicit TxCallbacks(size_t index) : index(index) {}

    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        const std::string value = characteristic->getValue();
        writes = writes + 1;
        const Profile& profile = PROFILES[index];
        emit(head("write") + ",\"seq\":" + String(writes) + ",\"service\":\"" +
             profile.service + "\",\"char\":\"" + profile.tx + "\",\"len\":" +
             String(value.size()) + ",\"hex\":\"" + hex(value) + "\",\"ascii\":\"" +
             jsonEscape(value) + "\"}");
        // A host may batch several commands in one write ("Vibrate1:3;Vibrate2:5;").
        size_t start = 0;
        while (start < value.size()) {
            size_t end = value.find(';', start);
            if (end == std::string::npos) end = value.size() - 1;
            const std::string command = value.substr(start, end - start + 1);
            start = end + 1;
            const String text = answer(profile, command);
            if (text.isEmpty()) continue;
            Reply reply{index, {}};
            strlcpy(reply.text, text.c_str(), sizeof(reply.text));
            xQueueSend(replies, &reply, 0);
        }
    }

  private:
    size_t index;
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
  public:
    explicit RxCallbacks(size_t index) : index(index) {}

    void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t value) override {
        emit(head("subscribe") + ",\"char\":\"" + PROFILES[index].rx +
             "\",\"value\":" + String(value) + "}");
    }

  private:
    size_t index;
};

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer*, NimBLEConnInfo& info) override {
        connected = true;
        connHandle = info.getConnHandle();
        emit(head("connect") + ",\"peer\":\"" +
             String(info.getAddress().toString().c_str()) + "\"}");
    }

    void onDisconnect(NimBLEServer*, NimBLEConnInfo& info, int reason) override {
        connected = false;
        emit(head("disconnect") + ",\"reason\":" + String(reason) + "}");
        NimBLEDevice::startAdvertising();
    }
} serverCallbacks;

void replyTask(void*) {
    Reply reply;
    for (;;) {
        if (xQueueReceive(replies, &reply, portMAX_DELAY) != pdTRUE) continue;
        vTaskDelay(pdMS_TO_TICKS(15));  // A real toy answers after a few ms.
        auto* rx = endpoints[reply.profile].rx;
        rx->setValue(reinterpret_cast<const uint8_t*>(reply.text), strlen(reply.text));
        const bool sent = rx->notify();
        emit(head("notify") + ",\"char\":\"" + PROFILES[reply.profile].rx +
             "\",\"ascii\":\"" + jsonEscape(reply.text) + "\",\"sent\":" +
             (sent ? "true" : "false") + "}");
    }
}

void advertise(size_t index) {
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->stop();
    if (connected) {
        server->disconnect(connHandle);
        delay(300);
    }
    active = index;
    const Profile& profile = PROFILES[index];
    NimBLEDevice::setDeviceName(profile.name);

    NimBLEAdvertisementData data;
    data.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    data.setName(profile.name);
    if (profile.advertiseService) data.setCompleteServices(NimBLEUUID(profile.service));
    advertising->setAdvertisementData(data);

    // Lovense manufacturer data (company 0x026C) goes in the scan response.
    NimBLEAdvertisementData response;
    const uint8_t manufacturer[] = {0x6c, 0x02, 0xff, 0x21};
    response.setManufacturerData(std::string(reinterpret_cast<const char*>(manufacturer),
                                             sizeof(manufacturer)));
    advertising->setScanResponseData(response);
    advertising->start();
    emit(head("advertising") + ",\"model\":\"" + profile.model + "\",\"name\":\"" +
         profile.name + "\",\"service\":\"" + profile.service +
         "\",\"serviceAdvertised\":" + (profile.advertiseService ? "true" : "false") + "}");
}

void list() {
    for (size_t i = 0; i < PROFILE_COUNT; ++i)
        emit(String("{\"event\":\"profile\",\"id\":\"") + PROFILES[i].id + "\",\"model\":\"" +
             PROFILES[i].model + "\",\"name\":\"" + PROFILES[i].name + "\",\"service\":\"" +
             PROFILES[i].service + "\",\"deviceType\":\"" + PROFILES[i].deviceType + "\"}");
}

void status() {
    emit(head("status") + ",\"firmware\":\"" + FIRMWARE + "\",\"connected\":" +
         (connected ? "true" : "false") + ",\"writes\":" + String(writes) + "}");
}

void command(String line) {
    line.trim();
    if (line == "list") return list();
    if (line == "status") return status();
    if (line == "stop") {
        NimBLEDevice::getAdvertising()->stop();
        if (connected) server->disconnect(connHandle);
        return emit(head("stopped") + "}");
    }
    if (line.startsWith("battery ")) {
        battery = line.substring(8).toInt();
        return status();
    }
    if (line.startsWith("profile ")) {
        const String id = line.substring(8);
        for (size_t i = 0; i < PROFILE_COUNT; ++i)
            if (id == PROFILES[i].id) return advertise(i);
        return emit(String("{\"event\":\"error\",\"message\":\"unknown profile ") + id + "\"}");
    }
    if (!line.isEmpty())
        emit(String("{\"event\":\"error\",\"message\":\"unknown command\"}"));
}

}  // namespace

void setup() {
    Serial.begin(115200);
    serialLock = xSemaphoreCreateMutex();
    replies = xQueueCreate(16, sizeof(Reply));
    delay(500);

    NimBLEDevice::init(PROFILES[0].name);
    // Advertise from a random static address, never the board's own. A host
    // that already knows the board (a PC it was provisioned from, a phone it
    // is bonded to) would otherwise reconnect by itself and block the test.
    const NimBLEAddress address("f2:4c:56:53:00:01", BLE_ADDR_RANDOM);
    NimBLEDevice::setOwnAddr(address);
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
    NimBLEDevice::setSecurityAuth(false, false, false);
    NimBLEDevice::setPower(9);
    server = NimBLEDevice::createServer();
    server->setCallbacks(&serverCallbacks, false);
    static std::vector<NimBLECharacteristicCallbacks*> callbacks;
    for (size_t i = 0; i < PROFILE_COUNT; ++i) {
        NimBLEService* service = server->createService(PROFILES[i].service);
        endpoints[i].tx = service->createCharacteristic(
            PROFILES[i].tx, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
        endpoints[i].rx = service->createCharacteristic(
            PROFILES[i].rx, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
        auto* tx = new TxCallbacks(i);
        auto* rx = new RxCallbacks(i);
        callbacks.push_back(tx);
        callbacks.push_back(rx);
        endpoints[i].tx->setCallbacks(tx);
        endpoints[i].rx->setCallbacks(rx);
        service->start();
    }
    server->start();
    xTaskCreate(replyTask, "replies", 4096, nullptr, 2, nullptr);
    emit(String("{\"event\":\"boot\",\"firmware\":\"") + FIRMWARE + "\",\"t\":" +
         String(millis()) + ",\"address\":\"" +
         String(NimBLEDevice::getAddress().toString().c_str()) + "\"}");
    list();
    advertise(0);
}

void loop() {
    static String line;
    static uint32_t lastBeat = 0;
    while (Serial.available()) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n') {
            command(line);
            line = "";
        } else if (c != '\r' && line.length() < 80) {
            line += c;
        }
    }
    if (millis() - lastBeat >= 5000) {
        lastBeat = millis();
        status();
    }
    delay(10);
}
