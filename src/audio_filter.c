#include "../include/audio_filter.h"
#include <stdio.h>

/**
 * @brief Initialize an audio filter graph with the specified parameters.
 *
 * This function constructs an FFmpeg filter graph for processing audio frames.
 * It creates a chain: abuffer -> [user filters] -> abuffersink.
 * The implementation uses the FFmpeg 8.0+ API which requires allocating filter
 * contexts, setting parameters via av_opt_set functions, then initializing
 * each filter explicitly.
 *
 * Implementation details (FFmpeg 8.0+ API):
 * - Uses avfilter_graph_alloc_filter() instead of avfilter_graph_create_filter()
 * - Sets parameters using av_opt_set() family of functions
 * - Explicitly calls avfilter_init_str() after setting all options
 * - Uses "channel_layout" parameter name (not "ch_layout")
 *
 * @param filter Pointer to audio_filter struct to initialize.
 * @param sample_rate Input audio sample rate in Hz (e.g., 44100).
 * @param format Input audio sample format (e.g., AV_SAMPLE_FMT_S16).
 * @param ch_layout Input channel layout structure describing speaker arrangement.
 * @param filter_desc FFmpeg filter description string (e.g., "atempo=1.25,volume=0.5").
 * @return 0 on success, negative AVERROR code on failure.
 */
int audio_filter_init(struct audio_filter *filter, int sample_rate,
                      enum AVSampleFormat format,
                      const AVChannelLayout *ch_layout,
                      const char *filter_desc) {
  int ret;
  char layout_str[128];

  /* Validate input channel layout */
  if (!ch_layout || ch_layout->nb_channels == 0) {
    fprintf(stderr, "Invalid channel layout provided to audio filter\n");
    return AVERROR(EINVAL);
  }

  /* Convert channel layout to string representation (e.g., "stereo", "5.1") */
  if (av_channel_layout_describe(ch_layout, layout_str, sizeof(layout_str)) <
      0) {
    snprintf(layout_str, sizeof(layout_str), "stereo");
  }

  /* Get sample format name string (e.g., "s16", "fltp") */
  const char *fmt_name = av_get_sample_fmt_name(format);
  if (!fmt_name) {
    fprintf(stderr, "Invalid sample format: %d\n", format);
    return AVERROR(EINVAL);
  }

  /* Retrieve abuffer (source) and abuffersink (sink) filter definitions */
  const AVFilter *src = avfilter_get_by_name("abuffer");
  const AVFilter *sink = avfilter_get_by_name("abuffersink");

  if (!src || !sink) {
    fprintf(stderr, "Failed to find audio buffer filters.\n");
    return AVERROR_FILTER_NOT_FOUND;
  }

  /* Allocate the filter graph container */
  AVFilterGraph *graph = avfilter_graph_alloc();
  if (!graph)
    return AVERROR(ENOMEM);

  AVFilterContext *src_ctx = NULL;
  AVFilterContext *sink_ctx = NULL;

  /* Allocate input/output link structures for filter chain connection */
  AVFilterInOut *outputs = avfilter_inout_alloc();
  AVFilterInOut *inputs = avfilter_inout_alloc();

  /*
   * Step 1: Create and configure the abuffer source filter.
   * This filter feeds audio frames into the filter graph.
   * FFmpeg 8.0+ requires:
   *   1. Allocate filter context
   *   2. Set parameters using av_opt_set functions
   *   3. Initialize with avfilter_init_str
   */
  src_ctx = avfilter_graph_alloc_filter(graph, src, "in");
  if (!src_ctx) {
    fprintf(stderr, "Failed to allocate abuffer filter\n");
    ret = AVERROR(ENOMEM);
    goto fail;
  }

  /* Configure abuffer parameters: channel_layout, sample_fmt, sample_rate, time_base */
  if ((ret = av_opt_set(src_ctx, "channel_layout", layout_str, AV_OPT_SEARCH_CHILDREN)) < 0) {
    fprintf(stderr, "Failed to set channel_layout: %s\n", av_err2str(ret));
    goto fail;
  }
  if ((ret = av_opt_set(src_ctx, "sample_fmt", fmt_name, AV_OPT_SEARCH_CHILDREN)) < 0) {
    fprintf(stderr, "Failed to set sample_fmt: %s\n", av_err2str(ret));
    goto fail;
  }
  if ((ret = av_opt_set_int(src_ctx, "sample_rate", sample_rate, AV_OPT_SEARCH_CHILDREN)) < 0) {
    fprintf(stderr, "Failed to set sample_rate: %s\n", av_err2str(ret));
    goto fail;
  }
  if ((ret = av_opt_set_q(src_ctx, "time_base", (AVRational){1, sample_rate}, AV_OPT_SEARCH_CHILDREN)) < 0) {
    fprintf(stderr, "Failed to set time_base: %s\n", av_err2str(ret));
    goto fail;
  }

  /* Initialize the abuffer filter after all parameters are set */
  ret = avfilter_init_str(src_ctx, NULL);
  if (ret < 0) {
    fprintf(stderr, "Failed to initialize abuffer: %s\n", av_err2str(ret));
    goto fail;
  }

  /*
   * Step 2: Create and configure the abuffersink output filter.
   * This filter receives processed frames from the filter graph.
   */
  sink_ctx = avfilter_graph_alloc_filter(graph, sink, "out");
  if (!sink_ctx) {
    fprintf(stderr, "Failed to allocate abuffersink filter\n");
    ret = AVERROR(ENOMEM);
    goto fail;
  }

  /* Initialize the abuffersink filter (no parameters needed) */
  ret = avfilter_init_str(sink_ctx, NULL);
  if (ret < 0) {
    fprintf(stderr, "Failed to initialize abuffersink: %s\n", av_err2str(ret));
    goto fail;
  }

  /* Validate input/output link allocation */
  if (!outputs || !inputs) {
    ret = AVERROR(ENOMEM);
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    goto fail;
  }

  /*
   * Step 3: Set up filter chain endpoints.
   * outputs points to the source (abuffer)
   * inputs points to the sink (abuffersink)
   */
  outputs->name = av_strdup("in");
  if (!outputs->name) {
    ret = AVERROR(ENOMEM);
    avfilter_inout_free(&outputs);
    avfilter_inout_free(&inputs);
    goto fail;
  }
  outputs->filter_ctx = src_ctx;
  outputs->pad_idx = 0;
  outputs->next = NULL;

  inputs->name = av_strdup("out");
  if (!inputs->name) {
    ret = AVERROR(ENOMEM);
    avfilter_inout_free(&outputs);
    avfilter_inout_free(&inputs);
    goto fail;
  }
  inputs->filter_ctx = sink_ctx;
  inputs->pad_idx = 0;
  inputs->next = NULL;

  /*
   * Step 4: Parse the user-provided filter description and connect the graph.
   * This creates the intermediate filters (e.g., atempo, volume) and links
   * them between the source and sink.
   */
  ret = avfilter_graph_parse_ptr(graph, filter_desc, &inputs, &outputs, NULL);
  if (ret < 0) {
    fprintf(stderr, "graph_parse_ptr failed: %s\n", av_err2str(ret));
    goto fail;
  }

  /*
   * Step 5: Finalize the filter graph configuration.
   * This validates connections and negotiates formats between filters.
   */
  ret = avfilter_graph_config(graph, NULL);
  if (ret < 0) {
    fprintf(stderr, "graph_config failed: %s\n", av_err2str(ret));
    goto fail;
  }

  /* Store the initialized graph and filter contexts in the output structure */
  memset(filter, 0, sizeof(*filter));
  filter->graph = graph;
  filter->src_ctx = src_ctx;
  filter->sink_ctx = sink_ctx;

  return 0;

fail:
  fprintf(stderr, "Audio filter init failed: %s\n", av_err2str(ret));
  if (inputs != NULL)
    avfilter_inout_free(&inputs);
  if (outputs != NULL)
    avfilter_inout_free(&outputs);
  avfilter_graph_free(&graph);
  return ret;
}

