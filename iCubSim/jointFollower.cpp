// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#include <yarp/os/all.h>
#include <yarp/os/Network.h>
#include <yarp/os/RateThread.h>
#include <yarp/os/Time.h>
#include <yarp/os/Property.h>
#include <yarp/dev/ControlBoardInterfaces.h>
#include <yarp/dev/CartesianControl.h>
#include <yarp/dev/PolyDriver.h>
#include <yarp/sig/Vector.h>

#include <stdio.h>
#include <string>
#include <iostream>

using namespace yarp::os;
using namespace yarp::dev;
using namespace yarp::sig;

using namespace std;

class ControlThread: public RateThread
{

    PolyDriver dd;
    ICartesianControl *icart;

    Vector X_current, O_current;
    Vector X_desired, O_desired;

    // ROS related variables
    Port port;
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
              std::cout << "driver is available!" << std::endl; // debug
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

        // send the request for dofs reconfiguration
        icart->setDOF(newDof,curDof);

        icart->setTrajTime(1.0);

        X_desired.resize(3);
        O_desired.resize(4);

        // ROS initialization
        Node node("/yarp/icub_sim/joint_listener");

        port.setReadOnly();

        if (!port.open("icub/jointPose")) {
            fprintf(stderr,"Failed to open port\n");
            return 1;
        }

        return true;
    }

    void threadRelease()
    {
        printf("ControlThread:stopping the robot\n");
        icart->stopControl();
        dd.close();

        printf("Done, goodbye from ControlThread\n");
    }

    void run()
    {

      printPoseStatus();
      getHumanJointPose();
      std::cout << "runned!" << std::endl; // debug
      // X_desired = X_current;
      // X_desired[0] += -0.1;
      X_desired[0] = -0.1;
      X_desired[1] = 0.1;
      X_desired[2] = 0.1;
      O_desired = O_current;

      icart->goToPose(X_desired,O_desired);
      // icart->goToPoseSync(X_desired,O_desired); // send request and wait for reply
      // icart->waitMotionDone(0.04); // wait until the motion is done and ping at each 0.04 seconds
    }


    void getHumanJointPose(/* arguments */) {
      Bottle msg;
      if (!port.read(msg)) {
          fprintf(stderr,"Failed to read msg\n");

      }
      else {
          printf("Got [%s]\n", msg.get(0).asString().c_str());
      }

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

    void printPoseStatus(){
      icart->getPose(X_current, O_current);
      std::cout << "Current Rotation (X)[m] = " << X_current.toString().c_str() << std::endl;
      std::cout << "Current Orientation (O)[m] = " << O_current.toString().c_str() << std::endl;

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




    ControlThread ctrlThread(400); //period is 40ms

    ctrlThread.start();
    int RUN_TIME = 4; // seconds
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
