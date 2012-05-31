#ifndef UPLOAD_MODULE_H
#define UPLOAD_MODULE_H

#include "UploadSpeedLimiter.h"
#include "VodUploadManager.h"
#include "LiveUploadManager.h"
#include "TcpUploadManager.h"
#include "p2sp/p2p/NetworkQualityMonitor.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"
#include "p2sp/p2p/UploadSpeedLimitTracker.h"

namespace p2sp
{
    struct UploadSpeedParam
    {
    private:
        // -1  不限速度；0  禁止上传
        boost::int32_t max_speed_in_KBps_;
        // -1  用户未设置; 其他 用户设置值 (最大上传值上限)
        boost::int32_t user_speed_in_KBps_;
    public:
        UploadSpeedParam()
            : max_speed_in_KBps_(-1)
            , user_speed_in_KBps_(-1)
        {
        }
        // user speed
        void SetUserSpeedInKBps(boost::int32_t speed)
        {
            user_speed_in_KBps_ = speed;
        }
        // max speed
        void SetMaxSpeedInKBps(boost::int32_t speed)
        {
            max_speed_in_KBps_ = speed;
        }
        boost::int32_t GetMaxSpeedInKBps() const
        {
            // 用户未设置
            if (user_speed_in_KBps_ <= -1) 
            {
                return max_speed_in_KBps_;
            }
            // 大于用户值
            if (max_speed_in_KBps_ <= -1 || max_speed_in_KBps_ > user_speed_in_KBps_) {
                return user_speed_in_KBps_;
            }
            // ok
            return max_speed_in_KBps_;
        }
    };

    class UploadModule
        : public boost::noncopyable
        , public boost::enable_shared_from_this<UploadModule>
        , public ConfigUpdateListener
    {
    public:

        static boost::shared_ptr<UploadModule> Inst()
        {
            if (!inst_)
            {
                inst_.reset(new UploadModule());
            }

            return inst_;
        }

        void Start(const string& config_path);
        void Stop();

        bool TryHandlePacket(const protocol::Packet & packet);

        void OnP2PTimer(boost::uint32_t times);

        void SetUploadSwitch(bool is_disable_upload);

        boost::uint32_t GetUploadBandWidthInBytes();

        boost::int32_t GetUploadSpeedLimitInKBps() const;

        boost::int32_t GetMaxConnectLimitSize() const;

        boost::int32_t GetMaxUploadLimitSize() const;

        bool NeedUseUploadPingPolicy();

        void UploadControlOnPingPolicy();

        void UploadControlOnIdleTimePolicy();

        void OnConfigUpdated();

        void SetUploadUserSpeedLimitInKBps(boost::int32_t user_speed_in_KBps);

        boost::uint32_t GetMaxUnlimitedUploadSpeedInRecord() const;
		boost::uint32_t GetMaxUploadSpeedIncludeSameSubnet() const;
        boost::uint32_t GetMaxUploadSpeedExcludeSameSubnet() const;

        void ReportRestPlayTime(uint32_t rest_play_time);

    private:
        UploadModule();

        void LoadHistoricalMaxUploadSpeed();
        void SaveHistoricalMaxUploadSpeed();

        void CheckCacheList();

        boost::uint32_t MeasureCurrentUploadSpeed() const;

        void OnUploadSpeedControl(uint32_t times);

        boost::uint32_t GetMaxUploadSpeedForControl() const;

        void UpdateSpeedLimit(boost::int32_t speed_limit);

        boost::int32_t CalcUploadSpeedLimitOnPingPolicy(bool is_network_good);

        void SubmitUploadInfoStatistic();

        void MeasureLiveMaxUploadSpeedExcludeSameSubnet();

    private:
        
        static boost::shared_ptr<UploadModule> inst_;

        string config_path_;

        boost::shared_ptr<UploadSpeedLimiter> upload_limiter_;
        UploadSpeedLimitTracker upload_speed_limit_tracker_;

        std::list<boost::shared_ptr<IUploadManager> > upload_manager_list_;

        bool is_disable_upload_;

        boost::uint32_t local_ip_from_ini_;

        boost::shared_ptr<NetworkQualityMonitor> network_quality_monitor_;

        BootStrapGeneralConfig::UploadPolicy upload_policy_;

        uint32_t recent_play_series_;

        storage::DTType desktop_type_;

        bool is_locking_;

        boost::int32_t max_connect_peers_;
        boost::int32_t max_upload_peers_;
        boost::int32_t max_speed_in_KBps_;

        UploadSpeedParam upload_speed_param_;

        boost::uint32_t max_upload_speed_include_same_subnet_;
        boost::shared_ptr<LiveUploadManager> live_upload_manager_;
        boost::uint32_t live_max_upload_speed_exclude_same_subnet_;

        bool is_play_urgent_;
    };
}

#endif
