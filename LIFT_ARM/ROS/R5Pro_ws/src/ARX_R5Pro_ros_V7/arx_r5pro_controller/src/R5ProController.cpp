#include "arx_r5pro_controller/R5ProController.hpp"
#include <ros/package.h>

// using namespace std::chrono_literals;

namespace arx::r5
{
    R5ProController::R5ProController(ros::NodeHandle nh)
    {
        ROS_INFO("机械臂开始初始化...");
        std::string arm_control_type = nh.param("arm_control_type", std::string("normal_v2"));

        std::string sub_topic_name = nh.param("sub_topic_name", std::string("/arm_cmd"));
        std::string pub_topic_ee_name_v1 = nh.param("pub_topic_ee_name_v1", std::string("/arm_status_ee"));
        std::string pub_topic_joint_name_v1 = nh.param("pub_topic_joint_name_v1", std::string("/arm_status_joint"));
        std::string pub_topic_name_v2 = nh.param("pub_topic_name_v2", std::string("/arm_status"));
        catch_control_mode_ = static_cast<CatchControlMode>(nh.param("catch_control_mode",0));

        arm_end_type_ = nh.param("arm_end_type", 0);

        std::string package_path = ros::package::getPath("arx_r5pro_controller");
        std::string urdf_path;
        if (arm_end_type_ == 0)
          urdf_path = package_path + "/X5liteaa0.urdf";
        else
          urdf_path = package_path + "/R5_master.urdf";
        interfaces_ptr_ = std::make_shared<InterfacesThread>(urdf_path,nh.param("arm_can_id", std::string("can0")), arm_end_type_);

        if (arm_control_type == "normal_v1")
        {
            ROS_INFO("常规模式启动[v1]");
            // sub
            joint_state_subscriber_ = nh.subscribe<arm_control::PosCmd>(
                sub_topic_name, 10, &R5ProController::CmdCallbackV1, this);

            // pub
            ee_pos_publisher_v1_ = nh.advertise<arm_control::PosCmd>(pub_topic_ee_name_v1, 10);
            joint_state_publisher_v1_ = nh.advertise<arm_control::JointInformation>(pub_topic_joint_name_v1, 10);
            pub_topic_v1_ = true;
            pub_topic_v2_ = false;
        }
        else if (arm_control_type == "normal_v2")
        {
            ROS_INFO("常规模式启动[v2]");
            // sub
            joint_state_subscriber_ = nh.subscribe<arx5_arm_msg::RobotCmd>(
                sub_topic_name, 10, &R5ProController::CmdCallbackV2, this);
            // pub
            joint_state_publisher_ = nh.advertise<arx5_arm_msg::RobotStatus>(pub_topic_name_v2, 10);
            pub_topic_v1_ = false;
            pub_topic_v2_ = true;
        }
        else if (arm_control_type == "vr_slave_v1")
        {
            ROS_INFO("vr_slave启动[v1]");
            // sub
            joint_state_subscriber_ = nh.subscribe<arm_control::JointInformation>(
                sub_topic_name, 10, &R5ProController::FollowCallbackV1, this);

            // pub
            ee_pos_publisher_v1_ = nh.advertise<arm_control::PosCmd>(pub_topic_ee_name_v1, 10);
            joint_state_publisher_v1_ = nh.advertise<arm_control::JointInformation>(pub_topic_joint_name_v1, 10);
            pub_topic_v1_ = true;
            pub_topic_v2_ = false;
        }
        else if (arm_control_type == "remote_master_v1")
        {
            ROS_INFO("remote_master启动[v1]");
            
            interfaces_ptr_->setArmStatus(InterfacesThread::state::G_COMPENSATION);
            // pub
            ee_pos_publisher_v1_ = nh.advertise<arm_control::PosCmd>(pub_topic_ee_name_v1, 10);
            joint_state_publisher_v1_ = nh.advertise<arm_control::JointInformation>(pub_topic_joint_name_v1, 10);
            pub_topic_v1_ = true;
            pub_topic_v2_ = false;
        }
        else if (arm_control_type == "remote_slave_v1")
        {
            ROS_INFO("remote_slave启动[v1]");
            // sub
            joint_state_subscriber_ = nh.subscribe<arm_control::JointInformation>(
                sub_topic_name, 10, &R5ProController::FollowCallbackV1, this);

            // pub
            ee_pos_publisher_v1_ = nh.advertise<arm_control::PosCmd>(pub_topic_ee_name_v1, 10);
            joint_state_publisher_v1_ = nh.advertise<arm_control::JointInformation>(pub_topic_joint_name_v1, 10);
            pub_topic_v1_ = true;
            pub_topic_v2_ = false;
        }
        else if (arm_control_type == "joint_control_v1")
        {
            ROS_INFO("常规模式启动[v1]");
            // sub
            joint_state_subscriber_ = nh.subscribe<arm_control::JointControl>(
                sub_topic_name, 10, &R5ProController::JointControlCallbackV1, this);

            // pub
            ee_pos_publisher_v1_ = nh.advertise<arm_control::PosCmd>(pub_topic_ee_name_v1, 10);
            joint_state_publisher_v1_ = nh.advertise<arm_control::JointInformation>(pub_topic_joint_name_v1, 10);
            pub_topic_v1_ = true;
            pub_topic_v2_ = false;
        }

        timer_ = nh.createTimer(ros::Duration(0.01), &R5ProController::PubState, this);
    }