/**
 * @brief Push an audio frame into the filter graph for processing.
 *
 * Sends a decoded audio frame to the abuffer source filter. The frame
 * will be processed through the filter chain (e.g., tempo adjustment,
 * volume changes, etc.) and can be retrieved using audio_filter_pull.
 *
 * The AV_BUFFERSRC_FLAG_KEEP_REF flag ensures the original frame data
 * is not modified, allowing the caller to continue using the frame
 * after this call if needed.
 *
 * @param filter Pointer to initialized audio_filter structure.
 * @param frame Audio frame to process (must be valid and contain audio data).
 * @return 0 on success, negative AVERROR code on failure.
 */
int audio_filter_push(struct audio_filter *filter, AVFrame *frame) {
  return av_buffersrc_add_frame_flags(filter->src_ctx, frame,
                                      AV_BUFFERSRC_FLAG_KEEP_REF);
}

/**
 * @brief Pull a processed audio frame from the filter graph.
 *
 * Retrieves one filtered audio frame from the abuffersink output filter.
 * The function allocates a new frame structure and fills it with the
 * processed audio data. The caller is responsible for freeing the frame
 * using av_frame_free when finished.
 *
 * This function may be called multiple times after a single push operation,
 * as some filters (e.g., atempo) can produce multiple output frames from
 * one input frame.
 *
 * @param filter Pointer to initialized audio_filter structure.
 * @param out_frame Pointer to receive the allocated filtered frame.
 * @return 0 on success, AVERROR(EAGAIN) if more input is needed,
 *         AVERROR_EOF at end of stream, or other negative AVERROR on failure.
 */
int audio_filter_pull(struct audio_filter *filter, AVFrame **out_frame) {
  *out_frame = av_frame_alloc();
  if (*out_frame == NULL) {
    return AVERROR(ENOMEM);
  }

  int ret = av_buffersink_get_frame(filter->sink_ctx, *out_frame);
  if (ret < 0) {
    av_frame_free(out_frame);
    *out_frame = NULL;
  }
  return ret;
}

/**
 * @brief Free all resources associated with an audio filter graph.
 *
 * Releases the filter graph and all associated filter contexts. This should
 * be called when audio filtering is complete to avoid memory leaks.
 * After calling this function, the filter structure should not be used
 * unless reinitialized with audio_filter_init.
 *
 * The function safely handles NULL or already-freed filter graphs, so it
 * can be called multiple times without issue.
 *
 * @param filter Pointer to audio_filter structure to free.
 */
void audio_filter_free(struct audio_filter *filter) {
  if (filter->graph) {
    avfilter_graph_free(&filter->graph);
    filter->graph = NULL;
    filter->src_ctx = NULL;
    filter->sink_ctx = NULL;
  }
}
