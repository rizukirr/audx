#ifndef AUDIO_FILTER_H
#define AUDIO_FILTER_H

#define __STDC_CONSTANT_MACROS

#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>

struct audio_filter {
  AVFilterGraph *graph;
  AVFilterContext *src_ctx;
  AVFilterContext *sink_ctx;
};

/**
 * Initialize audio filter with a simple filter description (e.g. atempo,
 * asetrate).
 *
 * @param filter   Pointer to filter struct.
 * @param sample_rate Input audio sample rate.
 * @param format   Audio sample format.
 * @param ch_layout Audio channel layout.
 * @param filter_desc FFmpeg filter string (e.g. "atempo=1.2,aresample=44100").
 * @return 0 on success, negative AVERROR on failure.
 */
int audio_filter_init(struct audio_filter *filter, int sample_rate,
                      enum AVSampleFormat format,
                      const AVChannelLayout *ch_layout,
                      const char *filter_desc);

/**
 * Push a decoded audio frame into the filter.
 */
int audio_filter_push(struct audio_filter *filter, AVFrame *frame);

/**
 * Pull a filtered frame out of the filter.
 * Caller must free the frame with av_frame_free().
 */
int audio_filter_pull(struct audio_filter *filter, AVFrame **out_frame);

/**
 * Free and cleanup all filter resources.
 */
void audio_filter_free(struct audio_filter *filter);

#endif // AUDIO_FILTER_H
