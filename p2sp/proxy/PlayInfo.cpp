//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/proxy/PlayInfo.h"
#include "p2sp/proxy/RangeInfo.h"
#include "statistic/StatisticModule.h"
#include "network/UrlCodec.h"

#include <framework/string/Algorithm.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#define LIVE_REQUEST_FLAG "/playlive.flv"
#define LIVE_SET_FLAG "/setlive.flv"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("downloadcenter");

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
        for (uint32_t i = 0; i < domain.length(); ++i) {
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
            std::vector<uint32_t> local_ips;
            statistic::StatisticModule::Inst()->GetLocalIPs(local_ips);
            for (uint32_t i = 0; i < local_ips.size(); ++i) {
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
//         if (!IsLocalHost(uri.getdomain(), 0))
//             return false;
        string request_path = uri.getpath();
        return (
            boost::algorithm::iequals(request_path, "/ppvaplaybyrid") ||
            boost::algorithm::iequals(request_path, "/ppvaplaybyurl") ||
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
        if (boost::algorithm::iequals(request_path, "/ppvaplaybyrid") ||
            boost::algorithm::iequals(request_path, "/ppvaplaybyurl") ||
            boost::algorithm::iequals(request_path, "/ppvaplaybyopen"))
            return true;
        else
        {
            string file = uri.getfile();
            if (boost::algorithm::iends_with(file, ".mp4") ||
                boost::algorithm::iends_with(file, ".flv") ||
                boost::algorithm::iends_with(file, ".hlv") ||
                boost::algorithm::iends_with(file, ".f4v"))
                return true;
            return false;
        }
    }

    bool PlayInfo::ParseRidInfo(const network::Uri& uri, protocol::RidInfo& rid_info)
    {
        // ????RID???
        protocol::RidInfo rid_info_local;
        if (rid_info_local.rid_.from_string(uri.getparameter("rid")))
        {
            return false;
        }

        // rid_info_local.GetFileLength() = boost::lexical_cast<uint32_t> (uri.getparameter("filelength"));
        // rid_info_local.GetBlockSize() = boost::lexical_cast<uint32_t> (uri.getparameter("blocksize"));
        // rid_info_local.GetBlockCount() = boost::lexical_cast<uint32_t> (uri.getparameter("blocknum"));
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

        for (uint32_t i = 0; i < rid_info_local.GetBlockCount(); i ++)
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

    bool PlayInfo::ParseSendSpeedLimit(const network::Uri& uri, boost::int32_t& send_speed_limit)
    {
        string param_send_speed_limit = uri.getparameter("sendspeedlimit");
        if (param_send_speed_limit.length() > 0)
        {
            boost::system::error_code ec = framework::string::parse2(param_send_speed_limit, send_speed_limit);

            if (send_speed_limit < 0)
            {
                return false;
            }

            if (!ec)
            {
                return true;
            }
        }
        return false;
    }

    bool PlayInfo::ParseHeadLength(const network::Uri& uri, uint32_t& head_length)
    {
        string head_length_str = uri.getparameter("headlength");
        if (head_length_str.length() > 0) {
            // double head_length_db = boost::lexical_cast<double>(head_length_str);
            double head_length_db;
            boost::system::error_code ec = framework::string::parse2(head_length_str, head_length_db);
            if (!ec)
            {
                head_length = (uint32_t)((head_length_db <= 0 || head_length_db >= (1ULL << 32)) ? 0 : (head_length_db + 0.5));
                return true;
            }
            else
            {
                head_length = 0;
                return false;
            }
        }
        return false;
    }

    bool PlayInfo::ParseSourceType(const network::Uri& uri, PlayInfo::SourceType& source_type)
    {
        string source_str = uri.getparameter("source");
        if (source_str.length() > 0)
        {
            uint32_t type;
            boost::system::error_code ec = framework::string::parse2(source_str, type);
            if (!ec)
            {
                source_type = static_cast<PlayInfo::SourceType>(type);
                return true;
            }
        }
        return false;
    }

    bool PlayInfo::ParseAutoClose(const network::Uri& uri, bool& auto_close)
    {
        string auto_close_str = uri.getparameter("autoclose");
        if (auto_close_str.length() > 0)
        {
            // uint32_t type = boost::lexical_cast<uint32_t>(auto_close_str);
//             uint32_t type;
//             boost::system::error_code ec = framework::string::parse2(auto_close_str, type);
//             // auto_close_str is number @herain
//             if (!ec)
//             {
//                 auto_close = (type > 0);
//                 return true;
//             }
//             else
            {
                if (boost::algorithm::iequals(auto_close_str, "true") || boost::algorithm::iequals(auto_close_str, "yes"))
                {
                    auto_close = true;
                    return true;
                }
                if (boost::algorithm::iequals(auto_close_str, "false") || boost::algorithm::iequals(auto_close_str, "no"))
                {
                    auto_close = false;
                    return true;
                }
                return false;
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

    bool PlayInfo::ParseRestTime(const network::Uri& uri, uint32_t& rest_time)
    {
        string resttime_str = uri.getparameter("resttime");
        if (resttime_str.length() > 0)
        {
            boost::system::error_code ec = framework::string::parse2(resttime_str, rest_time);
            if (!ec)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        return false;
    }

    bool PlayInfo::ParseHeadOnly(const network::Uri& uri, bool& head_only)
    {
        string head_only_str = uri.getparameter("headonly");
        if (head_only_str.length() > 0)
        {
            // int type = boost::lexical_cast<int>(head_only_str);
            int type;
            boost::system::error_code ec = framework::string::parse2(head_only_str, type);
            if (!ec)
            {
                head_only = (type > 0);
                return true;
            }
            else
            {
                if (boost::algorithm::iequals(head_only_str, "true") || boost::algorithm::iequals(head_only_str, "yes"))                 {
                    head_only = true;
                    return true;
                }
                if (boost::algorithm::iequals(head_only_str, "false") || boost::algorithm::iequals(head_only_str, "no"))                 {
                    head_only = false;
                    return true;
                }
                return false;
            }
        }
        return false;
    }

    bool PlayInfo::ParseBWType(const network::Uri& uri, boost::uint32_t & bwtype)
    {
        string bwtype_str = uri.getparameter("BWType");
        if (bwtype_str.length() == 0)
        {
            bwtype_str = uri.getparameter("bwtype");
        }

        if (bwtype_str.length() > 0)
        {
            boost::system::error_code ec = framework::string::parse2(bwtype_str, bwtype);
            if (!ec)
            {
                return true;
            }
            else
            {
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

    bool PlayInfo::ParseVip(const network::Uri & uri, boost::uint32_t & is_vip)
    {
        is_vip = 0;

        string vip_str = uri.getparameter("vip");

        if (vip_str.length() == 0)
        {
            
            return false;
        }

        if (vip_str.length() > 0)
        {
            boost::system::error_code ec = framework::string::parse2(vip_str, is_vip);
            if (!ec)
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        return false;
    }

    bool PlayInfo::ParseIsPreroll(const network::Uri& uri, bool& is_preroll)
    {
        string is_preroll_str = uri.getparameter("preroll");
        if (is_preroll_str.length() > 0)
        {
            int type;
            boost::system::error_code ec = framework::string::parse2(is_preroll_str, type);
            if (!ec)
            {
                is_preroll = (type > 0);
                return true;
            }
            else
            {
                if (boost::algorithm::iequals(is_preroll_str, "true") || boost::algorithm::iequals(is_preroll_str, "yes")) {
                    is_preroll = true;
                    return true;
                }
                if (boost::algorithm::iequals(is_preroll_str, "false") || boost::algorithm::iequals(is_preroll_str, "no")) {
                    is_preroll = false;
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
            uint32_t type;
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
        // Play By URL
        if (uri.getpath() == "/ppvaplaybyurl")
        {
            string param_url = uri.getparameter("url");
            string param_refer = uri.getparameter("refer");

            if (param_url.length() != 0)
                play_info->has_url_info_ = true;

            play_info->url_info_.url_ = UrlCodec::Decode(param_url);
            play_info->url_info_.refer_url_ = UrlCodec::Decode(param_refer);

            // parse optional rid_info
            play_info->has_rid_info_ = ParseRidInfo(uri, play_info->rid_info_);

            // play type
            play_info->play_type_ = PlayInfo::PLAY_BY_URL;
        }
        // Play By RID
        else if (uri.getpath() == "/ppvaplaybyrid")
        {
            play_info->has_url_info_ = false;
            play_info->has_rid_info_ = ParseRidInfo(uri, play_info->rid_info_);

            // play type
            play_info->play_type_ = PlayInfo::PLAY_BY_RID;
        }
        // Play By OpenService
        else if (uri.getpath() == "/ppvaplaybyopen")
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
            LOGX(__DEBUG, "proxy", "Parse PlayerID: " << play_info->player_id_);
            play_info->has_player_id_ = (play_info->player_id_.empty()) ? false : true;

            // head length
            if (false == ParseHeadLength(uri, play_info->head_length_)) {
                play_info->head_length_ = 0;
            }
            LOGX(__DEBUG, "proxy", "Parse HeadLength = " << play_info->head_length_);

            // source type
            if (false == ParseSourceType(uri, play_info->source_type_)) {
                play_info->source_type_ = PlayInfo::SOURCE_PPVOD;
            }
            LOGX(__DEBUG, "proxy", "Parse SourceType = " << play_info->source_type_);

            // is drag
            if (false == ParseIsDrag(uri, play_info->is_drag_)) {
                play_info->is_drag_ = -1;
            }
            LOGX(__DEBUG, "proxy", "Parse IsDrag = " << play_info->is_drag_);

            // head only
            if (false == ParseHeadOnly(uri, play_info->head_only_)) {
                play_info->head_only_ = 0;
            }

            // rest time
            if (!ParseRestTime(uri, play_info->rest_time_in_millisecond_))
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

            LOGX(__DEBUG, "proxy", "Parse HeadOnly = " << play_info->head_only_);
            if (false == ParseBWType(uri, play_info->bwtype_)) {
                play_info->bwtype_ = 0;
            }

            // 解析客户端请求中的vip字段
            ParseVip(uri, play_info->vip_);

            if (false == ParseIsPreroll(uri, play_info->is_preroll_)) {
                play_info->is_preroll_ = 0;
            }
            
            play_info->file_rate_type_ = uri.getparameter("ft");
            string channel_name = uri.getparameter("channelname");
            play_info->channel_name_ = UrlCodec::Decode(channel_name);
            ParseBakHosts(uri, play_info->bak_hosts_);
            LOGX(__DEBUG, "proxy", "Parse BWType = " << play_info->bwtype_);

            play_info->range_info_ = ParseRangeInfo(uri);
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
            ParseLiveStart(uri, play_info->live_start_);
            // 直播文件产生的间隔
            ParseLiveInterval(uri, play_info->live_interval_);
            // 直播是否回拖
            ParseLiveReplay(uri, play_info->live_replay_);
            if (!ParseSpeedLimit(uri, play_info->speed_limit_))
            {
                play_info->speed_limit_ = -1;
            }
            // 备用CDN
            ParseBakHosts(uri, play_info->bak_hosts_);
            // BWType
            if (false == ParseBWType(uri, play_info->bwtype_))
            {
                play_info->bwtype_ = 0;
            }
            // ChannelID
            ParseChannelID(uri, play_info->channel_id_);
            // SourceType
            ParseSourceType(uri, play_info->source_type_);
            // UniqueID
            ParseUniqueID(uri, play_info->unique_id_);
            // resttime
            if (!ParseRestTime(uri, play_info->rest_time_in_millisecond_))
            {
                play_info->rest_time_in_millisecond_ = 0;
            }

            return play_info;
        }
        else if (boost::algorithm::istarts_with(uri.getpath(), LIVE_SET_FLAG))
        {
            ParseChannelID(uri, play_info->channel_id_);
            ParseLivePause(uri, play_info->live_pause_);
            ParseUniqueID(uri, play_info->unique_id_);

            return play_info;
        }

        // sendspeedlimit
        if (false == ParseSendSpeedLimit(uri, play_info->send_speed_limit_))
        {
            play_info->send_speed_limit_ = DEFAULT_SEND_SPEED_LIMIT;
        }

        // start position
        string param_start = uri.getparameter("start");
        if (param_start.length() != 0)
        {
            play_info->has_start_ = true;
            uint32_t start_position = 0;

            //    double position = boost::lexical_cast<double>(param_start);
            double position;
            boost::system::error_code ec = framework::string::parse2(param_start, position);
            if (!ec)
            {
                start_position = (uint32_t)((position < 0 || position >= (1ULL << 32)) ? 0 : (position + 0.5));
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
        int32_t pos = 0;
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
        int32_t pos = 0;
        int32_t data_rate = 0;

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

    // 起始播放点
    void PlayInfo::ParseLiveStart(const network::Uri & uri, boost::uint32_t & live_start)
    {
        string str_start = uri.getparameter("start");
        if (str_start.length() > 0)
        {
            boost::system::error_code ec = framework::string::parse2(str_start, live_start);
            if (!ec)
            {
                return;
            }
        }
        assert(false);
    }

    // 直播文件产生的间隔
    void PlayInfo::ParseLiveInterval(const network::Uri & uri, boost::uint32_t & live_interval)
    {
        string str_interval = uri.getparameter("interval");
        if (str_interval.length() > 0)
        {
            boost::system::error_code ec = framework::string::parse2(str_interval, live_interval);
            if (!ec)
            {
                return;
            }
        }
        assert(false);
    }

    // 直播是否回拖
    void PlayInfo::ParseLiveReplay(const network::Uri& uri, bool & live_replay)
    {
        string str_replay = uri.getparameter("replay");
        int replay = 0;
        if (str_replay.length() > 0)
        {
            boost::system::error_code ec = framework::string::parse2(str_replay, replay);
            if (!ec)
            {
                live_replay = (replay != 0);
                return;
            }
        }
        assert(false);
    }

    // 直播是否暂停
    void PlayInfo::ParseLivePause(const network::Uri& uri, bool & live_pause)
    {
        string str_pause = uri.getparameter("pause");
        int pause = 0;
        if (str_pause.length() > 0)
        {
            boost::system::error_code ec = framework::string::parse2(str_pause, pause);
            if (!ec)
            {
                live_pause = (pause != 0);
                return;
            }
        }
        assert(false);
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

    // 直播的播放器id
    void PlayInfo::ParseUniqueID(const network::Uri & uri, boost::uint32_t & unique_id)
    {
        string str_unique_id = uri.getparameter("uniqueid");
        if (str_unique_id.length() > 0)
        {
            boost::system::error_code ec = framework::string::parse2(str_unique_id, unique_id);
            if (!ec)
            {
                return;
            }
            unique_id = 0;
        }
        unique_id = 0;
        assert(false);
    }

    RangeInfo::p PlayInfo::ParseRangeInfo(const network::Uri & uri)
    {
        uint32_t range_start;
        uint32_t range_end;
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

    bool PlayInfo::GetValueFromUri(const network::Uri & uri, const string & key, uint32_t & value)
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