    void R5ProController::CmdCallbackV2(const arx5_arm_msg::RobotCmd::ConstPtr &msg)
    {
        double end_pos[6] = {msg->end_pos[0], msg->end_pos[1], msg->end_pos[2], msg->end_pos[3], msg->end_pos[4], msg->end_pos[5]};

        Eigen::Isometry3d transform = solve::Xyzrpy2Isometry(end_pos);

        interfaces_ptr_->setEndPose(transform);

        std::vector<double> joint_positions = {0, 0, 0, 0, 0, 0};

        for (int i = 0; i < 6; i++)
        {
            joint_positions[i] = msg->joint_pos[i];
        }

        interfaces_ptr_->setJointPositions(joint_positions);

        interfaces_ptr_->setArmStatus(msg->mode);

        interfaces_ptr_->setCatch(msg->gripper);
    }

    void R5ProController::CmdCallbackV1(const arm_control::PosCmd::ConstPtr &msg)
    {
        double input[6] = {msg->x, msg->y, msg->z, msg->roll, msg->pitch, msg->yaw};
        Eigen::Isometry3d transform = solve::Xyzrpy2Isometry(input);
        interfaces_ptr_->setEndPose(transform);
        interfaces_ptr_->setArmStatus(InterfacesThread::state::END_CONTROL);
        interfaces_ptr_->setCatch(msg->gripper);
    }

    void R5ProController::FollowCallbackV2(const arx5_arm_msg::RobotStatus::ConstPtr &msg)
    {
        std::vector<double> target_joint_position(6, 0.0);

        for (int i = 0; i < 6; i++)
        {
            target_joint_position[i] = msg->joint_pos[i];
        }

        interfaces_ptr_->setJointPositions(target_joint_position);
        interfaces_ptr_->setArmStatus(InterfacesThread::state::POSITION_CONTROL);

        interfaces_ptr_->setCatch(msg->joint_pos[6]);
    }

    void R5ProController::FollowCallbackV1(const arm_control::JointInformation::ConstPtr &msg)
    {
        std::vector<double> target_joint_position(6, 0.0);

        for (int i = 0; i < 6; i++)
        {
            target_joint_position[i] = msg->joint_pos[i];
        }

        interfaces_ptr_->setJointPositions(target_joint_position);
        interfaces_ptr_->setArmStatus(InterfacesThread::state::POSITION_CONTROL);

        if (catch_control_mode_ == CatchControlMode::kPosition)
          interfaces_ptr_->setCatch(msg->joint_pos[6]);
        else
          interfaces_ptr_->setCatchTorque(msg->joint_cur[6]);
    }

