#include "Common.h"
#include "tinyxml.h"
#include "CrossDomainConfig.h"
#include "p2sp/AppModule.h"

namespace network
{
    string const CrossDomainConfig::cross_domain_url_ = "player.pplive.cn/crossdomain.xml";
    CrossDomainConfig::p CrossDomainConfig::inst_;

    CrossDomainConfig::CrossDomainConfig()
        : failed_count_(0)
    {
    }

    CrossDomainConfig::p CrossDomainConfig::GetInstance()
    {
        if (!inst_)
        {
            inst_.reset(new CrossDomainConfig());
        }
        return inst_;
    }

    void CrossDomainConfig::LoadConfig()
    {
        if (client_)
        {
            client_->Close();
            client_.reset();
        }

        client_ = network::HttpClient<protocol::SubPieceContent>::create(global_io_svc(), cross_domain_url_, "", 0, 0, false);
        client_->SetHandler(shared_from_this());
        client_->Connect();        
    }

    void CrossDomainConfig::SetCrossDomainString(const string& cross_domain_string)
    {
        tinyxml::TiXmlDocument document;
        document.Parse(cross_domain_string.c_str());
        if (document.Error())
        {
            return;
        }

        tinyxml::TiXmlElement *root_element = document.RootElement();
        if (root_element->ValueStr() != "cross-domain-policy")
        {
            return;
        }

        tinyxml::TiXmlElement *kugou_element = new tinyxml::TiXmlElement("allow-access-from");
        if (!kugou_element)
        {
            return;
        }

        kugou_element->SetAttribute("domain", "*.kugou.com");
        tinyxml::TiXmlElement *local_element = new tinyxml::TiXmlElement("allow-access-from");
        if (!local_element)
        {
            return;
        }

        local_element->SetAttribute("domain", "localhost");
        root_element->LinkEndChild(kugou_element);
        root_element->LinkEndChild(local_element);

        tinyxml::TiXmlHandle my_handler(&document);
        tinyxml::TiXmlElement* allow_domain = my_handler.FirstChild("cross-domain-policy").FirstChild("allow-access-from").ToElement();
  
        for(allow_domain; allow_domain; allow_domain = allow_domain->NextSiblingElement())
            allow_domain->SetAttribute("to-ports", "*");

        tinyxml::TiXmlPrinter my_printer;
        document.Accept(&my_printer);
        cross_domain_string_ = my_printer.Str();
    }

    string CrossDomainConfig::GetCrossDomainString() const
    {
        if (!cross_domain_string_.empty())
            return cross_domain_string_;
        return 
            "<?xml version=\"1.0\"?>"
            "<!DOCTYPE cross-domain-policy"
            "SYSTEM \"http://www.adobe.com/xml/dtds/cross-domain-policy.dtd\">"
            "<cross-domain-policy>"
            "<allow-access-from domain=\"*.pplive.com\" to-ports=\"*\"/>"
            "<allow-access-from domain=\"*.pplive.net\" to-ports=\"*\"/>"
            "<allow-access-from domain=\"*.pplive.cn\" to-ports=\"*\"/>"
            "<allow-access-from domain=\"*.pplive.com.cn\" to-ports=\"*\"/>"
            "<allow-access-from domain=\"*.pplive.net.cn\" to-ports=\"*\"/>"
            "<allow-access-from domain=\"*.pp.tv\" to-ports=\"*\"/>"
            "<allow-access-from domain=\"*.pptv.com\" to-ports=\"*\"/>"
            "<allow-access-from domain=\"*.sina.com.cn\" to-ports=\"*\"/>"
            "<allow-access-from domain=\"*.123yun.net\" to-ports=\"*\"/>"
            // 下面两条是为了和酷狗兼容，因为酷狗也监听了843端口，并且允许下面两个域的访问
            "<allow-access-from domain=\"*.kugou.com\" to-ports=\"*\"/>"    
            "<allow-access-from domain=\"localhost\" to-ports=\"*\"/>"
            "</cross-domain-policy>"
            ;
    }

    void CrossDomainConfig::OnConnectFailed(uint32_t error_code)
    {
        client_->Close();
        NeedReload();
    }

    void CrossDomainConfig::OnConnectSucced()
    {
        client_->HttpGet();
    }

    void CrossDomainConfig::OnConnectTimeout()
    {
        client_->Close();
        NeedReload();
    }

    void CrossDomainConfig::OnRecvHttpDataFailed(uint32_t error_code)
    {
        client_->Close();
        NeedReload();
    }

    void CrossDomainConfig::OnRecvHttpDataPartial(protocol::SubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset)
    {
        client_->Close();
    }

    void CrossDomainConfig::OnRecvHttpDataSucced(protocol::SubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset, bool is_gzip)
    {
        //目前仅仅处理crossdomain.xml文件小于1K的情况，在大于1K时将会出错
        //TODO: 处理crossdomain.xml > 1k时的逻辑
        string cross_domain_string((char *)buffer.Data());
        SetCrossDomainString(cross_domain_string);
    }

    void CrossDomainConfig::OnRecvHttpHeaderFailed(uint32_t error_code)
    {
        client_->Close();
        NeedReload();
    }

    void CrossDomainConfig::OnRecvHttpHeaderSucced(network::HttpResponse::p http_response)
    {
        if (http_response->GetStatusCode() != 200)
        {
            return;
        } 
        client_->HttpRecv(http_response->GetFileLength());
    }

    void CrossDomainConfig::OnRecvTimeout()
    {
        client_->Close();
        NeedReload();
    }

    void CrossDomainConfig::OnComplete()
    {
        if (client_)
        {
            client_->Close();
            client_.reset();
        }

        failed_count_ = 0;
    }

    void CrossDomainConfig::NeedReload()
    {
        if (failed_count_++ <= 5)
        {
            LoadConfig();
        }
    }
}
