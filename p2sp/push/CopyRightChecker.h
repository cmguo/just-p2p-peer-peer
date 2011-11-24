#ifndef P2SP_PUSH_COPYRIGHTCHECKER_H
#define P2SP_PUSH_COPYRIGHTCHECKER_H

#include <string>
#include <boost/shared_ptr.hpp>

#include <util/protocol/http/HttpClient.h>
#include <util/protocol/http/HttpRequest.h>
#include <util/protocol/http/HttpResponse.h>

namespace p2sp
{
#ifdef DISK_MODE

class CopyrightCheckerListener
{
public:
    typedef boost::shared_ptr<CopyrightCheckerListener> p;
    virtual void OnCopyrightCheckResult(bool passed) = 0;
    virtual void OnCopyrightCheckFailed() = 0;
};

class CopyrightChecker : 
    public boost::noncopyable, 
    public boost::enable_shared_from_this<CopyrightChecker>
{
public:
    typedef boost::shared_ptr<CopyrightChecker> p;
    static p create(boost::asio::io_service & io_svc, CopyrightCheckerListener::p listener)
    {
        return p(new CopyrightChecker(io_svc, listener));
    }

    ~CopyrightChecker();

    void Start(const std::string& channel_id);
    void Stop();

    static bool ParseXMLCopyrightResponse(const std::string& xml, boost::uint32_t &code);

private:
    CopyrightChecker(boost::asio::io_service & io_svc, CopyrightCheckerListener::p listener);

    void OnFetch(boost::system::error_code const & ec);

private:
    boost::asio::io_service & io_svc_;
    boost::shared_ptr<util::protocol::HttpClient> http_client_;
    CopyrightCheckerListener::p listener_;
    std::string channel_id_;

    static std::map<std::string, bool> channel_id2_copyright_map_;//used to cache copyright info
};


#endif
}


#endif