// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#include <yarp/os/all.h>
#include <yarp/dev/ControlBoardInterfaces.h>
#include <yarp/dev/CartesianControl.h>
#include <yarp/dev/PolyDriver.h>
#include <yarp/sig/Vector.h>

#include "../include/geometry_msgs_Pose.h"

#include <stdio.h>
#include <string>
#include <iostream>
#include <gsl/gsl_math.h>



using namespace yarp::os;
using namespace yarp::dev;
using namespace yarp::sig;

using namespace std;

class ControlThread: public RateThread
{

    PolyDriver dd;
    ICartesianControl *icart;

    Vector cRot, cOrt; // current rotation and orientation of robot's joint
    Vector dRot, dOrt; // desired rotation and orientation of robot's joint


    // ROS variables
    yarp::os::Node *node;
    yarp::os::Subscriber<geometry_msgs_Pose> poseSub;
    geometry_msgs_Pose *jointPose;

public:
    ControlThread(int period):RateThread(period){}

    bool threadInit()
    {
        //initialize here variables
        printf("ControlThread:starting\n");

        Property options("(device cartesiancontrollerclient)");
        options.put("remote","/icubSim/cartesianController/right_arm");
        options.put("local","/cartesian_client/right_arm");

        dd.open(options);

        if (dd.isValid()) {
           dd.view(icart);

           if (!icart){
              return false;
           }


        }


        // get the torso dofs
        Vector newDof, curDof;
        icart->getDOF(curDof);
        newDof=curDof;

        // enable the torso yaw and pitch
        // disable the torso roll
        newDof[0]=0;
        newDof[1]=0;
        newDof[2]=0;

        // impose some restriction on the torso pitch
        limitTorsoPitch();
        icart->setTrackingMode(true);
        // send the request for dofs reconfiguration
        icart->setDOF(newDof,curDof);

        // icart->setTrajTime(1.0);

        dRot.resize(3);
        dOrt.resize(4);

        // ROS initialization
        node = new yarp::os::Node("/icub_arm_imitator/arm_poses");
        if (!poseSub.topic("/icub/jointPose")) {
          cerr<< "Failed to subscriber to /icub/jointPose\n";
          return -1;
          }

        return true;
    }

    void threadRelease()
    {
        printf("ControlThread:stopping the robot\n");
        poseSub.close();

        icart->stopControl();
        dd.close();

        printf("Done, goodbye from ControlThread\n");
    }

    void run()
    {
      double tf = 0.60;
      dOrt = 0;

      jointPose = poseSub.read(false);

      if (!jointPose) {
        cout << "human palm rotation (xyz): no data yet!" << endl;
      }

      else if (jointPose) {
        dRot[0] = tf * jointPose->position.z;
        dRot[1] = tf * jointPose->position.x;
        dRot[2] = tf * jointPose->position.y;
        // cout << jointPose->position.z << " x " << tf << " = "<< dRot[0] << endl;
        // dOrt[0] = jointPose->orientation.z;
        // dOrt[1] = jointPose->orientation.x;
        // dOrt[2] = jointPose->orientation.y;
        // dOrt[3] = jointPose->orientation.w;
        cout << "robot palm dRotation (xyz)[m]: " << dRot.toString().c_str() << endl;
        icart->goToPose(dRot,dOrt);

        icart->getPose(cRot, cOrt);
        cout << "robot palm rotation (xyz)[m]: " << cRot.toString().c_str() << endl;
      }

      // else {
      //   std::cout << "couldn't convert human to robot!" << std::endl;
      //
      // }




      // icart->goToPoseSync(X_desired,O_desired); // send request and wait for reply
      // icart->waitMotionDone(0.04); // wait until the motion is done and ping at each 0.04 seconds
    }

    void limitTorsoPitch()
    {
        int axis=0; // pitch joint
        double min, max;
        int MAX_TORSO_PITCH = 30.0;
        // sometimes it may be helpful to reduce
        // the range of variability of the joints;
        // for example here we don't want the torso
        // to lean out more than 30 degrees forward

        // we keep the lower limit
        icart->getLimits(axis,&min,&max);
        icart->setLimits(axis,min,MAX_TORSO_PITCH);
    }


};

int main(int argc, char *argv[])
{
    Network yarp;

    if (!yarp.checkNetwork())
    {
        printf("No yarp network, quitting\n");
        return 1;
    }

    ControlThread ctrlThread(200);

    ctrlThread.start();
    int RUN_TIME = 3600; // seconds
    bool done=false;
    double startTime=Time::now();
    while(!done)
    {
        if ((Time::now()-startTime)>RUN_TIME)
            done=true;
    }

    ctrlThread.stop();

    return 0;
}
