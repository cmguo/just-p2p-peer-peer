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
        void Start(LiveDownloadDriver__p live_download_driver);
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

        framework::timer::TickCounter time_counter_live_control_mode_;
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

        enum ChangedToP2PCondition
        {
            REST_PLAYABLE_TIME_ENOUGTH = 0,
            LONG_TIME_USING_CDN = 1,
            BLOCK = 2,
        };
    };
}

#endif