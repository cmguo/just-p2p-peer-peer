//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _P2SP_DOWNLOAD_SWITCHCONTROLLER_INTERFACE_H_
#define _P2SP_DOWNLOAD_SWITCHCONTROLLER_INTERFACE_H_

namespace p2sp
{
    //////////////////////////////////////////////////////////////////////////
    // JumpBWType
    enum JumpBWType
    {
        JBW_NORMAL = 0,  // 正常模式，尽量节省服务器带宽
        JBW_HTTP_MORE = 1,   // 带宽充足，可以多用HTTP。下载时应用场景：小ISP下载，策略：只要HTTP不挂，就2300状态
        JBW_HTTP_ONLY = 2,     // 强制使用纯HTTP模式
        JBW_HTTP_PREFERRED = 3,  //只要带宽足够，优先使用HTTP。下载时应用场景：VIP，策略：不限速则2200，否则3200
        JBW_P2P_MORE = 4,  // 直播省带宽模式
        JBW_VOD_P2P_ONLY = 5,   // 强制点播使用纯P2P模式 
        JBW_P2P_INCREASE_CONNECT = 6   // 下载时应用场景：多终端，策略：增加P2P连接数，尽量节省服务器带宽，保证下载速度
    };
    //////////////////////////////////////////////////////////////////////////
    // Control Target
    class IControlTarget
        : public boost::noncopyable
    {
    public:
        typedef boost::shared_ptr<IControlTarget> p;

    public:
        virtual ~IControlTarget() {}

    public:

        // Control

        virtual void Pause() = 0;

        virtual void Resume() = 0;

        // Data

        virtual bool IsPausing() = 0;

        virtual boost::uint32_t GetSecondDownloadSpeed() = 0;
        virtual boost::uint32_t GetCurrentDownloadSpeed() = 0;

        virtual boost::uint32_t GetMinuteDownloadSpeed() = 0;

        virtual boost::uint32_t GetRecentDownloadSpeed() = 0;  // 20s

        virtual void SetSpeedLimitInKBps(boost::int32_t speed_in_KBps) = 0;
    };

    class IHTTPControlTarget
        : public IControlTarget
    {
    public:
        typedef boost::shared_ptr<IHTTPControlTarget> p;

    public:
        virtual ~IHTTPControlTarget() {}
    };

    class IP2PControlTarget
        : public IControlTarget
    {
    public:
        typedef boost::shared_ptr<IP2PControlTarget> p;

    public:
        enum P2PDwonloadMode{
            NORMAL_MODE = 0,
            CONTINUOUS_MODE,
            FAST_MODE
        };

    public:
        virtual ~IP2PControlTarget() {}

    public:

        // Control
        virtual void SetDownloadMode(P2PDwonloadMode mode) = 0;

        virtual void SetDownloadPriority(boost::int32_t prioriy) = 0;

        // Data

        virtual boost::uint32_t GetPooledPeersCount() = 0;

        virtual boost::uint32_t GetConnectedPeersCount() = 0;

        virtual boost::uint32_t GetFullBlockPeersCount() = 0;

        virtual boost::uint32_t GetActivePeersCount() = 0;

        virtual boost::uint32_t GetAvailableBlockPeerCount() = 0;

        virtual boost::uint16_t GetNonConsistentSize() = 0;

        virtual boost::uint32_t GetMaxConnectCount() = 0;

        virtual RID GetRid() = 0;

        virtual void GetCandidatePeerInfos(std::vector<protocol::CandidatePeerInfo> &candidate_peers) = 0;

        virtual bool IsLive() = 0;
    };

    //////////////////////////////////////////////////////////////////////////
    // Global Data Provider
    class IGlobalControlTarget
        : public boost::noncopyable
    {
    public:
        typedef boost::shared_ptr<IGlobalControlTarget> p;

    public:
        enum DwonloadMode{
            SMART_MODE = 0,
            FAST_MODE,
            SLOW_MODE
        };

    public:
        virtual ~IGlobalControlTarget() {}

    public:

        virtual boost::uint32_t GetBandWidth() = 0;

        virtual boost::uint32_t GetVipLevel() = 0;

        virtual boost::uint32_t GetDownloadLevel() = 0;

        virtual boost::uint32_t GetFileLength() = 0;

        virtual boost::uint32_t GetDataRate() = 0;

        virtual boost::uint32_t GetPlayElapsedTimeInMilliSec() = 0;

        virtual boost::uint32_t GetDownloadingPosition() = 0;

        virtual boost::uint32_t GetDownloadedBytes() = 0;

        virtual boost::uint32_t GetDataDownloadSpeed() = 0;

        virtual bool IsStartFromNonZero() = 0;

        virtual bool IsDrag() = 0;

        virtual bool IsHeadOnly() = 0;

        virtual bool HasRID() = 0;

        virtual IHTTPControlTarget::p GetHTTPControlTarget() = 0;

        virtual IP2PControlTarget::p GetP2PControlTarget() = 0;

        virtual void OnStateMachineType(boost::uint32_t state_machine_type) = 0;

        virtual void OnStateMachineState(const string& state_machine_state) = 0;

        virtual void SetSpeedLimitInKBps(boost::int32_t speed_in_KBps) = 0;

        virtual void SetSwitchState(boost::int32_t h, boost::int32_t p, boost::int32_t tu, boost::int32_t t) = 0;

        virtual boost::uint32_t GetRestPlayableTime() = 0;

        virtual void SetDragMachineState(boost::int32_t state) = 0;

        virtual bool IsPPLiveClient() = 0;

        virtual bool IsDragLocalPlayForSwitch() = 0;

        virtual boost::int32_t GetDownloadMode() = 0;

        virtual JumpBWType GetBWType() = 0;
        virtual void NoticeLeave2300() = 0;
        virtual void NoticeLeave2000() = 0;
        virtual void SetDragHttpStatus(boost::int32_t status) = 0;

        virtual std::vector<IHTTPControlTarget::p> GetAllHttpControlTargets() = 0;
        virtual void ReportUseBakHost() = 0;
        virtual void ReportBakHostFail() = 0;
    };

}

#endif  // _P2SP_DOWNLOAD_SWITCHCONTROLLER_INTERFACE_H_
