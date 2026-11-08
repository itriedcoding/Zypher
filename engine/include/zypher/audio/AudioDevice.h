// Audio output abstraction.
//
// The null device is always available and is what headless runs and CI use.
// Enabling ZYPHER_WITH_MINIAUDIO compiles in the real backend.
#pragma once

#include "zypher/assets/AssetId.h"
#include "zypher/core/Base.h"
#include "zypher/core/Result.h"
#include "zypher/math/Vec.h"

#include <string>
#include <vector>

namespace zy {

using VoiceId = u64;

struct AudioDesc {
    u32 sampleRate = 48000;
    u32 channels = 2;
    f32 masterGain = 1.0f;
};

struct AudioClipData {
    u32 sampleRate = 48000;
    u32 channels = 2;
    std::vector<f32> samples; // interleaved
};

class AudioDevice {
public:
    virtual ~AudioDevice() = default;

    [[nodiscard]] virtual std::string_view name() const = 0;
    [[nodiscard]] virtual Status initialize(const AudioDesc& desc) = 0;
    virtual void shutdown() = 0;

    // Returns 0 when the clip could not be played.
    virtual VoiceId play(const AudioClipData& clip, f32 gain, bool loop) = 0;
    virtual void stop(VoiceId voice) = 0;
    virtual void setMasterGain(f32 gain) = 0;
    virtual void setListener(const Vec3& position, const Vec3& forward) = 0;

    [[nodiscard]] virtual u32 activeVoices() const = 0;
};

// Silent device: tracks voices so gameplay code and tests can observe audio
// events without an output device.
class NullAudioDevice final : public AudioDevice {
public:
    [[nodiscard]] std::string_view name() const override { return "Null Audio"; }
    [[nodiscard]] Status initialize(const AudioDesc& desc) override;
    void shutdown() override;
    VoiceId play(const AudioClipData& clip, f32 gain, bool loop) override;
    void stop(VoiceId voice) override;
    void setMasterGain(f32 gain) override;
    void setListener(const Vec3& position, const Vec3& forward) override;
    [[nodiscard]] u32 activeVoices() const override;

    [[nodiscard]] u32 totalPlayRequests() const { return m_playRequests; }

private:
    struct Voice {
        VoiceId id = 0;
        f32 gain = 1.0f;
        bool loop = false;
    };
    AudioDesc m_desc{};
    std::vector<Voice> m_voices;
    VoiceId m_nextVoice = 1;
    u32 m_playRequests = 0;
    bool m_initialized = false;
};

[[nodiscard]] Result<Scope<AudioDevice>> createAudioDevice(const AudioDesc& desc);

} // namespace zy
