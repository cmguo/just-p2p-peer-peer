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
            CONTROL_MODE_VIDEO,
            CONTROL_MODE_DOWNLOAD,
            CONTROL_MODE_VIDEO_OPENSERVICE,       // 带宽节约优先
            CONTROL_MODE_PUSH_OPENSERVICE,
            CONTROL_MODE_LIVE,
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

        // for download driver statistic submit
        uint32_t GetDownloadStatus() const;

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

            //////////////////////////////////////////////////////////////////////////

            enum RangeState
            {
                RANGE_NONE           = 0,
                RANGE_DETECTING      = 1,
                RANGE_DETECTED       = 2,
                RANGE_SUPPORT        = 3,
                RANGE_UNSUPPORT      = 4,
            } range_;

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
            void CheckRange();
            void Next(uint32_t times);

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
        // Null
        class NullControlMode
            : public ControlMode
        {
        public:
            typedef boost::shared_ptr<NullControlMode> p;
        public:
            static NullControlMode::p Create(SwitchController::p controller);
        public:
            virtual void Start();
            virtual void Stop();
            virtual void OnControlTimer(uint32_t times);
        protected:
            NullControlMode(SwitchController::p controller)
                : ControlMode(controller)
            {
            }
        };

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

        ////////////////////////////////////////////////////////////////////
        // Video
        class VideoControlMode
            : public ControlMode
            // , public boost::enable_shared_from_this<VideoControlMode>
        {
        public:
            typedef boost::shared_ptr<VideoControlMode> p;
        public:
            static VideoControlMode::p Create(SwitchController::p controller);
        public:
            virtual void Start();
            virtual void Stop();
            virtual void OnControlTimer(uint32_t times);
        protected:
            VideoControlMode(SwitchController::p controller)
                : ControlMode(controller)
            {}

            bool P2PCanDropHttp();
            bool P2PCanPlayStably();  // 根据P2P的速度和节点数以及活跃节点数和全满的节点数等参数进行判断
            bool P2PMayPlayStably();  // 只根据p2p的节点数进行判断
            bool P2PCanDownloadStably();

        private:
            // 状态
            State state_;
            // HTTP 预下载
            framework::timer::TickCounter time_counter_t_;
            //
            framework::timer::TickCounter time_counter_x_;
            //
            framework::timer::TickCounter time_counter_y_;
            //
            framework::timer::TickCounter time_counter_z_;
            //
            framework::timer::TickCounter time_counter_h_;
            //
            framework::timer::TickCounter time_counter_elapsed_;
            //
            uint32_t t_;
            //
            uint32_t h_;
            //
            uint32_t x_;
            //
            uint32_t y_;
            //
            uint32_t z_;
            //
            uint32_t speed_h_;
        };

        //////////////////////////////////////////////////////////////////////////
        // SimpleVideoControlMode
        class SimpleVideoControlMode
            : public ControlMode
            // , public boost::enable_shared_from_this<SimpleVideoControlMode>
        {
        public:
            typedef boost::shared_ptr<SimpleVideoControlMode> p;
        public:
            static SimpleVideoControlMode::p Create(SwitchController::p controller);
        public:
            virtual void Start();
            virtual void Stop();
            virtual void OnControlTimer(uint32_t times);
        protected:
            SimpleVideoControlMode(SwitchController::p controller)
                : ControlMode(controller)
            {
            }

            bool P2PCanDropHttp();
            bool P2PCanPlayStably();  // 根据P2P的速度和节点数以及活跃节点数和全满的节点数等参数进行判断
            bool P2PMayPlayStably();  // 只根据p2p的节点数进行判断
            bool P2PCanDownloadStably();
            uint32_t P2PEstimatedDownloadTimeInSec();

            uint32_t CalcX();
            uint32_t CalcY();
            bool IsHttpBad();
            void CheckRID();

        private:
            // HTTP 预下载
            framework::timer::TickCounter time_counter_t_;
            //
            framework::timer::TickCounter time_counter_x_;
            //
            framework::timer::TickCounter time_counter_y_;
            //
            framework::timer::TickCounter time_counter_z_;
            //
            framework::timer::TickCounter time_counter_h_;
            //
            framework::timer::TickCounter time_counter_elapsed_;
            //
            framework::timer::TickCounter timer_counter_2300x1;
            //
            framework::timer::TickCounter timer_counter_230031;
            //
            framework::timer::TickCounter timer_counter_220031;
            //
            framework::timer::TickCounter timer_counter_3200x1;
            //
            bool is_2200_from_2300;
            //
            uint32_t t_;
            //
            uint32_t h_;
            //
            uint32_t x_;
            //
            uint32_t y_;
            //
            uint32_t z_;
            //
            uint32_t speed_h_;
            //
            bool p2p_tried;
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
            bool PrefersSavingServerBandwidth();
            bool Is3200P2pSlow();
            boost::int32_t CalcDownloadPriority();

            void ChangeTo2200();
            void ChangeTo3200(bool is_p2p_start);
            void ChangeTo2300();

#ifdef USE_MEMORY_POOL
            bool CheckMemory();
#endif

            framework::timer::TickCounter time_counter_x_;
            framework::timer::TickCounter time_counter_y_;

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
        // LiveControlMode
        class LiveControlMode
            : public QueryControlMode
        {
        public:
            typedef boost::shared_ptr<LiveControlMode> p;
            static LiveControlMode::p Create(SwitchController::p controller);
            virtual void Start();
            virtual void OnControlTimer(uint32_t times);

        protected:
            LiveControlMode(SwitchController::p controller)
                :QueryControlMode(controller)
                , is_started_(true)
                , rest_play_time_when_switched_(0)
            {
            }

        private:
            framework::timer::TickCounter time_counter_live_control_mode_;
            framework::timer::TickCounter time_counter_2300_;
            framework::timer::TickCounter time_counter_3200_;
#ifdef USE_MEMORY_POOL
            bool is_memory_full;
#endif
            bool is_started_;
            boost::uint32_t rest_play_time_when_switched_;

        private:
            void ChangeTo3200();
            void ChangeTo2300();

            void CheckState3200();
            void CheckState2300();

            bool NeedChangeTo2300();

            void PauseHttpDownloader();
            void ResumeHttpDownloader();
            void PauseP2PDownloader();
            void ResumeP2PDownloader();

#ifdef USE_MEMORY_POOL
            bool CheckMemory();
#endif
        };

        //////////////////////////////////////////////////////////////////////////
        // SwitchController Private
    private:

        volatile bool is_running_;

        IGlobalControlTarget::p global_data_provider_;

        ControlMode::p control_mode_;

        framework::timer::PeriodicTimer control_timer_;

    public:
        ControlMode::p GetControlMode();
    };
}

#endif  // _P2SP_DOWNLOAD_SWITCHCONTROLLER_H_
