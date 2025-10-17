#include "include/audio_dec.h"
#include "include/audio_enc.h"
#include "include/audio_filter.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Print version information.
 */
static void print_version(void) {
  printf("audx version %s\n", AUDX_VERSION);
  printf("A flexible audio transcoding tool built on FFmpeg\n\n");

  printf("FFmpeg libraries:\n");
  printf("  libavcodec     %u.%u.%u\n",
         LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO);
  printf("  libavformat    %u.%u.%u\n",
         LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO);
  printf("  libavutil      %u.%u.%u\n",
         LIBAVUTIL_VERSION_MAJOR, LIBAVUTIL_VERSION_MINOR, LIBAVUTIL_VERSION_MICRO);
  printf("  libavfilter    %u.%u.%u\n",
         LIBAVFILTER_VERSION_MAJOR, LIBAVFILTER_VERSION_MINOR, LIBAVFILTER_VERSION_MICRO);
  printf("  libswresample  %u.%u.%u\n\n",
         LIBSWRESAMPLE_VERSION_MAJOR, LIBSWRESAMPLE_VERSION_MINOR, LIBSWRESAMPLE_VERSION_MICRO);

  printf("License:\n");
  printf("  audx: MIT License\n");
  printf("  FFmpeg libraries: LGPL 2.1 or later\n");
  printf("  See LEGAL_NOTICES.md for details\n\n");

  printf("FFmpeg source: https://ffmpeg.org/\n");
}

/**
 * @brief Print usage information for audx.
 */
static void print_usage(const char *prog_name) {
  fprintf(stderr, "Usage: %s <input> <output> [OPTIONS]\n\n", prog_name);
  fprintf(stderr, "OPTIONS:\n");
  fprintf(stderr, "  --codec=<name>       Encoder codec (libmp3lame, aac, libopus, flac, alac, pcm_s16le)\n");
  fprintf(stderr, "  --quality=<preset>   Quality preset: low, medium, high, extreme (default: high)\n");
  fprintf(stderr, "  --bitrate=<rate>     Explicit bitrate (e.g., 192k, 320k) - overrides quality\n");
  fprintf(stderr, "  --filter=<desc>      FFmpeg filter chain (e.g., \"atempo=1.25,volume=0.5\")\n");
  fprintf(stderr, "  -h, --help           Show this help message\n");
  fprintf(stderr, "  -v, --version        Show version information\n\n");
  fprintf(stderr, "EXAMPLES:\n");
  fprintf(stderr, "  %s input.mp3 output.opus --codec=libopus --quality=high\n", prog_name);
  fprintf(stderr, "  %s input.mp3 output.mp3 --codec=libmp3lame --bitrate=320k --filter=\"atempo=1.25\"\n", prog_name);
  fprintf(stderr, "  %s input.flac output.pcm (raw PCM output, no codec needed)\n\n", prog_name);
}

/**
 * @brief Parse quality preset string to enum value.
 */
static enum audio_quality parse_quality(const char *quality_str) {
  if (!quality_str)
    return AUDIO_QUALITY_HIGH;
  if (strcmp(quality_str, "low") == 0)
    return AUDIO_QUALITY_LOW;
  if (strcmp(quality_str, "medium") == 0)
    return AUDIO_QUALITY_MEDIUM;
  if (strcmp(quality_str, "high") == 0)
    return AUDIO_QUALITY_HIGH;
  if (strcmp(quality_str, "extreme") == 0)
    return AUDIO_QUALITY_EXTREME;
  return AUDIO_QUALITY_HIGH;
}

