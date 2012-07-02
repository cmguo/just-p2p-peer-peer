#include "Common.h"
#include "CopyRightChecker.h"

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>

#include <iterator>
#include <sstream>

namespace p2sp
{
#ifdef DISK_MODE

    static const uint32_t MAX_COPYRIGHT_CACHE_SIZE = 100;
    static const std::string COPYRIGHT_INFO_URL = "http://client-play.pplive.cn/chplay-0-";

    std::map<std::string, bool> CopyrightChecker::channel_id2_copyright_map_;

    CopyrightChecker::CopyrightChecker(boost::asio::io_service & io_svc, CopyrightCheckerListener::p listener)
        : io_svc_(io_svc)
        , listener_(listener)
        , is_running_(false)
    {
    }

    void CopyrightChecker::Start(const std::string& channel_id)
    {
        if (is_running_)
        {
            return;
        }

        is_running_ = true;
        channel_id_ = channel_id;

        std::map<std::string, bool>::iterator iter = channel_id2_copyright_map_.find(channel_id);
        if (iter != channel_id2_copyright_map_.end())
        {
            listener_->OnCopyrightCheckResult(iter->second);
            return;
        }

        http_client_.reset(new util::protocol::HttpClient(io_svc_));

        std::string url = COPYRIGHT_INFO_URL + channel_id + ".xml";
        
        http_client_->async_fetch(url, boost::bind(&CopyrightChecker::OnFetch, shared_from_this(), _1));
    }

    void CopyrightChecker::Stop()
    {
        if (!is_running_)
        {
            return;
        }

        is_running_ = false;

        boost::system::error_code ec;

        if (http_client_)
        {
            http_client_->cancel(ec);
            http_client_->close(ec);
            http_client_.reset();
        }

        listener_.reset();
    }

    void CopyrightChecker::OnFetch(boost::system::error_code const & ec)
    {
        if (!is_running_)
        {
            return;
        }

        if (!ec)
        {
            if (http_client_->response_head().err_code == 200)
            {
                std::ostringstream oss;
                oss<<&http_client_->get_response().data();
                boost::uint32_t code;
                if (ParseXMLCopyrightResponse(oss.str(), code))
                {
                    bool passed = code == 0;
                    if (channel_id2_copyright_map_.size() >= MAX_COPYRIGHT_CACHE_SIZE)
                    {
                        channel_id2_copyright_map_.clear();
                    }
                    channel_id2_copyright_map_[channel_id_] = passed;
                    listener_->OnCopyrightCheckResult(passed);
                    return;
                }
            }
        }

        listener_->OnCopyrightCheckFailed();
    }

    bool CopyrightChecker::ParseXMLCopyrightResponse(const std::string& xml, boost::uint32_t &code)
    {
        std::string reg_exp_xml = "^<\\?xml[\\d\\D]*\\?>[\\d\\D]*<root[\\d\\D]*>[\\d\\D]*</root>[\\d\\D]*";
        std::string reg_exp_error_code = "[\\d\\D]*<error code=\"(\\d+)\"[\\d\\D]*/>[\\d\\D]*";
        
        try 
        {
            boost::smatch what;
            if (boost::regex_match(xml, what, boost::regex(reg_exp_xml))) 
            {
                if (boost::regex_match(xml, what, boost::regex(reg_exp_error_code)))
                {
                    code = boost::lexical_cast<boost::uint32_t>(std::string(what[1].first, what[1].second));
                }
                else
                {
                    code = 0;
                }
                return true;
            }
            else
            {
                return false;
            }
        }
        catch(boost::bad_lexical_cast& e)
        {
            return false;
        }
    }
#endif
}