    void R5ProController::JointControlCallbackV1(const arm_control::JointControl::ConstPtr &msg)
    {
        std::vector<double> target_joint_position(6, 0.0);

        for (int i = 0; i < 6; i++)
        {
            target_joint_position[i] = msg->joint_pos[i];
        }

        interfaces_ptr_->setJointPositions(target_joint_position);
        interfaces_ptr_->setArmStatus(InterfacesThread::state::POSITION_CONTROL);

        interfaces_ptr_->setCatch(msg->joint_pos[6]);
    }

    // Publisher
    void R5ProController::PubState(const ros::TimerEvent &)
    {

        Eigen::Isometry3d transform = interfaces_ptr_->getEndPose();

        // 填充vector

        std::vector<double> xyzrpy = solve::Isometry2Xyzrpy(transform);

        std::vector<double> joint_pos_vector = interfaces_ptr_->getJointPositons();

        std::vector<double> joint_velocities_vector = interfaces_ptr_->getJointVelocities();

        std::vector<double> joint_current_vector = interfaces_ptr_->getJointCurrent();

        // 发布消息
        ROS_INFO("Publishing RobotStatus message");

        if (pub_topic_v1_)
        {
            pubArmStatusV1(joint_pos_vector, joint_velocities_vector, joint_current_vector, xyzrpy);
        }

        if (pub_topic_v2_)
        {
            pubArmStatusV2(joint_pos_vector, joint_velocities_vector, joint_current_vector, xyzrpy);
        }
    }

    void R5ProController::pubArmStatusV1(std::vector<double> joint_pos_vector,
                                      std::vector<double> joint_velocities_vector,
                                      std::vector<double> joint_current_vector,
                                      std::vector<double> xyzrpy)
    {
        arm_control::JointInformation msg;

        for (int i = 0; i < 7; i++)
        {
            msg.joint_pos[i] = joint_pos_vector[i];
        }
        for (int i = 0; i < 7; i++)
        {
            msg.joint_vel[i] = joint_velocities_vector[i];
        }
        for (int i = 0; i < 7; i++)
        {
            msg.joint_cur[i] = joint_current_vector[i];
        }

        if(arm_end_type_ == 1)
        {
            msg.joint_pos[6] *= 5;
        }

        joint_state_publisher_v1_.publish(msg);

        arm_control::PosCmd msg_pos_cmd;

        msg_pos_cmd.x = xyzrpy[0];
        msg_pos_cmd.y = xyzrpy[1];
        msg_pos_cmd.z = xyzrpy[2];
        msg_pos_cmd.roll = xyzrpy[3];
        msg_pos_cmd.pitch = xyzrpy[4];
        msg_pos_cmd.yaw = xyzrpy[5];

        msg_pos_cmd.gripper = joint_pos_vector[6];

        ee_pos_publisher_v1_.publish(msg_pos_cmd);
    }

    void R5ProController::pubArmStatusV2(std::vector<double> joint_pos_vector,
                                      std::vector<double> joint_velocities_vector,
                                      std::vector<double> joint_current_vector,
                                      std::vector<double> xyzrpy)
    {
        arx5_arm_msg::RobotStatus msg;
        msg.header.stamp = ros::Time::now();

        boost::array<double, 6> result;
        for (int i = 0; i < 6; i++)
        {
            result[i] = xyzrpy[i];
        }

        msg.end_pos = result;

        for (int i = 0; i < 7; i++)
        {
            msg.joint_pos[i] = joint_pos_vector[i];
        }
        for (int i = 0; i < 7; i++)
        {
            msg.joint_vel[i] = joint_velocities_vector[i];
        }
        for (int i = 0; i < 7; i++)
        {
            msg.joint_cur[i] = joint_current_vector[i];
        }

        if(arm_end_type_ == 1)
        {
            msg.joint_pos[6] *= 5;
        }

        joint_state_publisher_.publish(msg);
    }
}

int main(int argc, char *argv[])
{
    ros::init(argc, argv, "r5_controller");
    ros::NodeHandle nh = ros::NodeHandle("~");
    arx::r5::R5ProController controller(nh);
    ros::spin();
    return 0;
}
