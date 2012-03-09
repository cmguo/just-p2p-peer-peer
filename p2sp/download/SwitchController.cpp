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
        uint32_t times = pointer->times();
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

    uint32_t SwitchController::GetDownloadStatus() const
    {
        if (false == is_running_)
            return 0;

        State s = control_mode_->GetState();

        uint32_t state = 0;
        // 1:p2p单独跑支持range  2: http单独跑支持range  3:两个一起跑支持range
        // 11:p2p单独跑不支持range 12:http单独跑不支持range 13:两个一起跑不支持range

        // range
        if (s.range_ != State::RANGE_SUPPORT)
            state += 10;
        bool http_pausing = (s.http_ != State::HTTP_DOWNLOADING);
        bool p2p_pausing = (s.p2p_ != State::P2P_DOWNLOADING);
        // p2p/http
        if (http_pausing && !p2p_pausing)  // http pause
            state += 1;
        else if (!http_pausing && p2p_pausing)  // p2p pause
            state += 2;
        else if (!http_pausing && !p2p_pausing)
            state += 3;
        else if (http_pausing && p2p_pausing)
            state = 0;

        return state;
    }

    //////////////////////////////////////////////////////////////////////////
    // State

    SwitchController::State::operator string () const
    {
        std::stringstream ss;
        // ss << "http=" << http_ << ";"
        //   << "p2p=" << p2p_ << ";"
        //   << "rid=" << rid_ << ";"
        //   << "range=" << range_ << ";"
        //   << "timer=" << timer_ << ";"
        //   << "timer_using=" << timer_using_ << ";"
        //  ;
        ss << "<" << http_ << p2p_ << timer_using_ << timer_ << range_ << rid_ << ">";
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
        case SwitchController::CONTROL_MODE_VIDEO:
            mode = SimpleVideoControlMode::Create(controller);
            break;
        case SwitchController::CONTROL_MODE_VIDEO_OPENSERVICE:
            mode = OpenServiceVideoControlMode::Create(controller);
            break;
        case SwitchController::CONTROL_MODE_PUSH_OPENSERVICE:
            mode = OpenServicePushControlMode::Create(controller);
            break;
        default:
            mode = NullControlMode::Create(controller);
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
    void SwitchController::ControlMode::Next(uint32_t times)
    {
        if (false == IsRunning())
            return;
        // 必须用post
        // 2010.1.6 modified by jeffrey
        // TODO: 暂时先加上post的方式调用，有时间把Next函数取消，全部换成while, continue的方式实现跳转
        global_io_svc().post(boost::bind(&SwitchController::ControlMode::OnControlTimer, shared_from_this(), times));
    }
    void SwitchController::ControlMode::CheckRange()
    {
        if (false == IsRunning())
            return;
        assert(GetHTTPControlTarget());
        if (!GetHTTPControlTarget())
            return;
        // check
        if (state_.range_ == State::RANGE_NONE)
        {
            if (GetHTTPControlTarget() && false == GetHTTPControlTarget()->IsDetecting())
            {
                state_.range_ = (GetHTTPControlTarget()->IsSupportRange() ? State::RANGE_SUPPORT : State::RANGE_UNSUPPORT);
            }
            else if (GetHTTPControlTarget() && true == GetHTTPControlTarget()->IsDetecting())
            {
                state_.range_ = State::RANGE_DETECTING;
            }
        }
        else if (state_.range_ == State::RANGE_DETECTING)
        {
            if (false == GetHTTPControlTarget()->IsDetecting())
            {
                state_.range_ = (GetHTTPControlTarget()->IsSupportRange() ? State::RANGE_SUPPORT : State::RANGE_UNSUPPORT);
            }
        }
        else if (state_.range_ == State::RANGE_DETECTED)
        {
            state_.range_ = (GetHTTPControlTarget()->IsSupportRange() ? State::RANGE_SUPPORT : State::RANGE_UNSUPPORT);
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // NullControlMode
    SwitchController::NullControlMode::p SwitchController::NullControlMode::Create(SwitchController::p controller)
    {
        return NullControlMode::p(new NullControlMode(controller));
    }
    void SwitchController::NullControlMode::Start()
    {
        ControlMode::Start();

        state_.http_ = (GetHTTPControlTarget() ? State::HTTP_DOWNLOADING : State::HTTP_NONE);
        state_.p2p_ = (GetP2PControlTarget() ? State::P2P_DOWNLOADING : State::P2P_NONE);
        state_.rid_ = (GetP2PControlTarget() ? State::RID_GOT : State::RID_NONE);
        state_.range_ = (GetHTTPControlTarget() ? (GetHTTPControlTarget()->IsDetecting() ? State::RANGE_DETECTING : State::RANGE_DETECTED) : State::RANGE_NONE);
        state_.timer_ = State::TIMER_NONE;
        state_.timer_using_ = State::TIMER_USING_NONE;
        if (GetHTTPControlTarget())      GetHTTPControlTarget()->Resume();
        if (GetP2PControlTarget())     GetP2PControlTarget()->Resume();
    }
    void SwitchController::NullControlMode::Stop()
    {
        ControlMode::Stop();
    }

    void SwitchController::NullControlMode::OnControlTimer(uint32_t times)
    {
        if (false == IsRunning())
            return;
        // nothing
    }

    SwitchController::ControlMode::p SwitchController::GetControlMode()
    {
        return control_mode_;
    }

}
