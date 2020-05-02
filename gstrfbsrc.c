/* GStreamer
 * Copyright (C) <2007> Thijs Vermeir <thijsvermeir@gmail.com>
 * Copyright (C) <2006> Andre Moreira Magalhaes <andre.magalhaes@indt.org.br>
 * Copyright (C) <2004> David A. Schleef <ds@schleef.org>
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/* TODOS
 *
 * lots of stuff
 * - fix logging
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstrfbsrc.h"

#include <gst/video/video.h>

#include <string.h>
#include <stdlib.h>
#ifdef HAVE_X11
#include <X11/Xlib.h>
#endif

enum
{
  PROP_0,
  PROP_HOST,
  PROP_PORT,
  PROP_USER,
  PROP_PASSWORD,
  PROP_OFFSET_X,
  PROP_OFFSET_Y,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_SHARED,
};

GST_DEBUG_CATEGORY_STATIC (rfbsrc_debug);
GST_DEBUG_CATEGORY (rfbdecoder_debug);
#define GST_CAT_DEFAULT rfbsrc_debug

static GstStaticPadTemplate gst_rfb_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGB")
        "; " GST_VIDEO_CAPS_MAKE ("BGR")
        "; " GST_VIDEO_CAPS_MAKE ("RGBx")
        "; " GST_VIDEO_CAPS_MAKE ("BGRx")
        "; " GST_VIDEO_CAPS_MAKE ("xRGB")
        "; " GST_VIDEO_CAPS_MAKE ("xBGR")));

static void gst_rfb_src_finalize (GObject * object);
static void gst_rfb_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_rfb_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_rfb_src_negotiate (GstBaseSrc * bsrc);
static gboolean gst_rfb_src_stop (GstBaseSrc * bsrc);
static gboolean gst_rfb_src_event (GstBaseSrc * bsrc, GstEvent * event);
static gboolean gst_rfb_src_decide_allocation (GstBaseSrc * bsrc,
    GstQuery * query);
static GstFlowReturn gst_rfb_src_fill (GstPushSrc * psrc, GstBuffer * outbuf);

#define gst_rfb_src_parent_class parent_class
G_DEFINE_TYPE (GstRfbSrc, gst_rfb_src, GST_TYPE_PUSH_SRC);

static void
gst_rfb_src_class_init (GstRfbSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstElementClass *gstelement_class;
  GstPushSrcClass *gstpushsrc_class;


  GST_DEBUG_CATEGORY_INIT (rfbsrc_debug, "rfbsrc", 0, "rfb src element");
  GST_DEBUG_CATEGORY_INIT (rfbdecoder_debug, "rfbdecoder", 0, "rfb decoder");

  gobject_class = (GObjectClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpushsrc_class = (GstPushSrcClass *) klass;

  gobject_class->finalize = gst_rfb_src_finalize;
  gobject_class->set_property = gst_rfb_src_set_property;
  gobject_class->get_property = gst_rfb_src_get_property;

  g_object_class_install_property (gobject_class, PROP_HOST,
      g_param_spec_string ("host", "Host to connect to", "Host to connect to",
          "127.0.0.1", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PORT,
      g_param_spec_int ("port", "Port", "Port",
          1, 65535, 5900, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_USER ,
      g_param_spec_string ("user", "Username for authentication",
          "Username for authentication", "",
          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PASSWORD,
      g_param_spec_string ("password", "Password for authentication",
          "Password for authentication", "",
          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OFFSET_X,
      g_param_spec_int ("offset-x", "x offset for screen scrapping",
          "x offset for screen scrapping", 0, 65535, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OFFSET_Y,
      g_param_spec_int ("offset-y", "y offset for screen scrapping",
          "y offset for screen scrapping", 0, 65535, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_WIDTH,
      g_param_spec_int ("width", "width of screen", "width of screen", 0, 65535,
          0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      g_param_spec_int ("height", "height of screen", "height of screen", 0,
          65535, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SHARED,
      g_param_spec_boolean ("shared", "Share desktop with other clients",
          "Share desktop with other clients", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstbasesrc_class->negotiate = GST_DEBUG_FUNCPTR (gst_rfb_src_negotiate);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_rfb_src_stop);
  gstbasesrc_class->event = GST_DEBUG_FUNCPTR (gst_rfb_src_event);
  gstpushsrc_class->fill = GST_DEBUG_FUNCPTR (gst_rfb_src_fill);
  gstbasesrc_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_rfb_src_decide_allocation);

  gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_rfb_src_template);

  gst_element_class_set_static_metadata (gstelement_class, "Rfb source",
      "Source/Video",
      "Creates a rfb video stream",
      "David A. Schleef <ds@schleef.org>, "
      "Andre Moreira Magalhaes <andre.magalhaes@indt.org.br>, "
      "Thijs Vermeir <thijsvermeir@gmail.com>");
}

static void
gst_rfb_src_init (GstRfbSrc * src)
{
  GstBaseSrc *bsrc = GST_BASE_SRC (src);

  gst_pad_use_fixed_caps (GST_BASE_SRC_PAD (bsrc));
  gst_base_src_set_live (bsrc, TRUE);
  gst_base_src_set_format (bsrc, GST_FORMAT_TIME);

  src->decoder = rfbGetClient(8,3,4); /* defaults; will be changed later */
  src->decoder->serverHost = g_strdup ("127.0.0.1");
  src->decoder->serverPort = 5900;
  src->decoder->canHandleNewFBSize = 0;
  src->decoder->appData.useRemoteCursor = 1;

}

