#include "../include/audio_enc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

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
 * @brief Select appropriate bitrate based on codec and quality preset.
 *
 * Maps quality presets to codec-specific bitrates or compression levels.
 *
 * @param codec_name FFmpeg codec name.
 * @param quality Quality preset enum value.
 * @return Bitrate in bits per second, or 0 for lossless codecs.
 */
static int get_bitrate_for_quality(const char *codec_name,
                                   enum audio_quality quality) {
  /* Lossless codecs don't use bitrate */
  if (strcmp(codec_name, "flac") == 0 || strcmp(codec_name, "alac") == 0 ||
      strcmp(codec_name, "pcm_s16le") == 0) {
    return 0;
  }

  /* Bitrate mapping for lossy codecs */
  const int bitrates[][4] = {
      /* LOW,    MEDIUM,  HIGH,    EXTREME */
      {128000, 192000, 256000, 320000}, /* libmp3lame */
      {96000, 160000, 256000, 320000},  /* aac */
      {96000, 128000, 192000, 256000},  /* libopus */
  };

  int idx = 0; /* default to MP3 bitrates */
  if (strcmp(codec_name, "aac") == 0)
    idx = 1;
  else if (strcmp(codec_name, "libopus") == 0)
    idx = 2;

  return bitrates[idx][quality];
}

/**
 * @brief Get compression level for lossless codecs.
 *
 * @param quality Quality preset enum value.
 * @return Compression level (0-12 for FLAC, 0-2 for ALAC).
 */
static int get_compression_level(enum audio_quality quality) {
  const int levels[] = {5, 8, 10, 12};
  return levels[quality];
}

