#ifndef LIVE_SWITCH_CONTROLLER_H
#define LIVE_SWITCH_CONTROLLER_H

namespace p2sp
{
    class LiveDownloadDriver;
    typedef boost::shared_ptr<LiveDownloadDriver> LiveDownloadDriver__p;

    class LiveHttpDownloader;
    typedef boost::shared_ptr<LiveHttpDownloader> LiveHttpDownloader__p;

    class LiveP2PDownloader;
    typedef boost::shared_ptr<LiveP2PDownloader> LiveP2PDownloader__p;

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
    };

    class LiveSwitchController
    {
    public:
        LiveSwitchController();
        void Start(LiveDownloadDriver__p live_download_driver, bool is_too_near_from_last_vv_of_same_channel,
            bool is_saving_mode);
        void Stop();
    private:
        void ChangeTo3200();
        void ChangeTo2300();
        void ChangeTo3300();

        bool NeedChangeTo2300();
        bool NeedChangeTo3200();
        bool NeedChangeTo3300();

        void CheckState2300();
        void CheckState3200();
        void CheckState3300();

        void ResumeHttpDownloader();
        void PauseHttpDownloader();

        void ResumeP2PDownloader();
        void PauseP2PDownloader();

        LiveHttpDownloader__p GetHTTPControlTarget();
        LiveP2PDownloader__p GetP2PControlTarget();

        void OnTimerElapsed(framework::timer::PeriodicTimer * timer);
        void OnControlTimer(boost::uint32_t times);

        void SetChangeToHttpBecauseOfUrgent();

        framework::timer::PeriodicTimer control_timer_;
        LiveDownloadDriver__p live_download_driver_;

        State state_;

        framework::timer::TickCounter time_counter_2300_;
        framework::timer::TickCounter time_counter_3200_;
#ifdef USE_MEMORY_POOL
        bool CheckMemory();
        bool is_memory_full;
#endif
        bool is_started_;
        boost::uint32_t rest_play_time_when_switched_;
        bool is_http_fast_;
        bool changed_to_http_because_of_large_upload_;

        bool blocked_this_time_;

        bool is_too_near_from_last_vv_of_same_channel_;

        enum StopHttpCondition
        {
            REST_PLAYABLE_TIME_ENOUGTH = 0,
            LONG_TIME_USING_CDN = 1,
            BLOCK = 2,
            UPLOAD_NOT_LARGE_ENOUGH = 3,
            NO_USE_HTTP_WHEN_URGENT = 4,
            WORSE_THAN_P2P = 5,
            REST_PLAYABLE_TIME_NOT_SHORT = 6,
        };

        enum StartHttpCondition
        {
            START = 0,
            URGENT = 1,
            LARGE_UPLOAD = 2,
            FALL_BEHIND = 3,
            NONE_PEERS = 4,
            NO_P2P = 5,
            DRAG = 6,
        };

        bool is_saving_mode_;
        boost::uint8_t reason_of_using_http_;
        boost::uint8_t reason_of_stoping_http_;
    };
}

#endif