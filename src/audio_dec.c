#include "../include/audio_dec.h"
#include <stdio.h>

/**
 * @brief Print a human-readable FFmpeg error message.
 *
 * Converts FFmpeg error codes (negative integers) into text and prints them.
 */
static void logerr(const char *msg, int errnum) {
  char errbuf[256];
  av_strerror(errnum, errbuf, sizeof(errbuf));
  fprintf(stderr, "Error: %s (%s)\n", msg, errbuf);
}

/**
 * @brief Initialize an audio decoder for a given file.
 *
 * This function sets up all FFmpeg contexts (format, codec, resampler)
 * required to decode an input audio file into PCM frames.
 */
int audio_dec_init(struct audio_dec *decoder, const char *filename) {
  av_log_set_level(AV_LOG_ERROR); // Only log critical FFmpeg errors

  if (decoder == NULL) {
    fprintf(stderr, "Cannot open input file\n");
    return -1;
  }

  int ret;

  // Initialize all fields of the decoder struct to zero
  // to ensure safe cleanup even if initialization fails midway.
  memset(decoder, 0, sizeof(*decoder));

  // Open the input file
  ret = avformat_open_input(&decoder->fmt_ctx, filename, NULL, NULL);
  if (ret < 0) {
    logerr("Cannot open input file", ret);
    return ret;
  }

  // Read stream information (metadata, codecs, etc.)
  ret = avformat_find_stream_info(decoder->fmt_ctx, NULL);
  if (ret < 0) {
    logerr("Cannot find stream info", ret);
    goto fail;
  }

  //  Locate the first audio stream in the file
  decoder->stream_index = av_find_best_stream(
      decoder->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  if (decoder->stream_index < 0) {
    fprintf(stderr, "No audio stream found\n");
    ret = decoder->stream_index;
    goto fail;
  }

  AVStream *stream = decoder->fmt_ctx->streams[decoder->stream_index];

  // Find and open the appropriate decoder
  decoder->codec = avcodec_find_decoder(stream->codecpar->codec_id);
  if (!decoder->codec) {
    fprintf(stderr, "Unsupported codec\n");
    ret = AVERROR_DECODER_NOT_FOUND;
    goto fail;
  }

  decoder->codec_ctx = avcodec_alloc_context3(decoder->codec);
  if (!decoder->codec_ctx) {
    fprintf(stderr, "Failed to allocate codec context\n");
    ret = AVERROR(ENOMEM);
    goto fail;
  }

  // Copy codec parameters from stream to codec context
  ret = avcodec_parameters_to_context(decoder->codec_ctx, stream->codecpar);
  if (ret < 0) {
    logerr("Cannot copy codec parameters", ret);
    goto fail;
  }

  // Open the codec
  ret = avcodec_open2(decoder->codec_ctx, decoder->codec, NULL);
  if (ret < 0) {
    logerr("Cannot open codec", ret);
    goto fail;
  }

  // Allocate reusable packet & frame
  decoder->pkt = av_packet_alloc();
  decoder->frame = av_frame_alloc();
  if (!decoder->pkt || !decoder->frame) {
    fprintf(stderr, "Failed to allocate packet/frame\n");
    ret = AVERROR(ENOMEM);
    goto fail;
  }

  // Configure resampler (SwrContext)
  decoder->sample_rate = decoder->codec_ctx->sample_rate;
  decoder->channels = decoder->codec_ctx->ch_layout.nb_channels;

  // Set target channel layout and output format
  av_channel_layout_default(&decoder->dst_ch_layout, decoder->channels);
  decoder->dst_fmt = AV_SAMPLE_FMT_S16; // output as signed 16-bit PCM

  decoder->swr_ctx = swr_alloc();
  if (!decoder->swr_ctx) {
    fprintf(stderr, "Failed to allocate SwrContext\n");
    ret = AVERROR(ENOMEM);
    goto fail;
  }

  // Configure conversion parameters for SwrContext
  if ((ret = av_opt_set_chlayout(decoder->swr_ctx, "in_chlayout",
                                 &decoder->codec_ctx->ch_layout, 0)) < 0 ||
      (ret = av_opt_set_chlayout(decoder->swr_ctx, "out_chlayout",
                                 &decoder->dst_ch_layout, 0)) < 0 ||
      (ret = av_opt_set_int(decoder->swr_ctx, "in_sample_rate",
                            decoder->codec_ctx->sample_rate, 0)) < 0 ||
      (ret = av_opt_set_int(decoder->swr_ctx, "out_sample_rate",
                            decoder->sample_rate, 0)) < 0 ||
      (ret = av_opt_set_sample_fmt(decoder->swr_ctx, "in_sample_fmt",
                                   decoder->codec_ctx->sample_fmt, 0)) < 0 ||
      (ret = av_opt_set_sample_fmt(decoder->swr_ctx, "out_sample_fmt",
                                   decoder->dst_fmt, 0)) < 0) {
    logerr("Failed to configure SwrContext", ret);
    goto fail;
  }

  // Initialize resampler
  ret = swr_init(decoder->swr_ctx);
  if (ret < 0) {
    logerr("Cannot initialize SwrContext", ret);
    goto fail;
  }

  return 0; // success

// Unified cleanup path for failures
fail:
  audio_dec_free(decoder);
  return ret;
}

/**
 * @brief Read and decode the next PCM chunk.
 *
 * Reads one packet from the audio file, decodes it, converts it
 * into the desired PCM format, and returns the raw buffer.
 */
int audio_decoder_read(struct audio_dec *decoder, uint8_t **out_data,
                       int *out_size) {
  *out_data = NULL;
  *out_size = 0;
  int ret;

  //  Read encoded packet
  ret = av_read_frame(decoder->fmt_ctx, decoder->pkt);
  if (ret < 0) {
    if (ret != AVERROR_EOF) {
      logerr("Error reading frame", ret);
    }
    return 0; // EOF or error
  }

  // Skip non-audio packets (e.g., metadata or other streams)
  if (decoder->pkt->stream_index != decoder->stream_index) {
    av_packet_unref(decoder->pkt);
    return 1;
  }

  // Send packet to decoder
  ret = avcodec_send_packet(decoder->codec_ctx, decoder->pkt);
  if (ret < 0) {
    logerr("Error sending packet to decoder", ret);
    av_packet_unref(decoder->pkt);
    return 1;
  }

  // Receive decoded frame
  ret = avcodec_receive_frame(decoder->codec_ctx, decoder->frame);
  while (ret >= 0) {
    // Calculate number of samples after resampling
    int dst_nb_samples = av_rescale_rnd(
        swr_get_delay(decoder->swr_ctx, decoder->codec_ctx->sample_rate) +
            decoder->frame->nb_samples,
        decoder->sample_rate, decoder->codec_ctx->sample_rate, AV_ROUND_UP);

    uint8_t **converted = NULL;

    // Allocate output buffer for resampled PCM
    ret =
        av_samples_alloc_array_and_samples(&converted, NULL, decoder->channels,
                                           dst_nb_samples, decoder->dst_fmt, 0);

    if (ret < 0) {
      logerr("Failed to allocate output buffer", ret);
      break;
    }

    // Perform sample format & rate conversion
    int samples_converted = swr_convert(
        decoder->swr_ctx, converted, dst_nb_samples,
        (const uint8_t **)decoder->frame->data, decoder->frame->nb_samples);

    if (samples_converted < 0) {
      logerr("Error during resampling", samples_converted);
      av_freep(&converted[0]);
      av_freep(&converted);
      break;
    }

    // Calculate output buffer size in bytes
    int buffer_size = av_samples_get_buffer_size(
        NULL, decoder->channels, samples_converted, decoder->dst_fmt, 1);

    if (buffer_size < 0) {
      logerr("Invalid buffer size", buffer_size);
      av_freep(&converted[0]);
      av_freep(&converted);
      break;
    }

    // Assign decoded PCM data to output
    *out_data = converted[0];
    *out_size = buffer_size;

    // Free container array (not the actual buffer, owned by caller)
    av_freep(&converted);
    av_packet_unref(decoder->pkt);
    return 1; // processed one frame successfully
  }

  // If not EOF or EAGAIN, report error
  if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    logerr("Error receiving frame", ret);
  }

  av_packet_unref(decoder->pkt);
  return 1;
}

/**
 * @brief Free all allocated FFmpeg resources.
 *
 * Safely releases SwrContext, codec, frame, packet, and format contexts.
 */
void audio_dec_free(struct audio_dec *decoder) {
  if (decoder->swr_ctx)
    swr_free(&decoder->swr_ctx);
  if (decoder->frame)
    av_frame_free(&decoder->frame);
  if (decoder->pkt)
    av_packet_free(&decoder->pkt);
  if (decoder->codec_ctx)
    avcodec_free_context(&decoder->codec_ctx);
  if (decoder->fmt_ctx)
    avformat_close_input(&decoder->fmt_ctx);
}
