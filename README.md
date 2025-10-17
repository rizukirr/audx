# audx

A flexible audio transcoding tool built on FFmpeg that supports decoding, filtering, and encoding audio files with various codecs and quality presets.

## Features

- Decode audio from any FFmpeg-supported format
- Apply FFmpeg audio filters (tempo, volume, effects, etc.)
- Encode to multiple formats: MP3, AAC, Opus, FLAC, ALAC, WAV/PCM
- Quality presets (low, medium, high, extreme) or explicit bitrate control
- Sample rate and format conversion
- Frame buffering for fixed-size encoder requirements

## Building

### Requirements

- CMake 4.0 or higher
- C compiler (GCC, Clang)
- FFmpeg development libraries:
  - libavcodec
  - libavformat
  - libavutil
  - libavfilter
  - libswresample
  - libswscale

### Installation on Debian/Ubuntu

```bash
sudo apt install cmake build-essential \
  libavcodec-dev \
  libavformat-dev \
  libavutil-dev \
  libavfilter-dev \
  libswresample-dev \
  libswscale-dev
```

### Installation on Arch Linux

```bash
sudo pacman -S cmake gcc ffmpeg
```

### Build Instructions

```bash
mkdir build
cd build
cmake ..
make
```

The executable will be located at `build/bin/audx`.

## Usage

```
audx <input> <output> [OPTIONS]
```

### Options

- `--codec=<name>` - Encoder codec (libmp3lame, aac, libopus, flac, alac, pcm_s16le)
- `--quality=<preset>` - Quality preset: low, medium, high, extreme (default: high)
- `--bitrate=<rate>` - Explicit bitrate (e.g., 192k, 320k) - overrides quality
- `--filter=<desc>` - FFmpeg filter chain (e.g., "atempo=1.25,volume=0.5")

## Supported Codecs

### Lossy Codecs

| Codec | Name       | Quality Bitrates          |
| ----- | ---------- | ------------------------- |
| MP3   | libmp3lame | 128k / 192k / 256k / 320k |
| AAC   | aac        | 96k / 160k / 256k / 320k  |
| Opus  | libopus    | 96k / 128k / 192k / 256k  |

### Lossless Codecs

| Codec | Name | Compression Levels |
| ----- | ---- | ------------------ |
| FLAC  | flac | 5 / 8 / 10 / 12    |
| ALAC  | alac | 5 / 8 / 10 / 12    |

### Raw Audio

| Format  | Name      | Description                     |
| ------- | --------- | ------------------------------- |
| WAV/PCM | pcm_s16le | 16-bit signed little-endian PCM |

## Examples

### Basic Transcoding

Convert MP3 to Opus with high quality:

```bash
audx input.mp3 output.opus --codec=libopus --quality=high
```

Convert FLAC to MP3 with explicit bitrate:

```bash
audx input.flac output.mp3 --codec=libmp3lame --bitrate=320k
```

Convert to lossless FLAC:

```bash
audx input.mp3 output.flac --codec=flac --quality=extreme
```

### Audio Filtering

Speed up audio by 25%:

```bash
audx input.mp3 output.mp3 --codec=libmp3lame --quality=high --filter="atempo=1.25"
```

Reduce volume by 50%:

```bash
audx input.mp3 output.mp3 --codec=libmp3lame --quality=high --filter="volume=0.5"
```

Chain multiple filters:

```bash
audx input.mp3 output.mp3 --codec=libmp3lame --bitrate=320k --filter="atempo=1.25,volume=0.8"
```

Apply audio effects:

```bash
audx input.mp3 output.mp3 --codec=libmp3lame --quality=high \
  --filter="acrusher=level_in=1:level_out=1:bits=8:mode=log:aa=1"
```

### Raw PCM Output

Extract raw PCM data (no encoding):

```bash
audx input.mp3 output.pcm
```

Play raw PCM with ffplay:

```bash
ffplay -f s16le -ar 44100 -ac 2 output.pcm
```

## Quality Presets

Quality presets map to codec-specific settings:

### For Lossy Codecs (bitrate in kbps)

| Preset  | MP3 | AAC | Opus |
| ------- | --- | --- | ---- |
| low     | 128 | 96  | 96   |
| medium  | 192 | 160 | 128  |
| high    | 256 | 256 | 192  |
| extreme | 320 | 320 | 256  |

### For Lossless Codecs (compression level)

| Preset  | FLAC/ALAC |
| ------- | --------- |
| low     | 5         |
| medium  | 8         |
| high    | 10        |
| extreme | 12        |

Higher compression levels result in smaller file sizes with slower encoding.

## FFmpeg Filter Examples

Common audio filters you can use with the `--filter` option:

- `atempo=1.5` - Speed up audio by 50%
- `atempo=0.75` - Slow down audio by 25%
- `volume=0.5` - Reduce volume by 50%
- `volume=2.0` - Double the volume
- `aecho=0.8:0.88:60:0.4` - Add echo effect
- `highpass=f=200` - High-pass filter at 200Hz
- `lowpass=f=3000` - Low-pass filter at 3000Hz
- `equalizer=f=1000:width_type=h:width=200:g=-10` - 10dB cut at 1kHz

For complete filter documentation, see: https://ffmpeg.org/ffmpeg-filters.html#Audio-Filters

## Notes

### Opus Sample Rate Requirements

The Opus codec only supports specific sample rates: 48000, 24000, 16000, 12000, 8000 Hz.
If your input has a different sample rate (e.g., 44100 Hz), you need to add resampling:

```bash
audx input.mp3 output.opus --codec=libopus --quality=high --filter="aresample=48000"
```

### Backward Compatibility

If no `--codec` option is specified, audx outputs raw PCM data, maintaining backward compatibility with earlier versions.

## Architecture

audx implements a complete audio processing pipeline:

1. **Decoder** (audio_dec.c) - Decodes input audio to PCM frames
2. **Filter** (audio_filter.c) - Applies FFmpeg filter graph to frames
3. **Encoder** (audio_enc.c) - Encodes frames to target codec
4. **Muxer** - Writes encoded data to output container

The encoder includes:

- AVAudioFifo buffer for frame size management
- SwrContext for automatic format conversion
- Dynamic format detection and conversion

## License

**audx source code** is licensed under the [MIT License](LICENSE).

**FFmpeg libraries** used by audx are licensed under the LGPL 2.1 or later.

### Important Legal Information

⚠️ **For Commercial Use**: Some audio codecs (particularly AAC) may require patent licenses in certain jurisdictions, independent of copyright licensing. Consult with a lawyer before using audx in commercial products.

📄 **Complete licensing information, FFmpeg compliance details, and patent notices**: See [LEGAL_NOTICES.md](LEGAL_NOTICES.md)

### Quick Compliance Summary

✅ **Personal/Non-Commercial Use**: Free to use with all codecs
✅ **Commercial Use**: Verify patent requirements for your jurisdiction
✅ **Distribution**: Must include LICENSE and LEGAL_NOTICES.md files
✅ **Modifications**: Must maintain MIT license for audx, LGPL for FFmpeg libraries

For FFmpeg legal information: https://www.ffmpeg.org/legal.html

## References

- FFmpeg Documentation: https://ffmpeg.org/documentation.html
- FFmpeg Audio Filters: https://ffmpeg.org/ffmpeg-filters.html#Audio-Filters
- FFmpeg Codecs: https://ffmpeg.org/ffmpeg-codecs.html