static void
gst_rfb_src_finalize (GObject * object)
{
  GstRfbSrc *src = GST_RFB_SRC (object);

  if (src->decoder) {
    g_free(src->decoder->serverHost);
    rfbClientCleanup(src->decoder);
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_rfb_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRfbSrc *src = GST_RFB_SRC (object);
  rfbClient *decoder = src->decoder;

  switch (prop_id) {
    case PROP_HOST:
      decoder->serverHost = g_strdup (g_value_get_string (value));
      break;
    case PROP_PORT:
      decoder->serverPort = g_value_get_int (value);
      break;
    case PROP_USER:
      if (src->user) { g_free (src->user); }
      src->user = g_strdup (g_value_get_string (value));
      break;
    case PROP_PASSWORD:
      if (src->pass) { g_free (src->pass); }
      src->pass = g_strdup (g_value_get_string (value));
      break;
    case PROP_OFFSET_X:
      decoder->updateRect.x = g_value_get_int (value);
      break;
    case PROP_OFFSET_Y:
      decoder->updateRect.y = g_value_get_int (value);
      break;
    case PROP_WIDTH:
      decoder->updateRect.w = g_value_get_int (value);
      break;
    case PROP_HEIGHT:
      decoder->updateRect.h = g_value_get_int (value);
      break;
    case PROP_SHARED:
      src->decoder->appData.shareDesktop = g_value_get_boolean (value);
      break;
    default:
      break;
  }
}

static void
gst_rfb_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRfbSrc *src = GST_RFB_SRC (object);
  gchar *version;

  switch (prop_id) {
    case PROP_HOST:
      g_value_set_string (value, src->decoder->serverHost);
      break;
    case PROP_PORT:
      g_value_set_int (value, src->decoder->serverPort);
      break;
    case PROP_SHARED:
      g_value_set_boolean (value, src->decoder->appData.shareDesktop);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_rfb_src_decide_allocation (GstBaseSrc * bsrc, GstQuery * query)
{
  GstBufferPool *pool = NULL;
  guint size, min = 1, max = 0;
  GstStructure *config;
  GstCaps *caps;
  GstVideoInfo info;
  gboolean ret;

  gst_query_parse_allocation (query, &caps, NULL);

  if (!caps || !gst_video_info_from_caps (&info, caps))
    return FALSE;

  while (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

    /* TODO We restrict to the exact size as we don't support strides or
     * special padding */
    if (size == info.size)
      break;

    gst_query_remove_nth_allocation_pool (query, 0);
    gst_object_unref (pool);
    pool = NULL;
  }

  if (pool == NULL) {
    /* we did not get a pool, make one ourselves then */
    pool = gst_video_buffer_pool_new ();
    size = info.size;
    min = 1;
    max = 0;

    if (gst_query_get_n_allocation_pools (query) > 0)
      gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
    else
      gst_query_add_allocation_pool (query, pool, size, min, max);
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, size, min, max);

  ret = gst_buffer_pool_set_config (pool, config);
  gst_object_unref (pool);

  return ret;
}

static void process_update(rfbClient* cl, int x, int y, int w, int h) {
  *((int*)rfbClientGetClientData(cl, process_update)) += 1;
}

static gboolean
gst_rfb_src_negotiate (GstBaseSrc * bsrc)
{
  GstRfbSrc *src = GST_RFB_SRC (bsrc);
  rfbClient *decoder;
  GstCaps *caps;
  GstVideoInfo vinfo;
  GstVideoFormat vformat;
  guint32 red_mask, green_mask, blue_mask;
  gchar *stream_id = NULL;
  GstEvent *stream_start = NULL;

  decoder = src->decoder;

  if (decoder->sock >= 0)
    return TRUE;

  GST_DEBUG_OBJECT (src, "connecting to host %s on port %d",
      src->decoder->serverHost, src->decoder->serverPort);
  if (!ConnectToRFBServer(decoder, decoder->serverHost, decoder->serverPort)) {
    GST_ELEMENT_ERROR (src, RESOURCE, READ,
        ("Could not connect to VNC server %s on port %d",
            src->decoder->serverHost, src->decoder->serverPort), (NULL));
    return FALSE;
  }

  if (!InitialiseRFBConnection(decoder)) {
    GST_ELEMENT_ERROR (src, RESOURCE, READ,
        ("Failed to setup VNC connection to host %s on port %d",
            src->decoder->serverHost, src->decoder->serverPort), (NULL));
    return FALSE;
  }

  decoder->width = decoder->si.framebufferWidth;
  decoder->height = decoder->si.framebufferHeight;
  if (!decoder->MallocFrameBuffer(decoder)) {
    GST_ELEMENT_ERROR (src, RESOURCE, READ,
        ("Failed to allocate VNC framebuffer for host %s on port %d",
            src->decoder->serverHost, src->decoder->serverPort), (NULL));
    return FALSE;
  }
  if (decoder->updateRect.x < 0) {
    decoder->updateRect.x = decoder->updateRect.y = 0;
    decoder->updateRect.w = decoder->width;
    decoder->updateRect.h = decoder->height;
  }

  stream_id = gst_pad_create_stream_id_printf (GST_BASE_SRC_PAD (bsrc),
      GST_ELEMENT (src), "%s:%d", src->decoder->serverHost, src->decoder->serverPort);
  stream_start = gst_event_new_stream_start (stream_id);
  g_free (stream_id);
  gst_pad_push_event (GST_BASE_SRC_PAD (bsrc), stream_start);

  GST_DEBUG_OBJECT (src, "setting caps width to %d and height to %d",
      decoder->updateRect.w, decoder->updateRect.h);

  red_mask = decoder->si.format.redMax << decoder->si.format.redShift;
  green_mask = decoder->si.format.greenMax << decoder->si.format.greenShift;
  blue_mask = decoder->si.format.blueMax << decoder->si.format.blueShift;

  vformat = gst_video_format_from_masks (decoder->si.format.depth,
      decoder->si.format.bitsPerPixel,
      decoder->si.format.bigEndian ? G_BIG_ENDIAN : G_LITTLE_ENDIAN,
      red_mask, green_mask, blue_mask, 0);
  decoder->format = decoder->si.format;

  gst_video_info_init (&vinfo);

  gst_video_info_set_format (&vinfo, vformat, decoder->updateRect.w,
      decoder->updateRect.h);

  caps = gst_video_info_to_caps (&vinfo);

  gst_base_src_set_caps (bsrc, caps);

  gst_caps_unref (caps);

  if (!SetFormatAndEncodings(decoder)) {
    GST_ELEMENT_ERROR (src, RESOURCE, READ,
        ("Failed to set format/encodings for host %s on port %d",
            src->decoder->serverHost, src->decoder->serverPort), (NULL));
    return FALSE;
  }

  /* Hopefully fill() is called soon */
  int* received_update = calloc(sizeof(int), 1);
  rfbClientSetClientData(decoder, process_update, received_update);
  decoder->GotFrameBufferUpdate = process_update;
  SendFramebufferUpdateRequest(decoder,
                               decoder->updateRect.x, decoder->updateRect.y,
                               decoder->updateRect.w, decoder->updateRect.h,
                               FALSE);

  return TRUE;
}

static gboolean
gst_rfb_src_stop (GstBaseSrc * bsrc)
{
  GstRfbSrc *src = GST_RFB_SRC (bsrc);

  rfbClientCleanup(src->decoder);
  src->decoder = NULL;

  return TRUE;
}

static GstFlowReturn
gst_rfb_src_fill (GstPushSrc * psrc, GstBuffer * outbuf)
{
  GstRfbSrc *src = GST_RFB_SRC (psrc);
  rfbClient *decoder = src->decoder;
  GstMapInfo info;

  int* received_update = rfbClientGetClientData(decoder, process_update);
  while (!*received_update) {
    int ret = WaitForMessage(decoder, 50000);
    if (ret < 0 || (ret > 0 && !HandleRFBServerMessage(decoder))) {
      GST_ELEMENT_ERROR (src, RESOURCE, READ,
          ("Error on setup VNC connection to host %s on port %d",
              decoder->serverHost, decoder->serverPort), (NULL));
      return GST_FLOW_ERROR;
    }
  }
  *received_update = 0;

  if (!gst_buffer_map (outbuf, &info, GST_MAP_WRITE)) {
    GST_ELEMENT_ERROR (src, RESOURCE, WRITE,
        ("Could not map the output frame"), (NULL));
    return GST_FLOW_ERROR;
  }

  int i;
  int bpp = decoder->format.bitsPerPixel/8;
  for (i = 0; i < decoder->updateRect.h; i++) {
    if (info.size < decoder->updateRect.w * decoder->updateRect.h * bpp) {
      gst_buffer_unmap(outbuf, &info);
      return GST_FLOW_OK;
    }
    memcpy(&info.data[i*decoder->updateRect.w*bpp],
           &decoder->frameBuffer[bpp*(decoder->updateRect.x+(decoder->updateRect.y+i)*decoder->width)],
           decoder->updateRect.w*bpp);
  }

  GST_BUFFER_PTS (outbuf) =
      gst_clock_get_time (GST_ELEMENT_CLOCK (src)) -
      GST_ELEMENT_CAST (src)->base_time;

  gst_buffer_unmap (outbuf, &info);

  return GST_FLOW_OK;
}

static gboolean
gst_rfb_src_event (GstBaseSrc * bsrc, GstEvent * event)
{
  GstRfbSrc *src = GST_RFB_SRC (bsrc);

  return TRUE;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "rfbsrc", GST_RANK_NONE,
      GST_TYPE_RFB_SRC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    rfbsrc,
    "Connects to a VNC server and decodes RFB stream",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
