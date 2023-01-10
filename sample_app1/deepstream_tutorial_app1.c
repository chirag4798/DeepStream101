
/*******************************************************************************
 * Blah Blah Blah License
 * You are free to play around and publish the code anywhere you want.
 * Author: Chirag Shetty
 *******************************************************************************/

#include <gst/gst.h>
#include <glib.h>
#include <stdio.h>
#include <cuda_runtime_api.h>
#include "gstnvdsmeta.h"

/* The muxer output resolution must be set if the input streams will be of
 * different resolution. The muxer will scale all the input frames to this
 * resolution. */
#define MUXER_OUTPUT_WIDTH 1920
#define MUXER_OUTPUT_HEIGHT 1080

/* Muxer batch formation timeout, for e.g. 40 millisec. Should ideally be set
 * based on the fastest source's framerate. */
#define MUXER_BATCH_TIMEOUT_USEC 40000

static gboolean bus_call (GstBus * bus, GstMessage * msg, gpointer data){
  GMainLoop *loop = (GMainLoop *) data;
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_ERROR:{
      gchar *debug;
      GError *error;
      gst_message_parse_error (msg, &error, &debug);
      g_printerr ("ERROR from element %s: %s\n",
          GST_OBJECT_NAME (msg->src), error->message);
      if (debug)
        g_printerr ("Error details: %s\n", debug);
      g_free (debug);
      g_error_free (error);
      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }
  return TRUE;
}