int main(int argc, char *argv[]) {
  /* Check for --help/-h or --version/-v flags */
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      print_usage(argv[0]);
      return 0;
    }
    if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
      print_version();
      return 0;
    }
  }

  if (argc < 3) {
    print_usage(argv[0]);
    return 1;
  }

  const char *input_filename = argv[1];
  const char *output_filename = argv[2];
  const char *codec_name = NULL;
  const char *quality_str = NULL;
  const char *bitrate_str = NULL;
  const char *filter_desc = NULL;

  /* Parse command-line arguments */
  for (int i = 3; i < argc; i++) {
    if (strncmp(argv[i], "--codec=", 8) == 0) {
      codec_name = argv[i] + 8;
    } else if (strncmp(argv[i], "--quality=", 10) == 0) {
      quality_str = argv[i] + 10;
    } else if (strncmp(argv[i], "--bitrate=", 10) == 0) {
      bitrate_str = argv[i] + 10;
    } else if (strncmp(argv[i], "--filter=", 9) == 0) {
      filter_desc = argv[i] + 9;
    } else {
      /* Backward compatibility: treat positional arg as filter */
      if (!filter_desc)
        filter_desc = argv[i];
    }
  }

  enum audio_quality quality = parse_quality(quality_str);
  int use_encoder = (codec_name != NULL);
  int use_filter = (filter_desc != NULL && filter_desc[0] != '\0');

  struct audio_dec decoder;
  struct audio_filter filter;
  struct audio_enc encoder;
  FILE *output_file = NULL;

  /* Initialize decoder */
  int ret = audio_dec_init(&decoder, input_filename);
  if (ret < 0) {
    fprintf(stderr, "Failed to initialize decoder\n");
    return 1;
  }

  printf("Audio stream info\n");
  printf("  Sample rate : %d Hz\n", decoder.sample_rate);
  printf("  Channels    : %d\n", decoder.channels);

  /* Initialize filter if specified */
  if (use_filter) {
    ret = audio_filter_init(&filter, decoder.sample_rate, decoder.dst_fmt,
                            &decoder.dst_ch_layout, filter_desc);
    if (ret < 0) {
      fprintf(stderr, "Failed to initialize filter\n");
      audio_dec_free(&decoder);
      return 1;
    }
    printf("Applying filter: %s\n", filter_desc);
  }

  /* Initialize encoder or open raw PCM file */
  if (use_encoder) {
    ret = audio_enc_init(&encoder, output_filename, codec_name,
                         decoder.sample_rate, &decoder.dst_ch_layout, quality,
                         bitrate_str);
    if (ret < 0) {
      fprintf(stderr, "Failed to initialize encoder\n");
      if (use_filter)
        audio_filter_free(&filter);
      audio_dec_free(&decoder);
      return 1;
    }
    printf("Encoding to: %s (codec: %s)\n", output_filename, codec_name);
  } else {
    output_file = fopen(output_filename, "wb");
    if (!output_file) {
      perror("Failed to open output file");
      if (use_filter)
        audio_filter_free(&filter);
      audio_dec_free(&decoder);
      return 1;
    }
    printf("Writing raw PCM to: %s\n", output_filename);
  }

  /* Main decode/filter/encode loop */
  uint8_t *data = NULL;
  int size = 0;

  while (audio_decoder_read(&decoder, &data, &size)) {
    if (data && size > 0) {
      AVFrame *frame = av_frame_alloc();
      if (!frame) {
        fprintf(stderr, "Failed to allocate frame\n");
        break;
      }

      /* Fill frame with decoded PCM data */
      frame->nb_samples =
          size / (decoder.channels * av_get_bytes_per_sample(decoder.dst_fmt));
      frame->format = decoder.dst_fmt;
      frame->sample_rate = decoder.sample_rate;
      av_channel_layout_copy(&frame->ch_layout, &decoder.dst_ch_layout);

      ret = avcodec_fill_audio_frame(frame, decoder.channels, decoder.dst_fmt,
                                     data, size, 1);
      if (ret < 0) {
        fprintf(stderr, "Error filling frame\n");
        av_frame_free(&frame);
        av_freep(&data);
        continue;
      }

      if (use_filter) {
        /* Push frame to filter */
        ret = audio_filter_push(&filter, frame);
        av_frame_free(&frame);
        av_freep(&data);

        if (ret < 0) {
          fprintf(stderr, "Error pushing frame to filter\n");
          continue;
        }

        /* Pull and process filtered frames */
        AVFrame *filtered_frame = NULL;
        while ((ret = audio_filter_pull(&filter, &filtered_frame)) >= 0) {
          if (use_encoder) {
            /* Encode filtered frame */
            ret = audio_enc_write_frame(&encoder, filtered_frame);
            if (ret < 0)
              fprintf(stderr, "Error encoding frame\n");
          } else {
            /* Write filtered PCM to file */
            int buf_size = av_samples_get_buffer_size(
                NULL, filtered_frame->ch_layout.nb_channels,
                filtered_frame->nb_samples, filtered_frame->format, 1);
            fwrite(filtered_frame->data[0], 1, buf_size, output_file);
          }
          av_frame_free(&filtered_frame);
        }
      } else {
        /* No filter: encode or write directly */
        if (use_encoder) {
          ret = audio_enc_write_frame(&encoder, frame);
          if (ret < 0)
            fprintf(stderr, "Error encoding frame\n");
        } else {
          fwrite(data, 1, size, output_file);
        }
        av_frame_free(&frame);
        av_freep(&data);
      }
    }
  }

  /* Finalize encoding or close PCM file */
  if (use_encoder) {
    audio_enc_finalize(&encoder);
    audio_enc_free(&encoder);
  } else {
    fclose(output_file);
  }

  /* Cleanup */
  if (use_filter)
    audio_filter_free(&filter);
  audio_dec_free(&decoder);

  printf("Finished. Output written to %s\n", output_filename);
  return 0;
}
