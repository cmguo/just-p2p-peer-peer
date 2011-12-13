//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// PlayInfo.h

#ifndef _P2SP_PROXY_PLAY_INFO_H_
#define _P2SP_PROXY_PLAY_INFO_H_

#include "network/Uri.h"

namespace p2sp
{
    class RangeInfo;

    class PlayInfo
        : public boost::noncopyable
#ifdef DUMP_OBJECT
        , public count_object_allocate<PlayInfo>
#endif
    {
    public:

        enum PlayType
        {
            PLAY_BY_NONE,    //
            PLAY_BY_RID,
            PLAY_BY_URL,
            PLAY_BY_OPEN,
            DOWNLOAD_BY_URL,
        };

        enum SourceType
        {
            SOURCE_PPLIVE,  // 0
            SOURCE_IKAN,    // 1
            SOURCE_PPVOD,   // 2
            SOURCE_BLANK,             // 3
            SOURCE_PPVADOWNLOADBYURL,  // 4
            SOURCE_PROXY,             // 5
            SOURCE_HOOK,              // 6
            SOURCE_DOWNLOAD_MOVIE,    // 7
            SOURCE_PPLIVE_LIVE2 = 12,  // 客户端观看二代直播
            SOURCE_LIVE_DEFAULT = 254,
            SOURCE_DEFAULT = 255,
        };

    public:

        typedef boost::shared_ptr<PlayInfo> p;

        static PlayInfo::p Parse(const string& url);

        // 判断是否为直播请求串
        static bool IsLiveUrl(const string& url);

        // 判断是否为直播参数修改
        static bool IsSetLiveUrl(const string & url);

        static bool IsGreenWayUri(const network::Uri& uri);

        static bool IsGreenWayUrl(const string& url);

        static bool IsMovieUrl(const string& url);

        static string PlayType2Str(const PlayType& play_type);

    public:

        bool HasUrlInfo() const { return has_url_info_; }

        bool HasRidInfo() const { return has_rid_info_; }

        bool HasPpvakey() const { return has_ppva_key_; }

        bool HasPlayerId() const { return has_player_id_; }

        bool HasStart() const { return has_start_; }

        string GetPlayerId() const { return player_id_; }

        protocol::UrlInfo GetUrlInfo() const { return url_info_; }

        protocol::RidInfo GetRidInfo() const { return rid_info_; }

        uint32_t GetStartPosition() const { return start_position_; }

        PlayType GetPlayType() const { return play_type_; }

        boost::int32_t GetSpeedLimit() const { return speed_limit_; }

        uint32_t GetHeadLength() const { return head_length_; }

        SourceType GetSourceType() const { return source_type_; }

        bool GetAutoClose() const { return auto_close_; }

        boost::int32_t GetIsDrag() const { return is_drag_; }

        boost::uint32_t GetRestTime() const { return rest_time_; }

        bool GetHeadOnly() const { return head_only_; }

        boost::uint32_t GetBWType() const {return bwtype_;}

        boost::int32_t GetSendSpeedLimit() const
        {
            return send_speed_limit_;
        }

        std::vector<std::string> GetBakHosts() const {return bak_hosts_;}

        vector<RID> GetLiveRIDs() const {return live_rid_s_;}

        uint32_t GetLiveStart() const {return live_start_;}

        uint32_t GetLiveInterval() const { return live_interval_; }

        bool IsLiveReplay() const { return live_replay_; }

        vector<boost::uint32_t> GetDataRates() const {return data_rate_s_;}

        RID GetChannelID() const
        {
            return channel_id_;
        }

        bool GetLivePause() const { return live_pause_; }

        boost::uint32_t GetUniqueID() const { return unique_id_; }

        boost::shared_ptr<RangeInfo> GetRangeInfo() const {return range_info_;}

    public:

        static bool IsLocalHost(const string& host, boost::uint16_t port = 0);

        static bool IsLocalIP(const string& host, boost::uint16_t port = 0);

    private:

        static bool ParseRidInfo(const network::Uri& uri, protocol::RidInfo& rid_info);

        static bool ParseSpeedLimit(const network::Uri& uri, boost::int32_t& speed_limit);

        static bool ParseHeadLength(const network::Uri& uri, uint32_t& head_length);

        static bool ParseSourceType(const network::Uri& uri, SourceType& source_type);

        static bool ParseAutoClose(const network::Uri& uri, bool& auto_close);

        static bool ParseIsDrag(const network::Uri& uri, boost::int32_t& is_drag);

        static bool ParseHeadOnly(const network::Uri& uri, bool& head_only);

        static bool ParseRestTime(const network::Uri& uri, uint32_t& rest_time);

        static bool ParseBWType(const network::Uri& uri, boost::uint32_t& bwtype);

        static bool ParseSendSpeedLimit(const network::Uri& uri, boost::int32_t& send_speed_limit);

        static bool ParseBakHosts(const network::Uri& uri, std::vector<std::string>& bak_hosts);

        // 解析一个频道所有的rid
        static void ParseLiveRids(const network::Uri& uri, vector<RID> & rid_s);
        // 解析所有rid对应的码流
        static void ParseLiveDataRates(const network::Uri& uri, vector<boost::uint32_t> & data_rate_s);
        // 起始播放点
        static void ParseLiveStart(const network::Uri& uri, boost::uint32_t & live_start);
        // 直播文件产生的间隔
        static void ParseLiveInterval(const network::Uri& uri, boost::uint32_t & live_interval);
        // 直播是否回拖
        static void ParseLiveReplay(const network::Uri& uri, bool & live_replay);
        // Channel ID
        static void ParseChannelID(const network::Uri& uri, RID & channel_id);
        // 直播是否暂停
        static void ParseLivePause(const network::Uri & uri, bool & live_replay);
        // 直播的播放器ID
        static void ParseUniqueID(const network::Uri & uri, boost::uint32_t & unique_id);
        // flash p2p的range
        static boost::shared_ptr<RangeInfo> ParseRangeInfo(const network::Uri & uri);

        static bool GetValueFromUri(const network::Uri & uri, const string & key, uint32_t & value);

    private:

        PlayInfo()
            : start_position_(0)
            , has_url_info_(false)
            , has_rid_info_(false)
            , play_type_(PLAY_BY_NONE)
            , speed_limit_(-1)
            , has_player_id_(false)
            , has_start_(false)
            , auto_close_(true)
            , is_drag_(-1)
            , bwtype_(0)
            , send_speed_limit_(DEFAULT_SEND_SPEED_LIMIT)
            , live_replay_(false)
            , live_pause_(false)
        {
        }

    private:
        protocol::UrlInfo url_info_;
        protocol::RidInfo rid_info_;
        uint32_t start_position_;
        bool has_url_info_;
        bool has_rid_info_;
        PlayType play_type_;
        bool has_ppva_key_;
        boost::int32_t speed_limit_;
        bool has_player_id_;
        string player_id_;
        bool has_start_;
        uint32_t head_length_;
        SourceType source_type_;
        bool auto_close_;
        boost::int32_t is_drag_;
        bool head_only_;
        boost::uint32_t rest_time_;
        boost::uint32_t bwtype_;
        boost::int32_t send_speed_limit_;
        std::vector<std::string> bak_hosts_;
        boost::shared_ptr<RangeInfo> range_info_;

        // 二代直播请求参数
        vector<RID> live_rid_s_;              // 频道rid
        vector<boost::uint32_t> data_rate_s_; // 码流率
        uint32_t live_start_;                 // 请求的播放点
        uint32_t live_interval_;              // 直播文件名的间隔
        bool live_replay_;                    // 是否回拖
        RID channel_id_;                      // 频道ID
        bool live_pause_;                     // 是否暂停
        boost::uint32_t unique_id_;           // 播放器的ID(用于区分多个播放器播放同一个频道的情况)
    };
}

#endif  // _P2SP_PROXY_PLAY_INFO_H_
