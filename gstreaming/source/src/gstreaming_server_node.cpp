#include "gstreaming_server.h"

#include <memory>
#include <ros/ros.h>

using namespace lmt;

int main(int argc, char* argv[])
{
    ros::init(argc, argv, "rtsp_server");

    ros::NodeHandle nh = ros::NodeHandle();
    ros::NodeHandle pnh = ros::NodeHandle("~");

    GStreamingServer serverRos(nh, pnh, argc, argv);

    ros::Rate loop(10);

    while (ros::ok())
    {
        ros::spinOnce();
        loop.sleep();
    }

    return EXIT_SUCCESS;
}
