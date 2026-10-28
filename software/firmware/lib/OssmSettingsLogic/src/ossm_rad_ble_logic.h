#pragma once

#include <ArduinoJson.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

// RAD BLE v1 request building, reply parsing and OSSM machine-geometry
// validation. No hardware dependencies, so it is tested natively.
namespace ossm_rad_ble {

constexpr uint32_t kRequestTimeoutMs = 2000;
constexpr uint32_t kLeaseTtlSeconds = 5;

// ---------------------------------------------------------------------------
// Requests and replies
// ---------------------------------------------------------------------------

/// Builds {"v":1,"id":N,"op":...,"path":...,"args":{...},"lease":T}.
/// Empty path/args and a zero lease are omitted.
inline std::string buildRequest(uint32_t id, const char *op, const char *path,
                                const std::string &argsJson, uint32_t lease) {
    JsonDocument document;
    document["v"] = 1;
    document["id"] = id;
    document["op"] = op;
    if (path != nullptr && path[0] != '\0') document["path"] = path;
    if (!argsJson.empty()) document["args"] = serialized(argsJson);
    if (lease != 0) document["lease"] = lease;
    std::string output;
    serializeJson(document, output);
    return output;
}

inline std::string valueArgs(float value) {
    JsonDocument document;
    document["value"] = value;
    std::string output;
    serializeJson(document, output);
    return output;
}

inline std::string ttlArgs(uint32_t ttlSeconds) {
    return "{\"ttl\":" + std::to_string(ttlSeconds) + "}";
}

enum class ReplyKind {
    Ignore,    // not JSON, another id, or a non-terminal progress stage
    Accepted,  // the request was queued; keep waiting
    Terminal,  // completed or failed
};

struct Reply {
    bool ok = false;
    std::string code;
    std::string message;
    std::string resultJson;
};

inline Reply failedReply(const char *code, const char *message) {
    Reply reply;
    reply.code = code;
    reply.message = message;
    return reply;
}

inline ReplyKind parseReply(const char *data, size_t length,
                            uint32_t expectedId, Reply &out) {
    if (data == nullptr || length == 0 || expectedId == 0)
        return ReplyKind::Ignore;
    JsonDocument document;
    if (deserializeJson(document, data, length) != DeserializationError::Ok)
        return ReplyKind::Ignore;
    if (!document["id"].is<uint32_t>() ||
        document["id"].as<uint32_t>() != expectedId)
        return ReplyKind::Ignore;

    const char *stage = document["stage"] | "";
    if (strcmp(stage, "accepted") == 0) return ReplyKind::Accepted;
    if (strcmp(stage, "completed") != 0 && strcmp(stage, "failed") != 0)
        return ReplyKind::Ignore;

    out = Reply();
    out.ok = strcmp(stage, "completed") == 0 && (document["ok"] | false);
    out.code = document["code"] | (out.ok ? "ok" : "failed");
    out.message = document["message"] | "";
    if (!document["result"].isNull())
        serializeJson(document["result"], out.resultJson);
    return ReplyKind::Terminal;
}

inline bool parseLease(const std::string &resultJson, uint32_t &lease) {
    JsonDocument document;
    if (deserializeJson(document, resultJson) != DeserializationError::Ok)
        return false;
    if (!document["lease"].is<uint32_t>()) return false;
    lease = document["lease"].as<uint32_t>();
    return lease != 0;
}

struct SettingRead {
    float value = 0.0f;   // stored in NVS
    float active = 0.0f;  // in use since boot
    bool restartRequired = false;
};

inline bool parseSettingRead(const std::string &resultJson, SettingRead &out) {
    JsonDocument document;
    if (deserializeJson(document, resultJson) != DeserializationError::Ok)
        return false;
    if (!document["value"].is<float>()) return false;
    out.value = document["value"].as<float>();
    out.active = document["active"].is<float>() ? document["active"].as<float>()
                                                : out.value;
    out.restartRequired = document["restartRequired"] | false;
    return true;
}

/// Older OSSM firmware has no response characteristic or no geometry paths.
inline bool meansFirmwareTooOld(const Reply &reply) {
    return reply.code == "unknown_path" || reply.code == "timeout" ||
           reply.code == "unsupported" || reply.code == "unavailable";
}

// ---------------------------------------------------------------------------
// Machine geometry (mirrors OSSM lib/OSSMLogic/src/device_config_logic.h)
// ---------------------------------------------------------------------------

constexpr float kMaxStepRateHz = 100000.0f;
constexpr float kMinStepsPerMm = 2.0f;
constexpr float kMinStrokeLengthMm = 50.0f;
constexpr float kMaxAccelerationMmPerS2 = 50000.0f;

struct MachineConfig {
    float stepsPerRev = 800.0f;
    float pulleyToothCount = 20.0f;
    float beltPitchMm = 2.0f;
    float maxRpm = 1500.0f;
    float maxStrokeMm = 500.0f;
};

enum class GeometryError {
    None,
    NotPositive,
    NotWholeNumber,
    StepsPerMmTooLow,
    StepRateTooHigh,
    StrokeTooShort,
    WouldOverflow,
};

inline float stepsPerMm(const MachineConfig &config) {
    return config.stepsPerRev / (config.pulleyToothCount * config.beltPitchMm);
}

inline float maxStepRateHz(const MachineConfig &config) {
    return config.maxRpm * config.stepsPerRev / 60.0f;
}

inline bool isWholeNumber(float value) {
    return std::isfinite(value) && std::floor(value) == value;
}

inline bool fitsInt32(float steps) {
    return std::isfinite(steps) &&
           std::fabs(steps) <
               static_cast<float>(std::numeric_limits<int32_t>::max());
}

inline GeometryError validateGeometry(const MachineConfig &config) {
    const float fields[] = {config.stepsPerRev, config.pulleyToothCount,
                            config.beltPitchMm, config.maxRpm,
                            config.maxStrokeMm};
    for (float field : fields) {
        if (!std::isfinite(field) || field <= 0.0f)
            return GeometryError::NotPositive;
    }
    if (!isWholeNumber(config.stepsPerRev) ||
        !isWholeNumber(config.pulleyToothCount))
        return GeometryError::NotWholeNumber;
    if (config.maxStrokeMm <= kMinStrokeLengthMm)
        return GeometryError::StrokeTooShort;

    const float perMm = stepsPerMm(config);
    if (!std::isfinite(perMm) || perMm < kMinStepsPerMm)
        return GeometryError::StepsPerMmTooLow;
    if (maxStepRateHz(config) > kMaxStepRateHz)
        return GeometryError::StepRateTooHigh;
    if (!fitsInt32(config.maxStrokeMm * perMm) ||
        !fitsInt32(kMaxAccelerationMmPerS2 * perMm))
        return GeometryError::WouldOverflow;
    return GeometryError::None;
}

/// Same strings the OSSM returns, so the remote and device agree.
inline const char *geometryErrorMessage(GeometryError error) {
    switch (error) {
        case GeometryError::None:
            return "ok";
        case GeometryError::NotPositive:
            return "Machine settings must be positive numbers";
        case GeometryError::NotWholeNumber:
            return "Steps/rev and pulley teeth must be whole numbers";
        case GeometryError::StepsPerMmTooLow:
            return "Steps per mm must be at least 2";
        case GeometryError::StepRateTooHigh:
            return "Max RPM x steps/rev / 60 must be at most 100000 steps/s";
        case GeometryError::StrokeTooShort:
            return "Max stroke must be longer than 50 mm";
        case GeometryError::WouldOverflow:
            return "Steps per mm is too large for the stroke or acceleration "
                   "(max about 42949)";
    }
    return "invalid";
}

// ---------------------------------------------------------------------------
// Editable fields
// ---------------------------------------------------------------------------

constexpr int kFieldCount = 5;

struct FieldSpec {
    const char *path;
    const char *label;
    const char *units;  // empty when unitless
    int decimals;       // shown and edited precision
    int encoderScale;   // encoder counts per unit (10^decimals)
    long encoderMin;
    long encoderMax;
};

// Encoder ranges allow values the OSSM rejects (for example a 40 mm stroke)
// so the confirm screen can show the device's own error text.
constexpr std::array<FieldSpec, kFieldCount> kFields = {{
    {"setting.machine.stepsPerRev", "Steps/rev", "", 0, 1, 1, 100000},
    {"setting.machine.pulleyToothCount", "Pulley teeth", "", 0, 1, 1, 1000},
    {"setting.machine.beltPitchMm", "Belt pitch", "mm", 1, 10, 1, 500},
    {"setting.machine.maxRpm", "Max RPM", "", 0, 1, 1, 20000},
    {"setting.machine.maxStrokeMm", "Max stroke", "mm", 0, 1, 1, 5000},
}};

inline float *field(MachineConfig &config, int index) {
    switch (index) {
        case 0:
            return &config.stepsPerRev;
        case 1:
            return &config.pulleyToothCount;
        case 2:
            return &config.beltPitchMm;
        case 3:
            return &config.maxRpm;
        case 4:
            return &config.maxStrokeMm;
        default:
            return nullptr;
    }
}

inline float fieldValue(const MachineConfig &config, int index) {
    MachineConfig copy = config;
    const float *value = field(copy, index);
    return value == nullptr ? 0.0f : *value;
}

inline bool sameValue(float a, float b) { return std::fabs(a - b) < 1e-4f; }

inline long toEncoder(int index, float value) {
    const FieldSpec &spec = kFields[index];
    const long counts = std::lround(value * spec.encoderScale);
    return std::min(std::max(counts, spec.encoderMin), spec.encoderMax);
}

inline float fromEncoder(int index, long counts) {
    return static_cast<float>(counts) / kFields[index].encoderScale;
}

/// "Max stroke: 500 mm", "Belt pitch: 2.5 mm", "Steps/rev: 800".
inline std::string formatValue(int index, float value) {
    const FieldSpec &spec = kFields[index];
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%.*f", spec.decimals, value);
    std::string text = buffer;
    if (spec.units[0] != '\0') {
        text += " ";
        text += spec.units;
    }
    return text;
}

inline std::string formatField(int index, float value) {
    return std::string(kFields[index].label) + ": " + formatValue(index, value);
}

// ---------------------------------------------------------------------------
// Write ordering
// ---------------------------------------------------------------------------

/// The OSSM validates each setting.write against its stored geometry with
/// only that field changed. Find an order in which every intermediate
/// configuration is valid, so a multi-field change never fails half-way.
/// Returns the number of fields to write, or -1 if no order works.
inline int planWrites(const MachineConfig &stored, const MachineConfig &target,
                      std::array<int, kFieldCount> &order) {
    std::array<int, kFieldCount> changed{};
    int count = 0;
    for (int index = 0; index < kFieldCount; ++index) {
        if (!sameValue(fieldValue(stored, index), fieldValue(target, index)))
            changed[count++] = index;
    }
    if (count == 0) return 0;
    if (validateGeometry(target) != GeometryError::None) return -1;

    std::sort(changed.begin(), changed.begin() + count);
    do {
        MachineConfig candidate = stored;
        bool valid = true;
        for (int step = 0; step < count && valid; ++step) {
            const int index = changed[step];
            *field(candidate, index) = fieldValue(target, index);
            valid = validateGeometry(candidate) == GeometryError::None;
        }
        if (valid) {
            order = changed;
            return count;
        }
    } while (std::next_permutation(changed.begin(), changed.begin() + count));
    return -1;
}

}  // namespace ossm_rad_ble
