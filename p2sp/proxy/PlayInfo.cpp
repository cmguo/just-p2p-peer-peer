//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/proxy/PlayInfo.h"
#include "p2sp/proxy/RangeInfo.h"
#include "statistic/StatisticModule.h"
#include "network/UrlCodec.h"
#include "p2sp/download/SwitchControllerInterface.h"

#include <framework/string/Algorithm.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#define LIVE_REQUEST_FLAG "/playlive.flv"
#define LIVE_SET_FLAG "/setlive.flv"

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_play_info = log4cplus::Logger::getInstance("[play_info]");
#endif

    using network::UrlCodec;

    bool PlayInfo::IsLocalHost(const string& host, boost::uint16_t local_port)
    {
        // std::vector<string> v = framework::util::splite(host, ":");
        std::vector<string> v;
        framework::string::slice<string>(host, std::inserter(v, v.end()), ":");
        if (v.size() == 0) {
            return false;
        }

        string domain = v[0];
        string port = "80";
        if (v.size() == 2) {
            port = v[1];
        }

        // magic port
        string magic_port = "19765";
        if (magic_port == port) {
            return true;
        }

        if (local_port != 0 && port != framework::string::format(local_port)) {
            return false;
        }

        // localhost
        if (boost::algorithm::iequals(domain, "localhost")) {
            return true;
        }
        // 127.0..1
        if (!boost::algorithm::istarts_with(domain, "127.")) {
            return false;
        }
        // simple check
        for (boost::uint32_t i = 0; i < domain.length(); ++i) {
            bool ok = ((domain[i] >= '0' && domain[i] <= '9') || domain[i] == '.');
            if (!ok) {
                return false;
            }
        }
        return true;
    }

    bool PlayInfo::IsLocalIP(const string& host, boost::uint16_t local_port)
    {
        std::vector<string> v;
        framework::string::slice<string>(host, std::inserter(v, v.end()), ":");
        if (v.size() == 0) {
            return false;
        }

        string domain = v[0];
        string port = "80";
        if (v.size() == 2) {
            port = v[1];
        }

        if (local_port != 0 && port != framework::string::format(local_port)) {
            return false;
        }

        boost::system::error_code error;
        boost::asio::ip::address_v4 addr = boost::asio::ip::address_v4::from_string(domain, error);
        if (!error)
        {
            std::vector<boost::uint32_t> local_ips;
            statistic::StatisticModule::Inst()->GetLocalIPs(local_ips);
            for (boost::uint32_t i = 0; i < local_ips.size(); ++i) {
                if (local_ips[i] == addr.to_ulong()) {
                    return true;
                }
            }
        }
        return false;
    }

    bool PlayInfo::IsGreenWayUrl(const string& url)
    {
        network::Uri uri(url);
        return IsGreenWayUri(uri);
    }

    bool PlayInfo::IsGreenWayUri(const network::Uri& uri)
    {
        string request_path = uri.getpath();
        return (
            boost::algorithm::iequals(request_path, "/ppvaplaybyopen") ||
            boost::algorithm::istarts_with(request_path, "/ppvadownloadbyurl") ||
            boost::algorithm::istarts_with(request_path, LIVE_REQUEST_FLAG) || 
            boost::algorithm::istarts_with(request_path, LIVE_SET_FLAG)
       );
    }

    bool PlayInfo::IsMovieUrl(const string& url)
    {
        network::Uri uri(url);
        string request_path = uri.getpath();
        if (boost::algorithm::iequals(request_path, "/ppvaplaybyopen"))
        {
            return true;
        }

        return false;
    }

    bool PlayInfo::ParseRidInfo(const network::Uri& uri, protocol::RidInfo& rid_info)
    {
        // ????RID???
        protocol::RidInfo rid_info_local;
        if (rid_info_local.rid_.from_string(uri.getparameter("rid")))
        {
            return false;
        }

        // rid_info_local.GetFileLength() = boost::lexical_cast<boost::uint32_t> (uri.getparameter("filelength"));
        // rid_info_local.GetBlockSize() = boost::lexical_cast<boost::uint32_t> (uri.getparameter("blocksize"));
        // rid_info_local.GetBlockCount() = boost::lexical_cast<boost::uint32_t> (uri.getparameter("blocknum"));
        boost::system::error_code ec;
        ec = framework::string::parse2(uri.getparameter("filelength"), rid_info_local.file_length_);
        if (ec) return false;
        ec = framework::string::parse2(uri.getparameter("blocksize"), rid_info_local.block_size_);
        if (ec) return false;
        ec = framework::string::parse2(uri.getparameter("blocknum"), rid_info_local.block_count_);
        if (ec) return false;

        // Modified by jeffrey 2010/2/28, ppbox发往内核的请求不带blocknum & block_md5
#ifdef PEER_PC_CLIENT

        string block_md5 = uri.getparameter("blockmd5");
        std::vector<string> v;
        boost::algorithm::split(v, block_md5, boost::algorithm::is_any_of("@"));

        if (rid_info_local.GetBlockCount() != v.size())
        {
            return false;
        }

        for (boost::uint32_t i = 0; i < rid_info_local.GetBlockCount(); i ++)
        {
            RID block_md5_i;
            if (block_md5_i.from_string(v[i]) || block_md5_i.is_empty())
            {
                return false;
            }
            rid_info_local.block_md5_s_.push_back(block_md5_i);
        }
#endif

        rid_info = rid_info_local;
        return true;
    }

    bool PlayInfo::ParseSpeedLimit(const network::Uri& uri, boost::int32_t& speed_limit)
    {
        string param_speedlimit = uri.getparameter("speedlimit");

        if (param_speedlimit.length() > 0) 
        {
            boost::system::error_code ec = framework::string::parse2(param_speedlimit, speed_limit);
            if (!ec) return true;
            else return false;
        }
        return false;
    }

    bool PlayInfo::ParseSourceType(const network::Uri& uri, PlayInfo::SourceType& source_type)
    {
        string source_str = uri.getparameter("source");
        if (source_str.length() > 0)
        {
            boost::uint32_t type;
            boost::system::error_code ec = framework::string::parse2(source_str, type);
            if (!ec)
            {
                source_type = static_cast<PlayInfo::SourceType>(type);
                return true;
            }
        }
        return false;
    }

    bool PlayInfo::ParseIsDrag(const network::Uri& uri, boost::int32_t& is_drag)
    {
        string is_drag_str = uri.getparameter("drag");
        if (is_drag_str.length() > 0)
        {
            // int type = boost::lexical_cast<int>(is_drag_str);
            int type;
            boost::system::error_code ec = framework::string::parse2(is_drag_str, type);
            if (!ec)
            {
                is_drag = (type > 0);
                return true;
            }
            else
            {
                if (boost::algorithm::iequals(is_drag_str, "true") || boost::algorithm::iequals(is_drag_str, "yes")) {
                    is_drag = true;
                    return true;
                }
                if (boost::algorithm::iequals(is_drag_str, "false") || boost::algorithm::iequals(is_drag_str, "no")) {
                    is_drag = false;
                    return true;
                }
                return false;
            }
        }
        return false;
    }

    bool PlayInfo::ParseBakHosts(const network::Uri& uri, std::vector<std::string>& bak_hosts)
    {
        string bak_hosts_str = uri.getparameter("bakhost");
        bak_hosts_str = network::UrlCodec::Decode(bak_hosts_str);
        if (bak_hosts_str.length() > 0)
        {
            boost::algorithm::split(bak_hosts, bak_hosts_str, boost::algorithm::is_any_of("@"));
            return true;
        }

        return false;
    }

    bool PlayInfo::ParseUint32Value(const network::Uri& uri, boost::uint32_t& value, string key)
    {
        string value_str = uri.getparameter(key);

        if (value_str.length() > 0)
        {
            boost::system::error_code ec = framework::string::parse2(value_str, value);
            if (!ec)
            {
                return true;
            }
        }

        return false;
    }

    bool PlayInfo::ParseBoolValue(const network::Uri& uri, bool& value, string key)
    {
        string value_str = uri.getparameter(key);
        if (value_str.length() > 0)
        {
            int type;
            boost::system::error_code ec = framework::string::parse2(value_str, type);
            if (!ec)
            {
                value = (type > 0);
                return true;
            }
            else
            {
                if (boost::algorithm::iequals(value_str, "true") || boost::algorithm::iequals(value_str, "yes")) {
                    value = true;
                    return true;
                }
                if (boost::algorithm::iequals(value_str, "false") || boost::algorithm::iequals(value_str, "no")) {
                    value = false;
                    return true;
                }
            }
        }

        return false;
    }

    bool PlayInfo::ParseDownloadLevel(const network::Uri& uri, DownloadLevel& level)
    {
        string level_str = uri.getparameter("level");
        if (level_str.length() > 0)
        {
            boost::uint32_t type;
            boost::system::error_code ec = framework::string::parse2(level_str, type);
            if (!ec)
            {
                level = static_cast<PlayInfo::DownloadLevel>(type);
                return true;
            }
        }
        return false;
    }

    PlayInfo::p PlayInfo::Parse(const string& url)
    {
        network::Uri uri(url);
        if (false == IsGreenWayUri(uri))
            return PlayInfo::p();

        PlayInfo::p play_info = PlayInfo::p(new PlayInfo());
        play_info->source_type_ = PlayInfo::SOURCE_DEFAULT;
        
        // Play By OpenService
        if (uri.getpath() == "/ppvaplaybyopen")
        {
            string param_url = uri.getparameter("url");
            string param_refer = uri.getparameter("refer");

            if (param_url.length() != 0)
                play_info->has_url_info_ = true;

            play_info->url_info_.url_ = UrlCodec::Decode(param_url);
            play_info->url_info_.refer_url_ = UrlCodec::Decode(param_refer);

            // parse rid info
            play_info->has_rid_info_ = ParseRidInfo(uri, play_info->rid_info_);

            // play type
            play_info->play_type_ = PlayInfo::PLAY_BY_OPEN;

            // player id
            play_info->player_id_ = uri.getparameter("id");
            LOG4CPLUS_DEBUG_LOG(logger_play_info, "Parse PlayerID: " << play_info->player_id_);
            play_info->has_player_id_ = (play_info->player_id_.empty()) ? false : true;

            // head length
            if (false == ParseUint32Value(uri, play_info->head_length_, "headlength")) {
                play_info->head_length_ = 0;
            }
            LOG4CPLUS_DEBUG_LOG(logger_play_info, "Parse HeadLength = " << play_info->head_length_);

            // source type
            if (false == ParseSourceType(uri, play_info->source_type_)) {
                play_info->source_type_ = PlayInfo::SOURCE_PPVOD;
            }
            LOG4CPLUS_DEBUG_LOG(logger_play_info,"Parse SourceType = " << play_info->source_type_);

            // is drag
            if (false == ParseIsDrag(uri, play_info->is_drag_)) {
                play_info->is_drag_ = -1;
            }
            LOG4CPLUS_DEBUG_LOG(logger_play_info, "Parse IsDrag = " << play_info->is_drag_);

            // head only
            if (false == ParseBoolValue(uri, play_info->head_only_, "headonly")) {
                play_info->head_only_ = 0;
            }

            // rest time
            if (!ParseUint32Value(uri, play_info->rest_time_in_millisecond_, "resttime"))
            {
                if (play_info->is_drag_ == 1)
                {
                    // 拖动请求，rest_time_设为0标明下载非常紧急
                    play_info->rest_time_in_millisecond_ = 0;
                }
                else
                {
                    // 不是拖动，要么是正常的启动播放，要么是预下载
                    // 正常启动客户端会在发出请求后通过接口继续设置rest_time_，这里的初始值不会有副作用
                    // 如果是预下载，那么客户端不会通过接口继续设置rest_time_，这里的初值会造成从http启动，
                    //      在后面内核会通过SetRestPlayTime的频度来确定是预下载，从而重新设置rest_time。
                    play_info->rest_time_in_millisecond_ = 0;
                }
            }

            LOG4CPLUS_DEBUG_LOG(logger_play_info, "Parse HeadOnly = " << play_info->head_only_);
            if (false == ParseUint32Value(uri, play_info->bwtype_, "BWType") &&
                false == ParseUint32Value(uri, play_info->bwtype_, "bwtype"))
            {
                play_info->bwtype_ = 0;
            }
            else
            {
                switch(play_info->bwtype_)
                {
                case JBW_NORMAL:
                case JBW_HTTP_MORE:
                case JBW_HTTP_ONLY:
                case JBW_HTTP_PREFERRED:
                case JBW_P2P_MORE:
                case JBW_VOD_P2P_ONLY:
                case JBW_P2P_INCREASE_CONNECT:
                    break;
                default:
                    play_info->bwtype_ = 0;
                    break;
                }
            }

            // 解析客户端请求中的vip字段
            if (false == ParseUint32Value(uri, play_info->vip_, "vip"))
            {
                play_info->vip_ = 0;
            }

            if (false == ParseDownloadLevel(uri, play_info->level_))
            {
                play_info->level_ = PASSIVE_DOWNLOAD_LEVEL;
            }

            if (false == ParseUint32Value(uri, play_info->is_vip_channel_, "vipchannel"))
            {
                play_info->is_vip_channel_ = 0;
            }

            if (false == ParseBoolValue(uri, play_info->is_preroll_, "preroll"))
            {
                play_info->is_preroll_ = 0;
            }
            
            play_info->file_rate_type_ = uri.getparameter("ft");
            string channel_name = uri.getparameter("channelname");
            play_info->channel_name_ = UrlCodec::Decode(channel_name);
            ParseBakHosts(uri, play_info->bak_hosts_);
            LOG4CPLUS_DEBUG_LOG(logger_play_info, "Parse BWType = " << play_info->bwtype_);

            play_info->range_info_ = ParseRangeInfo(uri);

            // speedlimit
            if (false == ParseSpeedLimit(uri, play_info->speed_limit_)) {
                play_info->speed_limit_ = -1;
            }
        }
        // Download By Url
        // else if (uri.getpath() == "/ppvadownloadbyurl")
        else if (true == boost::algorithm::istarts_with(uri.getpath(), "/ppvadownloadbyurl"))
        {
            string param_url = uri.getparameter("url");
            string param_refer = uri.getparameter("refer");

            if (param_url.length() != 0)
                play_info->has_url_info_ = true;

            play_info->url_info_.url_ = UrlCodec::Decode(param_url);
            play_info->url_info_.refer_url_ = UrlCodec::Decode(param_refer);

            // parse optional rid_info
            play_info->has_rid_info_ = ParseRidInfo(uri, play_info->rid_info_);

            // speedlimit
            if (false == ParseSpeedLimit(uri, play_info->speed_limit_)) {
                play_info->speed_limit_ = -1;
            }

            if (false == ParseDownloadLevel(uri, play_info->level_))
            {
                play_info->level_ = PASSIVE_DOWNLOAD_LEVEL;
            }

            play_info->play_type_ = PlayInfo::DOWNLOAD_BY_URL;

            if (false == ParseSourceType(uri, play_info->source_type_)) 
            {
                play_info->source_type_ = PlayInfo::SOURCE_PPVADOWNLOADBYURL;
            }
        }
        // 直播的请求
        else if (boost::algorithm::istarts_with(uri.getpath(), LIVE_REQUEST_FLAG))
        {
            // 解析URL
            string param_url = uri.getparameter("url");
            if (param_url.length() != 0)
            {
                play_info->has_url_info_ = true;
            }

            // Decode Url
            play_info->url_info_.url_ = UrlCodec::Decode(param_url);

            // 解析一个频道所有的rid
            ParseLiveRids(uri, play_info->live_rid_s_);
            // 解析所有rid对应的码流
            ParseLiveDataRates(uri, play_info->data_rate_s_);

            assert(play_info->live_rid_s_.size() == play_info->data_rate_s_.size());
            if (play_info->live_rid_s_.size() != play_info->data_rate_s_.size())
            {
                return PlayInfo::p();
            }

            // 起始播放点
            if (false == ParseUint32Value(uri, play_info->live_start_, "start"))
            {
                assert(false);
            }
            // 直播文件产生的间隔
            if (false == ParseUint32Value(uri, play_info->live_interval_, "interval"))
            {
                assert(false);
            }
            // 直播是否回拖
            if (false == ParseBoolValue(uri, play_info->live_replay_, "replay"))
            {
                assert(false);
            }
            if (!ParseSpeedLimit(uri, play_info->speed_limit_))
            {
                play_info->speed_limit_ = -1;
            }
            // 备用CDN
            ParseBakHosts(uri, play_info->bak_hosts_);
            // BWType
            if (false == ParseUint32Value(uri, play_info->bwtype_, "BWType") &&
                false == ParseUint32Value(uri, play_info->bwtype_, "bwtype"))
            {
                play_info->bwtype_ = 0;
            }
            // ChannelID
            ParseChannelID(uri, play_info->channel_id_);
            // SourceType
            ParseSourceType(uri, play_info->source_type_);
            // UniqueID
            if (false == ParseUint32Value(uri, play_info->unique_id_, "uniqueid"))
            {
                play_info->unique_id_ = 0;
                assert(false);
            }
            // resttime
            if (!ParseUint32Value(uri, play_info->rest_time_in_millisecond_, "resttime"))
            {
                play_info->rest_time_in_millisecond_ = 0;
            }

            return play_info;
        }
        else if (boost::algorithm::istarts_with(uri.getpath(), LIVE_SET_FLAG))
        {
            ParseChannelID(uri, play_info->channel_id_);
            if (false == ParseBoolValue(uri, play_info->live_pause_, "pause"))
            {
                assert(false);
            }

            if (false == ParseUint32Value(uri, play_info->unique_id_, "uniqueid"))
            {
                play_info->unique_id_ = 0;
                assert(false);
            }

            return play_info;
        }

        // start position
        string param_start = uri.getparameter("start");
        if (param_start.length() != 0)
        {
            play_info->has_start_ = true;
            boost::uint32_t start_position = 0;

            //    double position = boost::lexical_cast<double>(param_start);
            double position;
            boost::system::error_code ec = framework::string::parse2(param_start, position);
            if (!ec)
            {
                start_position = (boost::uint32_t)((position < 0 || position >= (1ULL << 32)) ? 0 : (position + 0.5));
            }
            else
            {
                start_position = 0;
            }

            // start_position 不是0表示ikan是拖动请求，设置rest_time默认值为0
            if (start_position != 0)
            {
                play_info->rest_time_in_millisecond_ = 0;
            }
            play_info->start_position_ = start_position;
        }

        return (play_info->has_rid_info_ || play_info->has_url_info_) ? play_info : PlayInfo::p();
    }

    string PlayInfo::PlayType2Str(const PlayType& play_type)
    {
        switch (play_type)
        {
        case PLAY_BY_NONE:
            return "PlayByNone";
        case PLAY_BY_RID:
            return "PlayByRid";
        case PLAY_BY_URL:
            return "PlayByUrl";
        case PLAY_BY_OPEN:
            return "PlayByOpenService";
        case DOWNLOAD_BY_URL:
            return "DownloadByUrl";
        default:
            return "UnknownPlayType";
        }
    }

    // 判断是否为直播请求串
    bool PlayInfo::IsLiveUrl(const string& url)
    {
        network::Uri uri(url);
        string request_path = uri.getpath();
        if (boost::algorithm::iequals(request_path, LIVE_REQUEST_FLAG))
        {
            return true;
        }
        return false;
    }

    // 判断是否为直播参数修改
    bool PlayInfo::IsSetLiveUrl(const string & url)
    {
        network::Uri uri(url);
        string request_path = uri.getpath();
        if (boost::algorithm::iequals(request_path, LIVE_SET_FLAG))
        {
            return true;
        }
        return false;
    }

    // 解析一个频道所有的rid
    void PlayInfo::ParseLiveRids(const network::Uri& uri, vector<RID> & rid_s)
    {
        // 
        string str_rids = uri.getparameter("rid");
        boost::int32_t pos = 0;
        RID rid;

        pos = str_rids.find("@");

        while(pos != -1)
        {
            rid.clear();
            boost::system::error_code ec = rid.from_string(string(str_rids, 0, 32));

            if (ec)
            {
                assert(false);
                return;
            }

            rid_s.push_back(rid);

            str_rids = str_rids.substr(pos+1);
            pos = str_rids.find("@");
        }

        rid.clear();
        rid.from_string(string(str_rids, 0, 32));
        rid_s.push_back(rid);
    }

    // 解析所有rid对应的码流
    void PlayInfo::ParseLiveDataRates(const network::Uri& uri, vector<boost::uint32_t> & data_rate_s)
    {
        string str_datarates = uri.getparameter("datarate");
        boost::int32_t pos = 0;
        boost::int32_t data_rate = 0;

        pos = str_datarates.find("@");

        while(pos != -1)
        {
            boost::system::error_code ec = framework::string::parse2(
                string(str_datarates, 0, pos), data_rate
                );

            if (ec)
            {
                assert(false);
                return;
            }

            data_rate_s.push_back(data_rate);

            str_datarates = str_datarates.substr(pos+1);
            pos = str_datarates.find("@");
        }

        boost::system::error_code ec = framework::string::parse2(str_datarates, data_rate);
        data_rate_s.push_back(data_rate);
    }

    // 直播Channel ID
    void PlayInfo::ParseChannelID(const network::Uri& uri, RID & channel_id)
    {
        string str_channel_id = uri.getparameter("channelid");
        assert (str_channel_id.length() == 32);

        channel_id.clear();
        boost::system::error_code ec = channel_id.from_string(str_channel_id);
        if (!ec)
        {
            return;
        }
        assert(false);
    }

    RangeInfo::p PlayInfo::ParseRangeInfo(const network::Uri & uri)
    {
        boost::uint32_t range_start;
        boost::uint32_t range_end;
        if (!GetValueFromUri(uri, "rangeStart", range_start))
        {
            return RangeInfo::p();
        }
        else if (GetValueFromUri(uri, "rangeEnd", range_end))
        {
            if (range_end == 0)
                range_end = RangeInfo::npos;
            return RangeInfo::p(new RangeInfo(range_start, range_end));
        }

        // rangeEnd missed.
        assert(false);
        return RangeInfo::p();
    }

    bool PlayInfo::GetValueFromUri(const network::Uri & uri, const string & key, boost::uint32_t & value)
    {
        string str_value = uri.getparameter(key);
        if (!str_value.empty() && !framework::string::parse2(str_value, value))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}
