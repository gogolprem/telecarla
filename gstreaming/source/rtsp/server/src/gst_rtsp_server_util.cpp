#include "gst_rtsp_server_util.h"
#include "rtsp_server_context.h"

#include <cv_bridge/cv_bridge.h>
#include <glib.h>
#include <ros/console.h>
#include <sensor_msgs/image_encodings.h>

namespace lmt
{
namespace rtsp
{
namespace server
{
sensor_msgs::ImageConstPtr getDefaultImage(int width, int height, int frameCount)
{
    cv_bridge::CvImage imgBridge;
    std_msgs::Header header;
    header.stamp = ros::Time::now();

    cv::Mat overlay(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::rectangle(overlay, cvPoint(10, 10), cvPoint(200, 50), cvScalar(1, 1, 1), cv::FILLED, 8, 0);

    cv::putText(overlay,
                "Frame " + std::to_string(frameCount),
                cvPoint(30, 30),
                cv::FONT_HERSHEY_COMPLEX_SMALL,
                0.8,
                cvScalar(255, 255, 255),
                1,
                CV_AA);

    imgBridge = cv_bridge::CvImage(header, sensor_msgs::image_encodings::RGB8, overlay);

    return imgBridge.toImageMsg();
}

GstCaps* gstCapsFromImage(const sensor_msgs::Image::ConstPtr& msg, int framerate)
{
    // http://gstreamer.freedesktop.org/data/doc/gstreamer/head/pwg/html/section-types-definitions.html
    static const ros::M_string known_formats = {{
        {sensor_msgs::image_encodings::RGB8, "RGB"},
        {sensor_msgs::image_encodings::RGB16, "RGB16"},
        {sensor_msgs::image_encodings::RGBA8, "RGBA"},
        {sensor_msgs::image_encodings::RGBA16, "RGBA16"},
        {sensor_msgs::image_encodings::BGR8, "BGR"},
        {sensor_msgs::image_encodings::BGR16, "BGR16"},
        {sensor_msgs::image_encodings::BGRA8, "BGRA"},
        {sensor_msgs::image_encodings::BGRA16, "BGRA16"},
        {sensor_msgs::image_encodings::MONO8, "GRAY8"},
        {sensor_msgs::image_encodings::MONO16, "GRAY16_LE"},
    }};

    if (msg->is_bigendian)
    {
        ROS_ERROR("GST: big endian image format is not supported");
        return nullptr;
    }

    auto format = known_formats.find(msg->encoding);
    if (format == known_formats.end())
    {
        ROS_ERROR("GST: image format '%s' unknown", msg->encoding.c_str());
        return nullptr;
    }

    return gst_caps_new_simple("video/x-raw",
                               "format",
                               G_TYPE_STRING,
                               format->second.c_str(),
                               "width",
                               G_TYPE_INT,
                               msg->width,
                               "height",
                               G_TYPE_INT,
                               msg->height,
                               "framerate",
                               GST_TYPE_FRACTION,
                               framerate,
                               1,
                               nullptr);
}

void needData(GstElement* appSrc, guint unused, RTSPServerContext* context)
{
    context->app->bufferNewData(appSrc);
}

void mediaConfigure(GstRTSPMediaFactory* factory, GstRTSPMedia* media, RTSPServerContext* context)
{
    auto element =
        std::unique_ptr<GstElement, decltype(&gst_object_unref)>(gst_rtsp_media_get_element(media), gst_object_unref);
    GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(element.get()), GST_DEBUG_GRAPH_SHOW_ALL, "pipeline-rtsp-server");
    auto appSrc = std::unique_ptr<GstElement, decltype(&gst_object_unref)>(
        gst_bin_get_by_name_recurse_up(GST_BIN(element.get()), context->app->getName().c_str()), gst_object_unref);

    context->encoder->setEncoderElement(std::unique_ptr<GstElement>(
        gst_bin_get_by_name_recurse_up(GST_BIN(element.get()), context->encoder->getName().c_str())));

    gst_util_set_object_arg(G_OBJECT(appSrc.get()), "format", "time");
    g_object_set_data_full(G_OBJECT(media), "my-extra-data", context->app.get(), (GDestroyNotify)g_free);
    g_signal_connect(appSrc.get(), "need-data", (GCallback)needData, context);

    ROS_INFO("Media configured done");
}
}  // namespace server
}  // namespace rtsp
}  // namespace lmt