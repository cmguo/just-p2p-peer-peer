//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// SwitchController.h

#ifndef _P2SP_DOWNLOAD_SWITCHCONTROLLER_H_
#define _P2SP_DOWNLOAD_SWITCHCONTROLLER_H_

#include "p2sp/download/SwitchControllerInterface.h"
#include "p2sp/download/HttpTargetsManager.h"

#define SWITCH_LOG_TYPE "switch"
#define SWITCH_DEBUG(msg) LOG(__DEBUG, SWITCH_LOG_TYPE, __FUNCTION__ << ":" << __LINE__ << " " << msg)

namespace p2sp
{
    //////////////////////////////////////////////////////////////////////////
    // SwitchController
    class SwitchController
        : public boost::noncopyable
        , public boost::enable_shared_from_this<SwitchController>
#ifdef DUMP_OBJECT
        , public count_object_allocate<SwitchController>
#endif
    {
    public:
        typedef boost::shared_ptr<SwitchController> p;

        enum ControlModeType
        {
            CONTROL_MODE_NULL,
            CONTROL_MODE_DOWNLOAD,
            CONTROL_MODE_VIDEO_OPENSERVICE,       // 带宽节约优先
            CONTROL_MODE_PUSH_OPENSERVICE,
            CONTROL_MODE_COUNT
        };

        enum DragMachineState
        {
            MS_UNDEFINED = 0,    // 非法值
            MS_YES,         // 拖动后HTTP启动成功
            MS_NO,          // 拖动后HTTP启动不成功
            MS_WAIT         // 尚未到达判断HTTP下载状态的时刻
        };

    public:

        ~SwitchController();

        static SwitchController::p Create(IGlobalControlTarget::p global_data_provider);

        static bool IsValidControlMode(int control_mode);

        void Start(ControlModeType mode);

        void Stop();

        bool IsRunning() const { return is_running_; }

        // framework::timer::ITimerListener
        void OnTimerElapsed(framework::timer::PeriodicTimer * pointer);

    protected:

        IGlobalControlTarget::p GetGlobalDataProvider();

        IHTTPControlTarget::p GetHTTPControlTarget();

        IP2PControlTarget::p GetP2PControlTarget();

    protected:
        SwitchController(IGlobalControlTarget::p global_data_provider);

    protected:

        //////////////////////////////////////////////////////////////////////////
        // 状态机状态
        struct State
        {
            enum HttpState
            {
                HTTP_NONE            = 0,
                HTTP_STOPPED         = 1,
                HTTP_DOWNLOADING     = 2,
                HTTP_PAUSING         = 3,
            } http_;

            enum P2PState
            {
                P2P_NONE             = 0,
                P2P_STOPPED          = 1,
                P2P_DOWNLOADING      = 2,
                P2P_PAUSING          = 3,
            } p2p_;

            enum TimerUsingState
            {
                TIMER_USING_NONE     = 0,
                TIMER_USING_X        = 1,
                TIMER_USING_Y        = 2,
                TIMER_USING_Z        = 3,
                TIMER_USING_H        = 4,
                TIMER_USING_T        = 5,
            } timer_using_;

            enum TimerState
            {
                TIMER_NONE           = 0,
                TIMER_STARTED        = 1,
                TIMER_STOPPED        = 2,
            } timer_;

            enum RIDState
            {
                RID_NONE             = 0,
                RID_GOT              = 1,
            } rid_;

            //////////////////////////////////////////////////////////////////////////
            // Operates
            operator string () const;
        };

        //////////////////////////////////////////////////////////////////////////
        // ControlMode
        class ControlMode
            : public boost::noncopyable
            , public boost::enable_shared_from_this<ControlMode>
#ifdef DUMP_OBJECT
            , public count_object_allocate<ControlMode>
#endif
        {
        public:
            typedef boost::shared_ptr<ControlMode> p;
        public:
            static ControlMode::p Create(ControlModeType control_mode_type, SwitchController::p controller);
        public:
            virtual ~ControlMode(){}
            virtual void Start();
            virtual void Stop();
            virtual void OnControlTimer(uint32_t times) = 0;
            bool IsRunning() const { return is_running_; }
            State GetState() const { return state_; }

        protected:
            SwitchController::p GetController() { assert(controller_); return controller_; }
            IGlobalControlTarget::p GetGlobalDataProvider() { return controller_->GetGlobalDataProvider(); }
            IHTTPControlTarget::p GetHTTPControlTarget() { assert(controller_); return controller_->GetHTTPControlTarget(); }
            IP2PControlTarget::p GetP2PControlTarget() { assert(controller_); return controller_->GetP2PControlTarget(); }


        protected:
            //
            volatile bool is_running_;
            //
            SwitchController::p controller_;
            //
            State state_;

        protected:
            ControlMode(SwitchController::p controller)
                : is_running_(false)
                , controller_(controller)
            {}
        };
    public:
        //////////////////////////////////////////////////////////////////////////
        // QueryControlMode
        class QueryControlMode
            : public ControlMode
        {
        protected:
            QueryControlMode(SwitchController::p controller)
                :ControlMode(controller)
            {
            }
        };
    protected:
        //////////////////////////////////////////////////////////////////////////
        // Download
        class DownloadControlMode
            : public ControlMode
            // , public boost::enable_shared_from_this<DownloadControlMode>
        {
        public:
            typedef boost::shared_ptr<DownloadControlMode> p;
        public:
            static DownloadControlMode::p Create(SwitchController::p controller);
        public:
            virtual void Start();
            virtual void Stop();
            virtual void OnControlTimer(uint32_t times);
        protected:
            bool CanP2PDownloadStably();
            bool IsP2PBad();

            DownloadControlMode(SwitchController::p controller)
                : ControlMode(controller)
            {
            }
        private:
            // 获取RID的定时器
            framework::timer::TickCounter time_counter_h_;
            // P2P尝试的定时器
            framework::timer::TickCounter time_counter_x_;
        };

        //////////////////////////////////////////////////////////////////////////
        // OpenServiceVideoControlMode
        class OpenServiceVideoControlMode
            : public ControlMode
            // , public boost::enable_shared_from_this<OpenServiceVideoControlMode>
        {
        public:
            typedef boost::shared_ptr<OpenServiceVideoControlMode> p;
        public:
            static OpenServiceVideoControlMode::p Create(SwitchController::p controller);
        public:
            virtual void Start();
            virtual void Stop();
            virtual void OnControlTimer(uint32_t times);
        protected:
            OpenServiceVideoControlMode(SwitchController::p controller)
                : ControlMode(controller)
            {
            }
        private:

            IHTTPControlTarget::p GetHTTPControlTarget() {return http_target_manager_.GetCurrentHttpTarget();}

            enum HTTPSPEED
            {
                http_fast = 0,
                http_checking,
                http_slow
            };

            HTTPSPEED Check2300HttpSpeed();
            bool Is2300RestTimeEnough();
            bool Is2200RestTimeEnough();
            bool PrefersSavingServerBandwidth();
            bool Is3200P2pSlow();
            boost::int32_t CalcDownloadPriority();
            bool NeedHttpStart();

            void ChangeTo2200();
            void ChangeTo3200(bool is_p2p_start);
            void ChangeTo2300();
            void ChangeTo2000();

#ifdef USE_MEMORY_POOL
            bool CheckMemory();
#endif

            framework::timer::TickCounter time_counter_x_;
            framework::timer::TickCounter time_counter_y_;
            framework::timer::TickCounter time_counter_h_;
            bool is_timer_h_reset;

            framework::timer::TickCounter time_counter_2200;
            framework::timer::TickCounter time_counter_2300;
            framework::timer::TickCounter time_counter_3200;

            bool is_memory_full;
            bool is_p2p_start_;
            enum HTTPSTASUS
            {
                http_unknown = 0,
                http_bad_once,
                http_bad,
                http_good
            } http_status_;

            enum P2PSTATUS
            {
                p2p_unknown = 0,
                p2p_bad,
                p2p_good
            } p2p_status_;

            int32_t p2p_failed_times_;

            uint32_t http_total;
            uint32_t last_p2p_speed_;

            HttpTargetsManager http_target_manager_;
        };

        //////////////////////////////////////////////////////////////////////////
        // OpenServicePushControlMode
        class OpenServicePushControlMode
            : public ControlMode
            // , public boost::enable_shared_from_this<OpenServicePushControlMode>
        {
        public:
            typedef boost::shared_ptr<OpenServicePushControlMode> p;
        public:
            static OpenServicePushControlMode::p Create(SwitchController::p controller);
        public:
            virtual void Start();
            virtual void Stop();
            virtual void OnControlTimer(uint32_t times);
        protected:
            OpenServicePushControlMode(SwitchController::p controller)
                : ControlMode(controller)
            {
            }
        private:
            framework::timer::TickCounter time_counter_x_;
            framework::timer::TickCounter time_counter_y_;
            framework::timer::TickCounter time_counter_z_;
            uint32_t x_;
            uint32_t y_;
            uint32_t z_;
        };

        //////////////////////////////////////////////////////////////////////////
        // SwitchController Private
    private:

        volatile bool is_running_;

        IGlobalControlTarget::p global_data_provider_;

        ControlMode::p control_mode_;

        framework::timer::PeriodicTimer control_timer_;
    };
}

#endif  // _P2SP_DOWNLOAD_SWITCHCONTROLLER_H_
