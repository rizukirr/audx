#ifndef AUDIO_DEC
#define AUDIO_DEC

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

/**
 * @brief Audio decoder abstraction built around FFmpeg.
 *
 * This structure wraps all the necessary FFmpeg components for decoding
 * compressed audio files (e.g., MP3, MP4, AAC) into raw PCM audio samples.
 *
 * It provides a unified interface for:
 *  - Opening an audio file or stream
 *  - Decoding compressed audio packets
 *  - Resampling and converting to a desired PCM format
 *
 * This makes it easier to process or stream decoded PCM chunks
 * without directly managing FFmpeg's complex internal contexts.
 */
struct audio_dec {
  /**
   * @brief Manages input file or container format context.
   *
   * Responsible for reading and parsing media files (e.g., MP3, MP4).
   * Created via `avformat_open_input()` and used by the demuxer to
   * locate audio streams.
   */
  AVFormatContext *fmt_ctx;

  /**
   * @brief Codec context for the selected audio stream.
   *
   * Handles decoding of compressed audio packets into raw frames.
   * Created from the codec found via `avcodec_find_decoder()`.
   */
  AVCodecContext *codec_ctx;

  /**
   * @brief Pointer to the codec used for decoding.
   *
   * Holds the codec reference returned by `avcodec_find_decoder()`.
   */
  const AVCodec *codec;

  /**
   * @brief Temporary packet structure used for demuxed data.
   *
   * Each call to `av_read_frame()` fills this packet with encoded audio data
   * that will be sent to the decoder.
   */
  AVPacket *pkt;

  /**
   * @brief Frame structure for storing decoded, uncompressed audio samples.
   *
   * After decoding, each frame contains raw audio samples before resampling.
   */
  AVFrame *frame;

  /**
   * @brief Software resampler context.
   *
   * Converts audio from the source format (channels, sample rate, sample fmt)
   * to the desired target PCM format. Initialized using
   * `swr_alloc_set_opts2()`.
   */
  SwrContext *swr_ctx;

  /**
   * @brief Index of the audio stream inside the media container.
   *
   * Determined by scanning available streams in the input file and selecting
   * the first one that contains audio.
   */
  int stream_index;

  /**
   * @brief Target output sample rate in Hz (e.g., 44100).
   *
   * The resampler will convert to this rate if the source differs.
   */
  int sample_rate;

  /**
   * @brief Target output number of channels (e.g., 2 for stereo).
   */
  int channels;

  /**
   * @brief Target PCM sample format (e.g., AV_SAMPLE_FMT_S16).
   *
   * Determines how each audio sample is represented (integer or float, bit
   * depth).
   */
  enum AVSampleFormat dst_fmt;

  /**
   * @brief Target output channel layout.
   *
   * Defines speaker arrangement (e.g., stereo, mono, 5.1, etc.).
   * Used together with `channels` for correct resampling.
   */
  AVChannelLayout dst_ch_layout;
};

/**
 * @brief Initialize and prepare the decoder for reading.
 *
 * Opens the given audio file, finds the audio stream, initializes
 * codec and resampler contexts, and prepares everything for decoding.
 *
 * @param decoder Pointer to an `audio_dec` struct to initialize.
 * @param filename Path to the input audio file (e.g., "song.mp3").
 * @return 0 on success, negative AVERROR code on failure.
 */
int audio_dec_init(struct audio_dec *decoder, const char *filename);

/**
 * @brief Read and decode the next audio packet into PCM data.
 *
 * Reads one packet from the input file, decodes it, resamples
 * to the desired format, and writes the resulting PCM buffer to `out_data`.
 *
 * This function can be called repeatedly to read sequential PCM chunks.
 *
 * @param decoder Initialized `audio_dec` instance.
 * @param out_data Pointer to the decoded PCM buffer (allocated by FFmpeg).
 * @param out_size Size of the decoded PCM data in bytes.
 * @return 1 on successful frame read, 0 on EOF, negative on error.
 */
int audio_decoder_read(struct audio_dec *decoder, uint8_t **out_data,
                       int *out_size);

/**
 * @brief Release all allocated FFmpeg resources.
 *
 * Cleans up contexts, buffers, and internal memory used by the decoder.
 * Must be called after decoding is complete to avoid memory leaks.
 *
 * @param decoder Initialized `audio_dec` instance to free.
 */
void audio_dec_free(struct audio_dec *decoder);

#endif