int main (int argc, char *argv[]){
  GMainLoop *loop = NULL;
  GstElement *pipeline = NULL, *source = NULL, *h264parser = NULL, *nvv4l2h264enc = NULL,
             *nvv4l2decoder = NULL, *streammux = NULL, *sink = NULL, *nvvidconv = NULL, *qtmux = NULL,
             *pgie = NULL, *tracker = NULL, *nvvidconv2 = NULL, *nvosd = NULL, *h264parser2 = NULL;

  GstElement *transform = NULL;
  GstBus *bus = NULL;
  guint bus_watch_id;

  /* Check input arguments */
  if (argc != 2) {
    g_printerr ("Usage: %s </path/to/input/video.mp4>\n", argv[0]);
    return -1;
  }

  /* Standard GStreamer initialization */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  /* Create gstreamer elements */
  /* Create Pipeline element that will form a connection of other elements */
  pipeline = gst_pipeline_new ("deepstream_tutorial_app1");

  /* Input File source element */
  source = gst_element_factory_make ("filesrc", "file-source");

  /* Since the data format in the input file is elementary h264 stream,
   * we need a h264parser */
  h264parser = gst_element_factory_make ("h264parse", "h264-parser");

  /* Use nvdec_h264 for hardware accelerated decode on GPU */
  nvv4l2decoder = gst_element_factory_make ("nvv4l2decoder", "nvv4l2-decoder");

  /* Create nvstreammux instance to form batches from one or more sources. */
  streammux = gst_element_factory_make ("nvstreammux", "stream-muxer");

  /* Use nvinfer to run inferencing on decoder's output,
   * behaviour of inferencing is set through config file */
  pgie = gst_element_factory_make ("nvinfer", "primary-nvinference-engine");

  /* Assigns track ids to detected bounding boxes*/
  tracker = gst_element_factory_make ("nvtracker", "tracker");

  /* Use convertor to convert from NV12 to RGBA as required by nvosd */
  nvvidconv = gst_element_factory_make ("nvvideoconvert", "nvvideo-converter");

  /* Create OSD to draw on the converted RGBA buffer */
  nvosd = gst_element_factory_make ("nvdsosd", "nv-onscreendisplay");

  /* Use convertor to convert from NV12 to RGBA as required by nvosd */
  nvvidconv2 = gst_element_factory_make ("nvvideoconvert", "nvvideo-converter2");

  /* Use convertor to convert from NV12 to H264 as required */
  nvv4l2h264enc = gst_element_factory_make ("nvv4l2h264enc", "nvv4l2h264enc");

  /* Since the data format for the output file is elementary h264 stream,
   * we need a h264parser */
  h264parser2 = gst_element_factory_make ("h264parse", "h264parser2");

  qtmux = gst_element_factory_make ("qtmux", "qtmux");

  sink = gst_element_factory_make ("filesink", "filesink");

  if (!pipeline || !source || !h264parser || 
      !nvv4l2decoder || !streammux || !pgie ||
      !tracker || !nvvidconv || !nvosd || !nvvidconv2 || 
      !nvv4l2h264enc || !h264parser2 || !qtmux || !sink) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }

  /* we set the input filename to the source element */
  g_object_set (
    G_OBJECT (source), 
    "location", 
    argv[1], 
    NULL
  );

  g_object_set (
    G_OBJECT (streammux), 
    "batch-size", 
    1, 
    "width", 
    MUXER_OUTPUT_WIDTH, 
    "height",
    MUXER_OUTPUT_HEIGHT,
    "batched-push-timeout", 
    MUXER_BATCH_TIMEOUT_USEC, NULL
  );

  /* Set all the necessary properties of the nvinfer element,
  * the necessary ones are : */
  g_object_set (
      G_OBJECT (pgie),
      "config-file-path", 
      "/opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test1/dstest1_pgie_config.txt", 
      NULL
    );

  /* Set all the necessary properties of the nvtracker element,
  * the necessary ones are : */
  g_object_set (
      G_OBJECT (tracker),
      "ll-lib-file", 
      "/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so", 
      NULL
    );

  /* Set output file location */
  g_object_set (
      G_OBJECT (sink),
      "location", 
      "output.mp4", 
      NULL
    );

  /* we add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  /* Set up the pipeline */
  /* we add all elements into the pipeline */
  gst_bin_add_many (
    GST_BIN (pipeline),
    source,
    h264parser,
    nvv4l2decoder,
    streammux, 
    pgie,
    tracker,
    nvvidconv, 
    nvosd,
    nvvidconv2,
    nvv4l2h264enc,
    h264parser2,
    qtmux,
    sink, 
    NULL
  );


  GstPad *sinkpad, *srcpad;
  gchar pad_name_sink[16] = "sink_0";
  gchar pad_name_src[16] = "src";

  sinkpad = gst_element_get_request_pad (streammux, pad_name_sink);
  if (!sinkpad) {
    g_printerr ("Streammux request sink pad failed. Exiting.\n");
    return -1;
  }

  srcpad = gst_element_get_static_pad (nvv4l2decoder, pad_name_src);
  if (!srcpad) {
    g_printerr ("Decoder request src pad failed. Exiting.\n");
    return -1;
  }

  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
      g_printerr ("Failed to link decoder to stream muxer. Exiting.\n");
      return -1;
  }

  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  /* we link the elements together */
  /* file-source -> h264-parser -> nvh264-decoder ->
   * nvinfer -> nvvidconv -> nvosd -> video-renderer */
  if (!gst_element_link_many (source, h264parser, nvv4l2decoder, NULL)) {
    g_printerr ("Elements could not be linked: 1. Exiting.\n");
    return -1;
  }

  if (!gst_element_link_many (streammux, pgie, tracker, nvvidconv, nvosd, nvvidconv2, nvv4l2h264enc, h264parser2, qtmux, sink, NULL)) {
    g_printerr ("Elements could not be linked: 2. Exiting.\n");
    return -1;
  }

  /* Set the pipeline to "playing" state */
  g_print ("Using file: %s\n", argv[1]);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Wait till pipeline encounters an error or EOS */
  g_print ("Running...\n");
  g_main_loop_run (loop);

  /* Out of the main loop, clean up nicely */
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_print ("Deleting pipeline\n");
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
  return 0;
}