int audio_enc_init(struct audio_enc *encoder, const char *filename,
                   const char *codec_name, int sample_rate,
                   const AVChannelLayout *ch_layout, enum audio_quality quality,
                   const char *bitrate_str) {
  int ret;

  if (!encoder || !filename || !codec_name || !ch_layout) {
    fprintf(stderr, "Invalid parameters to audio_enc_init\n");
    return AVERROR(EINVAL);
  }

  /* Initialize all fields to zero */
  memset(encoder, 0, sizeof(*encoder));

  /* Allocate output format context based on filename */
  ret = avformat_alloc_output_context2(&encoder->fmt_ctx, NULL, NULL, filename);
  if (ret < 0) {
    logerr("Failed to allocate output context", ret);
    return ret;
  }

  /* Find the encoder codec */
  encoder->codec = avcodec_find_encoder_by_name(codec_name);
  if (!encoder->codec) {
    fprintf(stderr, "Codec '%s' not found\n", codec_name);
    ret = AVERROR_ENCODER_NOT_FOUND;
    goto fail;
  }

  /* Create a new audio stream in the output file */
  encoder->stream = avformat_new_stream(encoder->fmt_ctx, NULL);
  if (!encoder->stream) {
    fprintf(stderr, "Failed to create output stream\n");
    ret = AVERROR(ENOMEM);
    goto fail;
  }

  /* Allocate encoder context */
  encoder->codec_ctx = avcodec_alloc_context3(encoder->codec);
  if (!encoder->codec_ctx) {
    fprintf(stderr, "Failed to allocate encoder context\n");
    ret = AVERROR(ENOMEM);
    goto fail;
  }

  /* Set encoder parameters */
  encoder->codec_ctx->sample_rate = sample_rate;
  ret = av_channel_layout_copy(&encoder->codec_ctx->ch_layout, ch_layout);
  if (ret < 0) {
    logerr("Failed to copy channel layout", ret);
    goto fail;
  }

  /* Select sample format supported by the encoder (using modern API) */
  const enum AVSampleFormat *sample_fmts = NULL;
  int num_fmts = 0;
  if (avcodec_get_supported_config(NULL, encoder->codec,
                                    AV_CODEC_CONFIG_SAMPLE_FORMAT, 0,
                                    (const void **)&sample_fmts, &num_fmts) >= 0 &&
      num_fmts > 0) {
    encoder->codec_ctx->sample_fmt = sample_fmts[0];
  } else {
    encoder->codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
  }

  /* Set bitrate or compression level */
  if (bitrate_str) {
    /* Parse explicit bitrate string (e.g., "192k") */
    char *endptr;
    errno = 0;
    int64_t bitrate = strtoll(bitrate_str, &endptr, 10);

    if (errno == ERANGE || bitrate < 0) {
      fprintf(stderr, "Invalid bitrate value: %s\n", bitrate_str);
      ret = AVERROR(EINVAL);
      goto fail;
    }

    if (*endptr == 'k' || *endptr == 'K') {
      /* Check for overflow before multiplication */
      if (bitrate > INT64_MAX / 1000) {
        fprintf(stderr, "Bitrate value too large: %s\n", bitrate_str);
        ret = AVERROR(EINVAL);
        goto fail;
      }
      bitrate *= 1000;
    }

    encoder->codec_ctx->bit_rate = bitrate;
  } else {
    /* Use quality preset */
    int bitrate = get_bitrate_for_quality(codec_name, quality);
    if (bitrate > 0) {
      encoder->codec_ctx->bit_rate = bitrate;
    } else {
      /* Lossless codec: set compression level */
      int level = get_compression_level(quality);
      av_opt_set_int(encoder->codec_ctx, "compression_level", level, 0);
    }
  }

  /* Set time base */
  encoder->codec_ctx->time_base = (AVRational){1, sample_rate};
  encoder->stream->time_base = encoder->codec_ctx->time_base;

  /* Some formats require global headers */
  if (encoder->fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    encoder->codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  /* Open the encoder */
  ret = avcodec_open2(encoder->codec_ctx, encoder->codec, NULL);
  if (ret < 0) {
    logerr("Failed to open encoder", ret);
    goto fail;
  }

  /* Copy encoder parameters to output stream */
  ret = avcodec_parameters_from_context(encoder->stream->codecpar,
                                        encoder->codec_ctx);
  if (ret < 0) {
    logerr("Failed to copy encoder parameters to stream", ret);
    goto fail;
  }

  /* Open the output file */
  if (!(encoder->fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
    ret = avio_open(&encoder->fmt_ctx->pb, filename, AVIO_FLAG_WRITE);
    if (ret < 0) {
      logerr("Failed to open output file", ret);
      goto fail;
    }
  }

  /* Write the stream header */
  ret = avformat_write_header(encoder->fmt_ctx, NULL);
  if (ret < 0) {
    logerr("Failed to write format header", ret);
    goto fail;
  }

  /* Allocate packet for encoded data */
  encoder->pkt = av_packet_alloc();
  if (!encoder->pkt) {
    fprintf(stderr, "Failed to allocate packet\n");
    ret = AVERROR(ENOMEM);
    goto fail;
  }

  /* Allocate audio FIFO buffer for frame size management */
  encoder->fifo = av_audio_fifo_alloc(encoder->codec_ctx->sample_fmt,
                                       encoder->codec_ctx->ch_layout.nb_channels,
                                       encoder->codec_ctx->frame_size);
  if (!encoder->fifo) {
    fprintf(stderr, "Failed to allocate audio FIFO\n");
    ret = AVERROR(ENOMEM);
    goto fail;
  }

  /* Initialize resampler for format conversion (input will be S16, might need conversion) */
  encoder->swr_ctx = swr_alloc();
  if (!encoder->swr_ctx) {
    fprintf(stderr, "Failed to allocate SwrContext\n");
    ret = AVERROR(ENOMEM);
    goto fail;
  }

  /* Configure resampler - we'll set input format dynamically when we receive the first frame */
  if ((ret = av_opt_set_chlayout(encoder->swr_ctx, "out_chlayout",
                                  &encoder->codec_ctx->ch_layout, 0)) < 0 ||
      (ret = av_opt_set_int(encoder->swr_ctx, "out_sample_rate",
                            encoder->codec_ctx->sample_rate, 0)) < 0 ||
      (ret = av_opt_set_sample_fmt(encoder->swr_ctx, "out_sample_fmt",
                                    encoder->codec_ctx->sample_fmt, 0)) < 0) {
    logerr("Failed to configure output SwrContext parameters", ret);
    goto fail;
  }

  encoder->pts = 0;
  return 0;

fail:
  audio_enc_free(encoder);
  return ret;
}

/**
 * @brief Helper function to encode a single frame from properly sized data.
 *
 * @param encoder Initialized audio_enc instance.
 * @param frame Frame to encode (must have correct nb_samples).
 * @return 0 on success, negative AVERROR code on failure.
 */
static int encode_frame(struct audio_enc *encoder, AVFrame *frame) {
  int ret;

  /* Set frame timestamp if frame is provided */
  if (frame) {
    frame->pts = encoder->pts;
    encoder->pts += frame->nb_samples;
  }

  /* Send frame to encoder */
  ret = avcodec_send_frame(encoder->codec_ctx, frame);
  if (ret < 0) {
    logerr("Error sending frame to encoder", ret);
    return ret;
  }

  /* Retrieve all available encoded packets */
  while (ret >= 0) {
    ret = avcodec_receive_packet(encoder->codec_ctx, encoder->pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      return 0; /* Need more input or flushing complete */
    } else if (ret < 0) {
      logerr("Error receiving packet from encoder", ret);
      return ret;
    }

    /* Set packet stream index and rescale timestamps */
    encoder->pkt->stream_index = encoder->stream->index;
    av_packet_rescale_ts(encoder->pkt, encoder->codec_ctx->time_base,
                         encoder->stream->time_base);

    /* Write the compressed packet to the output file */
    ret = av_interleaved_write_frame(encoder->fmt_ctx, encoder->pkt);
    if (ret < 0) {
      logerr("Error writing packet to output file", ret);
      av_packet_unref(encoder->pkt);
      return ret;
    }

    av_packet_unref(encoder->pkt);
  }

  return 0;
}

/**
 * @brief Read samples from FIFO and encode frames of correct size.
 *
 * @param encoder Initialized audio_enc instance.
 * @param finish If true, encode all remaining samples (for flushing).
 * @return 0 on success, negative AVERROR code on failure.
 */
static int encode_from_fifo(struct audio_enc *encoder, int finish) {
  int ret;
  int frame_size = encoder->codec_ctx->frame_size;

  /* Encode all complete frames in the FIFO */
  while (av_audio_fifo_size(encoder->fifo) >= frame_size ||
         (finish && av_audio_fifo_size(encoder->fifo) > 0)) {

    int samples_to_read = av_audio_fifo_size(encoder->fifo);
    if (samples_to_read > frame_size)
      samples_to_read = frame_size;

    /* Allocate frame for encoded data */
    AVFrame *output_frame = av_frame_alloc();
    if (!output_frame)
      return AVERROR(ENOMEM);

    output_frame->nb_samples = samples_to_read;
    output_frame->format = encoder->codec_ctx->sample_fmt;
    ret = av_channel_layout_copy(&output_frame->ch_layout,
                                  &encoder->codec_ctx->ch_layout);
    if (ret < 0) {
      av_frame_free(&output_frame);
      return ret;
    }

    /* Allocate buffer for frame data */
    ret = av_frame_get_buffer(output_frame, 0);
    if (ret < 0) {
      av_frame_free(&output_frame);
      return ret;
    }

    /* Read samples from FIFO into frame */
    ret = av_audio_fifo_read(encoder->fifo, (void **)output_frame->data,
                              samples_to_read);
    if (ret < 0) {
      av_frame_free(&output_frame);
      return ret;
    }

    /* Encode the frame */
    ret = encode_frame(encoder, output_frame);
    av_frame_free(&output_frame);

    if (ret < 0)
      return ret;
  }

  return 0;
}

int audio_enc_write_frame(struct audio_enc *encoder, AVFrame *frame) {
  int ret;

  if (!encoder || !encoder->codec_ctx) {
    fprintf(stderr, "Encoder not initialized\n");
    return AVERROR(EINVAL);
  }

  if (frame) {
    /* Initialize SwrContext with input format on first frame */
    if (!swr_is_initialized(encoder->swr_ctx)) {
      if ((ret = av_opt_set_chlayout(encoder->swr_ctx, "in_chlayout",
                                      &frame->ch_layout, 0)) < 0 ||
          (ret = av_opt_set_int(encoder->swr_ctx, "in_sample_rate",
                                frame->sample_rate, 0)) < 0 ||
          (ret = av_opt_set_sample_fmt(encoder->swr_ctx, "in_sample_fmt",
                                        frame->format, 0)) < 0) {
        logerr("Failed to configure input SwrContext parameters", ret);
        return ret;
      }

      ret = swr_init(encoder->swr_ctx);
      if (ret < 0) {
        logerr("Failed to initialize SwrContext", ret);
        return ret;
      }
    }

    /* Convert frame to encoder format */
    int dst_nb_samples = av_rescale_rnd(
        swr_get_delay(encoder->swr_ctx, frame->sample_rate) +
            frame->nb_samples,
        encoder->codec_ctx->sample_rate, frame->sample_rate, AV_ROUND_UP);

    uint8_t **converted = NULL;
    ret = av_samples_alloc_array_and_samples(
        &converted, NULL, encoder->codec_ctx->ch_layout.nb_channels,
        dst_nb_samples, encoder->codec_ctx->sample_fmt, 0);
    if (ret < 0) {
      logerr("Failed to allocate conversion buffer", ret);
      return ret;
    }

    int samples_converted =
        swr_convert(encoder->swr_ctx, converted, dst_nb_samples,
                    (const uint8_t **)frame->data, frame->nb_samples);
    if (samples_converted < 0) {
      logerr("Error during resampling", samples_converted);
      av_freep(&converted[0]);
      av_freep(&converted);
      return samples_converted;
    }

    /* Write converted samples to FIFO */
    ret = av_audio_fifo_write(encoder->fifo, (void **)converted,
                               samples_converted);
    av_freep(&converted[0]);
    av_freep(&converted);

    if (ret < samples_converted) {
      fprintf(stderr, "Failed to write samples to FIFO\n");
      return AVERROR(ENOMEM);
    }

    /* Encode complete frames from FIFO */
    return encode_from_fifo(encoder, 0);
  } else {
    /* Flush: encode remaining samples and flush encoder */
    ret = encode_from_fifo(encoder, 1);
    if (ret < 0)
      return ret;

    /* Flush encoder */
    return encode_frame(encoder, NULL);
  }
}

int audio_enc_finalize(struct audio_enc *encoder) {
  int ret;

  if (!encoder || !encoder->fmt_ctx) {
    fprintf(stderr, "Encoder not initialized\n");
    return AVERROR(EINVAL);
  }

  /* Flush the encoder by sending NULL frame */
  ret = audio_enc_write_frame(encoder, NULL);
  if (ret < 0) {
    fprintf(stderr, "Error flushing encoder\n");
    return ret;
  }

  /* Write the trailer */
  ret = av_write_trailer(encoder->fmt_ctx);
  if (ret < 0) {
    logerr("Failed to write trailer", ret);
    return ret;
  }

  return 0;
}

void audio_enc_free(struct audio_enc *encoder) {
  if (!encoder)
    return;

  /* Close output file */
  if (encoder->fmt_ctx && !(encoder->fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
    avio_closep(&encoder->fmt_ctx->pb);
  }

  /* Free audio FIFO */
  if (encoder->fifo)
    av_audio_fifo_free(encoder->fifo);

  /* Free resampler context */
  if (encoder->swr_ctx)
    swr_free(&encoder->swr_ctx);

  /* Free packet */
  if (encoder->pkt)
    av_packet_free(&encoder->pkt);

  /* Free encoder context */
  if (encoder->codec_ctx)
    avcodec_free_context(&encoder->codec_ctx);

  /* Free format context */
  if (encoder->fmt_ctx)
    avformat_free_context(encoder->fmt_ctx);

  memset(encoder, 0, sizeof(*encoder));
}
