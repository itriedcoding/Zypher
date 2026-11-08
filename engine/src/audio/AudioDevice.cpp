#include "zypher/audio/AudioDevice.h"

#include "zypher/core/Log.h"

#include <algorithm>

namespace zy {

Status NullAudioDevice::initialize(const AudioDesc& desc) {
    if (m_initialized) {
        return Error{ErrorCode::InvalidArgument, "audio device already initialised"};
    }
    m_desc = desc;
    m_desc.channels = std::clamp(m_desc.channels, 1u, 8u);
    m_desc.sampleRate = std::clamp(m_desc.sampleRate, 8000u, 192000u);
    m_initialized = true;
    ZY_LOG_INFO("audio", "null audio device at ", m_desc.sampleRate, " Hz, ", m_desc.channels, " channel(s)");
    return ok();
}

void NullAudioDevice::shutdown() {
    m_voices.clear();
    m_initialized = false;
}

VoiceId NullAudioDevice::play(const AudioClipData& clip, f32 gain, bool loop) {
    if (!m_initialized || clip.samples.empty()) {
        return 0;
    }
    const Voice voice{m_nextVoice++, gain, loop};
    m_voices.push_back(voice);
    ++m_playRequests;
    return voice.id;
}

void NullAudioDevice::stop(VoiceId voice) {
    m_voices.erase(std::remove_if(m_voices.begin(), m_voices.end(),
                                  [voice](const Voice& entry) { return entry.id == voice; }),
                   m_voices.end());
}

void NullAudioDevice::setMasterGain(f32 gain) { m_desc.masterGain = std::clamp(gain, 0.0f, 1.0f); }

void NullAudioDevice::setListener(const Vec3& position, const Vec3& forward) {
    (void)position;
    (void)forward;
}

u32 NullAudioDevice::activeVoices() const { return static_cast<u32>(m_voices.size()); }

Result<Scope<AudioDevice>> createAudioDevice(const AudioDesc& desc) {
#if defined(ZYPHER_WITH_MINIAUDIO)
    extern Scope<AudioDevice> createMiniaudioDevice();
    auto device = createMiniaudioDevice();
    if (device != nullptr) {
        ZY_TRY(device->initialize(desc));
        return device;
    }
    ZY_LOG_WARN("audio", "miniaudio backend unavailable, falling back to the null device");
#endif
    auto device = makeScope<NullAudioDevice>();
    ZY_TRY(device->initialize(desc));
    return Scope<AudioDevice>(std::move(device));
}

} // namespace zy
