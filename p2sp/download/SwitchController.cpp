//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/download/SwitchController.h"
#include "p2sp/AppModule.h"

namespace p2sp
{
    //////////////////////////////////////////////////////////////////////////
    // Controller
    SwitchController::SwitchController(IGlobalControlTarget::p global_data_provider)
        : is_running_(false)
        , global_data_provider_(global_data_provider)
        , control_timer_(global_second_timer(), 1000,  boost::bind(&SwitchController::OnTimerElapsed, this, &control_timer_))
    {
    }

    SwitchController::~SwitchController()
    {
    }

    SwitchController::p SwitchController::Create(IGlobalControlTarget::p global_data_provider)
    {
        return SwitchController::p(new SwitchController(global_data_provider));
    }

    bool SwitchController::IsValidControlMode(int control_mode)
    {
        return control_mode >= CONTROL_MODE_NULL && control_mode < CONTROL_MODE_COUNT;
    }

    void SwitchController::Start(SwitchController::ControlModeType mode)
    {
        if (true == is_running_)
            return;

        is_running_ = true;

        assert(global_data_provider_);
        // assert(p2p_control_target_);
        // assert(http_control_target_);
        assert(!control_mode_);

        // start mode
        control_mode_ = ControlMode::Create(mode, shared_from_this());
        control_mode_->Start();

        control_timer_.start();

        // global control
        global_data_provider_->OnStateMachineType(mode);
    }

    void SwitchController::Stop()
    {
        if (false == is_running_)
            return;

        // provider
        global_data_provider_.reset();
        // p2p_control_target_.reset();
        // http_control_target_.reset();
        //
        if (control_mode_) { control_mode_->Stop(); control_mode_.reset(); }

        control_timer_.stop();

        is_running_ = false;

    }

    // void SwitchController::SetGlobalControlTarget(IGlobalControlTarget::p global_data_provider)
    // {
    //    global_data_provider_ = global_data_provider;
    // }

    // void SwitchController::SetHTTPControlTarget(IHTTPControlTarget::p http_control_target)
    // {
    //    http_control_target_ = http_control_target;
    // }

    // void SwitchController::SetP2PControlTarget(IP2PControlTarget::p p2p_control_target)
    // {
    //    p2p_control_target_ = p2p_control_target;
    // }

    IGlobalControlTarget::p SwitchController::GetGlobalDataProvider()
    {
        if (false == is_running_)
            return IGlobalControlTarget::p();
        return global_data_provider_;
    }

    IHTTPControlTarget::p SwitchController::GetHTTPControlTarget()
    {
        if (false == is_running_)
            return IHTTPControlTarget::p();
        assert(global_data_provider_);
        return global_data_provider_->GetHTTPControlTarget();
    }

    IP2PControlTarget::p SwitchController::GetP2PControlTarget()
    {
        if (false == is_running_)
            return IP2PControlTarget::p();
        assert(global_data_provider_);
        return global_data_provider_->GetP2PControlTarget();
    }

    void SwitchController::OnTimerElapsed(framework::timer::PeriodicTimer * pointer)
    {
        if (false == is_running_)
            return;
        boost::uint32_t times = pointer->times();
        if (pointer == &control_timer_)
        {
            // dododo
            string state = (string)(control_mode_->GetState());
            global_data_provider_->OnStateMachineState(state);
            //
            global_data_provider_->SetSwitchState(control_mode_->GetState().http_, control_mode_->GetState().p2p_, control_mode_->GetState().timer_using_, control_mode_->GetState().timer_);
            control_mode_->OnControlTimer(times);
        }
        else
        {
            assert(!"SwitchController::OnTimerElapsed: No such timer!");
        }
    }

    void SwitchController::ResumeOrPauseDownload(bool need_pause)
    {
        if(!is_running_)
        {
            return;
        }

        control_mode_->ResumeOrPause(need_pause);
    }

    //////////////////////////////////////////////////////////////////////////
    // State

    SwitchController::State::operator string () const
    {
        std::stringstream ss;
        ss << "<" << http_ << p2p_ << timer_using_ << timer_ << rid_ << ">";
        return ss.str();
    }

    //////////////////////////////////////////////////////////////////////////
    // ControlMode
    SwitchController::ControlMode::p SwitchController::ControlMode::Create(ControlModeType control_mode_type, SwitchController::p controller)
    {
        ControlMode::p mode;
        switch (control_mode_type)
        {
        case SwitchController::CONTROL_MODE_DOWNLOAD:
            mode = DownloadControlMode::Create(controller);
            break;
        case SwitchController::CONTROL_MODE_VIDEO_OPENSERVICE:
            mode = OpenServiceVideoControlMode::Create(controller);
            break;
        case SwitchController::CONTROL_MODE_PUSH_OPENSERVICE:
            mode = OpenServicePushControlMode::Create(controller);
            break;
        default:
            assert(false);
            mode = DownloadControlMode::Create(controller);
            break;
        }
        return mode;
    }
    void SwitchController::ControlMode::Start()
    {
        assert(controller_);
        // assert(controller_->IsRunning());
        if (true == IsRunning())
            return;
        is_running_ = true;
    }
    void SwitchController::ControlMode::Stop()
    {
        if (false == IsRunning())
            return;
        if (controller_)
        {
            // just release the reference to controller
            controller_.reset();
        }
        is_running_ = false;
    }

    void SwitchController::ControlMode::ResumeOrPause(bool neer_pause)
    {
        if (!is_running_)
        {
            return;
        }

        if (neer_pause)
        {
            if (GetP2PControlTarget())
            {
                GetP2PControlTarget()->Pause();
                state_.p2p_ = State::P2P_PAUSING;
            }
            if (GetHTTPControlTarget())
            {
                GetHTTPControlTarget()->Pause();
                state_.http_ = State::HTTP_PAUSING;
            }
            is_paused_by_sdk_ = true;
        }
        else
        {
            is_paused_by_sdk_ = false;
        }
    }
}
