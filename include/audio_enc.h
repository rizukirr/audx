#ifndef AUDIO_ENC_H
#define AUDIO_ENC_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

/**
 * @brief Quality presets for audio encoding.
 *
 * These presets map to codec-specific bitrates or quality settings:
 * - Lossy codecs (MP3, AAC, Opus): Maps to bitrate ranges
 * - Lossless codecs (FLAC, ALAC): Maps to compression levels
 */
enum audio_quality {
  AUDIO_QUALITY_LOW = 0,     /* 96-128k for lossy, level 5 for lossless */
  AUDIO_QUALITY_MEDIUM = 1,  /* 160-192k for lossy, level 8 for lossless */
  AUDIO_QUALITY_HIGH = 2,    /* 256-320k for lossy, level 12 for lossless */
  AUDIO_QUALITY_EXTREME = 3, /* 320k+ for lossy, max compression for lossless */
};

/**
 * @brief Audio encoder abstraction built around FFmpeg.
 *
 * This structure wraps all necessary FFmpeg components for encoding
 * PCM audio frames into compressed audio formats (MP3, AAC, Opus, FLAC, etc.).
 *
 * It provides a unified interface for:
 *  - Initializing an encoder with specified codec and quality
 *  - Encoding PCM frames to compressed packets
 *  - Managing output file writing and muxing
 */
struct audio_enc {
  /**
   * @brief Output format context for writing the encoded file.
   *
   * Manages the output container format (e.g., MP4, OGG, MP3).
   */
  AVFormatContext *fmt_ctx;

  /**
   * @brief Codec context for the audio encoder.
   *
   * Handles encoding of PCM frames into compressed audio packets.
   */
  AVCodecContext *codec_ctx;

  /**
   * @brief Pointer to the encoder codec.
   *
   * Holds the encoder reference (e.g., libmp3lame, aac, libopus).
   */
  const AVCodec *codec;

  /**
   * @brief Output audio stream in the container.
   *
   * Represents the audio track in the output file.
   */
  AVStream *stream;

  /**
   * @brief Temporary packet structure for encoded data.
   *
   * Holds compressed audio packets before writing to file.
   */
  AVPacket *pkt;

  /**
   * @brief Presentation timestamp counter for encoded frames.
   *
   * Tracks the timestamp of each frame for proper A/V sync.
   */
  int64_t pts;

  /**
   * @brief Audio FIFO buffer for frame size management.
   *
   * Buffers samples to ensure frames sent to encoder have the correct size.
   * Required because encoders like MP3 need fixed frame sizes (e.g., 1152 samples).
   */
  AVAudioFifo *fifo;

  /**
   * @brief Software resampler for format conversion.
   *
   * Converts audio from input format to encoder's required format.
   * Handles sample format and channel layout conversions.
   */
  SwrContext *swr_ctx;
};

/**
 * @brief Initialize an audio encoder with the specified parameters.
 *
 * Opens the output file, initializes the encoder codec, and prepares
 * the muxer for writing compressed audio data.
 *
 * Supported codecs:
 * - libmp3lame: MP3 encoding
 * - aac: AAC encoding (native FFmpeg encoder)
 * - libopus: Opus encoding
 * - flac: FLAC lossless encoding
 * - alac: Apple Lossless encoding
 * - pcm_s16le: Raw 16-bit PCM (WAV)
 *
 * @param encoder Pointer to audio_enc struct to initialize.
 * @param filename Output file path (determines container format).
 * @param codec_name FFmpeg codec name (e.g., "libmp3lame", "libopus").
 * @param sample_rate Output sample rate in Hz (e.g., 44100).
 * @param ch_layout Channel layout structure (e.g., stereo, mono).
 * @param quality Quality preset (AUDIO_QUALITY_LOW to AUDIO_QUALITY_EXTREME).
 * @param bitrate_str Explicit bitrate string (e.g., "192k"), or NULL to use quality preset.
 * @return 0 on success, negative AVERROR code on failure.
 */
int audio_enc_init(struct audio_enc *encoder, const char *filename,
                   const char *codec_name, int sample_rate,
                   const AVChannelLayout *ch_layout, enum audio_quality quality,
                   const char *bitrate_str);

/**
 * @brief Encode and write a PCM audio frame to the output file.
 *
 * Sends a PCM frame to the encoder, retrieves compressed packets,
 * and writes them to the output file.
 *
 * @param encoder Initialized audio_enc instance.
 * @param frame PCM audio frame to encode (NULL to flush encoder).
 * @return 0 on success, negative AVERROR code on failure.
 */
int audio_enc_write_frame(struct audio_enc *encoder, AVFrame *frame);

/**
 * @brief Finalize encoding and close the output file.
 *
 * Flushes any remaining frames in the encoder, writes the file trailer,
 * and closes the output file handle.
 *
 * @param encoder Initialized audio_enc instance.
 * @return 0 on success, negative AVERROR code on failure.
 */
int audio_enc_finalize(struct audio_enc *encoder);

/**
 * @brief Free all allocated FFmpeg encoder resources.
 *
 * Cleans up encoder context, format context, packets, and internal memory.
 * Must be called after encoding is complete to avoid memory leaks.
 *
 * @param encoder Initialized audio_enc instance to free.
 */
void audio_enc_free(struct audio_enc *encoder);

#endif /* AUDIO_ENC_H */
