//! Audio asset handling (WAV, OGG, FLAC).
//!
//! The encoded payload is stored verbatim so the runtime can decode it with
//! its audio backend (miniaudio); the header records the codec and stream
//! metadata for fast inspection without decoding.

pub const AUDIO_MAGIC: &[u8; 8] = b"ZYAUD001";

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AudioCodec {
    Unknown = 0,
    Wav = 1,
    OggOpus = 2,
    OggVorbis = 3,
    Flac = 4,
}

#[derive(Clone, Debug, PartialEq)]
pub struct AudioInfo {
    pub codec: AudioCodec,
    pub channels: u32,
    pub sample_rate: u32,
    pub bits_per_sample: u32,
}

fn u16_le(bytes: &[u8], offset: usize) -> u16 {
    u16::from_le_bytes([bytes[offset], bytes[offset + 1]])
}

fn u32_le(bytes: &[u8], offset: usize) -> u32 {
    u32::from_le_bytes([bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3]])
}

fn wav_info(data: &[u8]) -> Result<AudioInfo, String> {
    if data.len() < 44 || &data[0..4] != b"RIFF" || &data[8..12] != b"WAVE" {
        return Err("not a RIFF/WAVE file".into());
    }
    let mut offset = 12;
    while offset + 8 <= data.len() {
        let chunk_id = &data[offset..offset + 4];
        let chunk_size = u32_le(data, offset + 4) as usize;
        if chunk_id == b"fmt " {
            if offset + 8 + 16 > data.len() {
                return Err("WAV fmt chunk is truncated".into());
            }
            let channels = u16_le(data, offset + 10) as u32;
            let sample_rate = u32_le(data, offset + 12);
            let bits = u16_le(data, offset + 22) as u32;
            return Ok(AudioInfo {
                codec: AudioCodec::Wav,
                channels,
                sample_rate,
                bits_per_sample: bits,
            });
        }
        offset += 8 + chunk_size + (chunk_size & 1);
    }
    Err("WAV is missing its fmt chunk".into())
}

fn ogg_info(data: &[u8]) -> Result<AudioInfo, String> {
    if data.len() < 32 || &data[0..4] != b"OggS" {
        return Err("not an Ogg stream".into());
    }
    let window = &data[..data.len().min(256)];
    if let Some(position) = window.windows(8).position(|slice| slice == b"OpusHead") {
        let start = position + 8;
        if start + 10 <= data.len() {
            let channels = data[start + 1] as u32;
            let sample_rate = u32_le(data, start + 4).max(48_000);
            return Ok(AudioInfo {
                codec: AudioCodec::OggOpus,
                channels,
                sample_rate: if sample_rate == 0 { 48_000 } else { sample_rate },
                bits_per_sample: 0,
            });
        }
        return Ok(AudioInfo { codec: AudioCodec::OggOpus, channels: 2, sample_rate: 48_000, bits_per_sample: 0 });
    }
    if window.windows(6).any(|slice| slice == b"vorbis") {
        return Ok(AudioInfo { codec: AudioCodec::OggVorbis, channels: 2, sample_rate: 44_100, bits_per_sample: 0 });
    }
    Err("Ogg stream has no supported codec (Opus or Vorbis)".into())
}

fn flac_info(data: &[u8]) -> Result<AudioInfo, String> {
    if data.len() < 42 || &data[0..4] != b"fLaC" {
        return Err("not a FLAC file".into());
    }
    // First metadata block must be STREAMINFO (type 0).
    let header = data[4];
    let block_type = header & 0x7F;
    if block_type != 0 {
        return Err("FLAC is missing its STREAMINFO block".into());
    }
    let info = &data[8..];
    if info.len() < 34 {
        return Err("FLAC STREAMINFO is truncated".into());
    }
    // 20 bits sample rate, 3 bits channels-1, 5 bits bits-1, starting at byte 10.
    let packed = ((info[10] as u32) << 16) | ((info[11] as u32) << 8) | info[12] as u32;
    let sample_rate = packed >> 12;
    let channels = ((packed >> 9) & 0x7) + 1;
    let bits = (((packed >> 4) & 0x1F) + 1) as u32;
    Ok(AudioInfo { codec: AudioCodec::Flac, channels, sample_rate, bits_per_sample: bits })
}

pub fn inspect(extension: &str, data: &[u8]) -> Result<AudioInfo, String> {
    match extension.to_ascii_lowercase().as_str() {
        ".wav" => wav_info(data),
        ".ogg" | ".opus" => ogg_info(data),
        ".flac" => flac_info(data),
        other => Err(format!("unsupported audio extension '{}'", other)),
    }
}

pub fn encode_blob(extension: &str, data: &[u8]) -> Result<Vec<u8>, String> {
    let info = inspect(extension, data)?;
    let mut buffer = Vec::with_capacity(32 + data.len());
    buffer.extend_from_slice(AUDIO_MAGIC);
    buffer.extend_from_slice(&(info.codec as u32).to_le_bytes());
    buffer.extend_from_slice(&info.channels.to_le_bytes());
    buffer.extend_from_slice(&info.sample_rate.to_le_bytes());
    buffer.extend_from_slice(&info.bits_per_sample.to_le_bytes());
    buffer.extend_from_slice(&(data.len() as u32).to_le_bytes());
    buffer.extend_from_slice(&0u32.to_le_bytes()); // reserved
    buffer.extend_from_slice(data);
    Ok(buffer)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn silent_wav() -> Vec<u8> {
        let mut data = Vec::new();
        data.extend_from_slice(b"RIFF");
        data.extend_from_slice(&36u32.to_le_bytes());
        data.extend_from_slice(b"WAVE");
        data.extend_from_slice(b"fmt ");
        data.extend_from_slice(&16u32.to_le_bytes());
        data.extend_from_slice(&1u16.to_le_bytes()); // PCM
        data.extend_from_slice(&1u16.to_le_bytes()); // mono
        data.extend_from_slice(&44_100u32.to_le_bytes());
        data.extend_from_slice(&88_200u32.to_le_bytes());
        data.extend_from_slice(&2u16.to_le_bytes());
        data.extend_from_slice(&16u16.to_le_bytes());
        data.extend_from_slice(b"data");
        data.extend_from_slice(&0u32.to_le_bytes());
        data
    }

    #[test]
    fn parses_wav_metadata() {
        let info = inspect(".wav", &silent_wav()).expect("valid wav");
        assert_eq!(info.codec, AudioCodec::Wav);
        assert_eq!(info.channels, 1);
        assert_eq!(info.sample_rate, 44_100);
        assert_eq!(info.bits_per_sample, 16);
    }

    #[test]
    fn encodes_audio_blob() {
        let blob = encode_blob(".wav", &silent_wav()).expect("encodes");
        assert_eq!(&blob[0..8], AUDIO_MAGIC);
        assert_eq!(u32::from_le_bytes([blob[8], blob[9], blob[10], blob[11]]), 1);
    }

    #[test]
    fn rejects_garbage() {
        assert!(inspect(".wav", b"garbage").is_err());
        assert!(inspect(".mp3", b"garbage").is_err());
    }
}
