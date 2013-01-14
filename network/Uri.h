//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef FRAMEWORK_NETWORK_URI_H
#define FRAMEWORK_NETWORK_URI_H

const boost::uint32_t NOPOS = (boost::uint32_t)-1;

//            synacast:  // www.pplive.com:554/xxxx/xxx/xx.exe?y1 = a&y2 = b#poiasdygopaisdugopasidjfopi
//                      |     domain   |   |        |      |         |
//                      |              |port        +-file-+         |
//                      |     host         |               |         |
//                                         | <--path---->  |<-param->|
//                                         | <---------request------>|
//                                                  |<--filerequest->|
//            <------refer---------------->|
//            <----------------fileurl-------------------->|

namespace network
{
    class Uri
#ifdef DUMP_OBJECT
        : public count_object_allocate<Uri>
#endif
    {
    public:
        Uri(const string &url)
            : url_(url)
            , protcalpos_(NOPOS)
            , host_start_pos_(NOPOS)
            , host_end_pos_(NOPOS)
            , domain_end_pos_(NOPOS)
            , port_start_pos_(NOPOS)
            , port_end_pos_(NOPOS)
            , path_start_pos_(NOPOS)
            , path_end_pos_(NOPOS)
            , file_start_pos_(NOPOS)
            , file_end_pos_(NOPOS)
            , param_start_pos_(NOPOS)
            , param_end_pos_(NOPOS)
            , request_start_pos_(NOPOS)
            , request_end_pos_(NOPOS)
        {
            parseurl();
        }

        Uri(const char *url)
            : url_(url)
            , protcalpos_(NOPOS)
            , host_start_pos_(NOPOS)
            , host_end_pos_(NOPOS)
            , domain_end_pos_(NOPOS)
            , port_start_pos_(NOPOS)
            , port_end_pos_(NOPOS)
            , path_start_pos_(NOPOS)
            , path_end_pos_(NOPOS)
            , file_start_pos_(NOPOS)
            , file_end_pos_(NOPOS)
            , param_start_pos_(NOPOS)
            , param_end_pos_(NOPOS)
            , request_start_pos_(NOPOS)
            , request_end_pos_(NOPOS)
        {
            parseurl();
        }

        bool replacepath(string file_name)
        {
            url_ = getreferer() + file_name;
            return true;
        }

        bool replacefile(string path_name)
        {
            if (file_start_pos_ != NOPOS)
            {
                url_ = url_.substr(0, file_start_pos_) + path_name;

                return true;
            }
            else
                return false;
        }

        string getfileurl() const
        {
            if (file_end_pos_ != NOPOS)
                return url_.substr(0, file_end_pos_);
            else
                return "";
        }

        string getrequest() const
        {
            if (request_start_pos_ != NOPOS)
                return url_.substr(request_start_pos_, request_end_pos_ - request_start_pos_);
            else
                return "";
        }

        string geturl() const
        {
            return url_;
        }

        string getprotocol() const
        {
            if (protcalpos_ != NOPOS)
                return url_.substr(0, protcalpos_);
            else
                return "http";
        }

        string getdomain() const
        {
            if (host_start_pos_ != NOPOS)
                return url_.substr(host_start_pos_, domain_end_pos_ - host_start_pos_);
            else
                return "";
        }

        string getport() const
        {
            if (port_start_pos_ != NOPOS)
                return url_.substr(port_start_pos_, port_end_pos_ - port_start_pos_);
            else
                return "80";
        }

        string getpath() const
        {
            if (path_start_pos_ != NOPOS)
                return url_.substr(path_start_pos_, path_end_pos_ - path_start_pos_);
            else
                return "/";
        }

        string getfile() const
        {
            if (file_start_pos_ != NOPOS)
                return url_.substr(file_start_pos_, file_end_pos_ - file_start_pos_);
            else
                return "";
        }

        string getfilerequest() const
        {
            if (file_start_pos_ != NOPOS)
                return url_.substr(file_start_pos_, request_end_pos_ - file_start_pos_);
            else
                return "";
        }

        string getparameter(const string& key) const
        {
            string para = getparameter();
            boost::uint32_t startp_, endp_;

            para = "&" + para;
            string key1 = "&" + key + "=";

            if ((startp_ = para.find(key1)) == string::npos)
                return "";
            else
            {
                startp_ = para.find('=', startp_) + 1;
                endp_ = para.find('&', startp_);
                if (endp_ == string::npos)
                    endp_ = para.length();
                return para.substr(startp_, endp_ - startp_);
            }
        }

        string getparameter() const
        {
            if (param_start_pos_ != NOPOS)
                return url_.substr(param_start_pos_, param_end_pos_ - param_start_pos_);
            else
                return "";
        }

        string gethost() const
        {
            if (host_start_pos_ != NOPOS)
                return url_.substr(host_start_pos_, host_end_pos_ - host_start_pos_);
            else
                return "";
        }

        string getreferer() const
        {
            if (host_end_pos_ != NOPOS)
                return url_.substr(0, host_end_pos_);
            else
                return "";
        }

    private:
        void parseurl()
        {
            string::size_type beginpos = 0, endpos = 0;

            beginpos = url_.find(':', 0);
            endpos = url_.find('/', 0);
            if (beginpos != string::npos && beginpos < endpos && beginpos < url_.find('.', 0))
            {
                protcalpos_ = beginpos;
                beginpos = url_.find('/', endpos + 1) + 1;
                host_start_pos_ = beginpos;
            }
            else
            {
                host_start_pos_ = 0;
                beginpos = host_start_pos_;
            }

            string s = url_.substr(0, url_.find('/', beginpos));
            endpos = s.find(':', beginpos);
            if (endpos != string::npos)
            {
                port_start_pos_ = endpos + 1;
                beginpos = endpos;
                domain_end_pos_ = endpos;
            }

            endpos = url_.find('/', beginpos);
            if (endpos != string::npos)
            {
                if (domain_end_pos_ == NOPOS)
                    domain_end_pos_ = endpos;
                if (port_start_pos_ != NOPOS)
                    port_end_pos_ = endpos;
                path_start_pos_ = endpos;
                request_start_pos_ = endpos;
                host_end_pos_ = endpos;
            }
            else
            {
                url_ = url_ + '/';
                endpos = url_.find('/', beginpos);
                path_start_pos_ = endpos;
                if (domain_end_pos_ == NOPOS)
                    domain_end_pos_ = url_.length() - 1;
                if (port_start_pos_ != NOPOS)
                    port_end_pos_ = endpos;
                request_start_pos_ = endpos;
                host_end_pos_ = endpos;
            }

            beginpos = endpos;

            // endpos = url_.find('.', beginpos);
            // if (endpos != string::npos)
            // {
            //    beginpos = url_.rfind('/', endpos) + 1;
            //    file_start_pos_ = beginpos;
            // }

            beginpos = url_.find('?', beginpos);
            if (beginpos != string::npos)
            {
                param_start_pos_ = beginpos + 1;
                file_end_pos_ = beginpos;
                path_end_pos_ = beginpos;

                file_start_pos_ = url_.rfind('/', beginpos) + 1;

            }
            else
            {
                file_end_pos_ = url_.length();
                path_end_pos_ = url_.length();

                file_start_pos_ = url_.rfind('/', url_.length()) + 1;
            }

            beginpos = url_.rfind('#', url_.length() - 1);
            if (beginpos != string::npos)
            {
                request_end_pos_ = beginpos;
                param_end_pos_ = beginpos;
            }
            else
            {
                request_end_pos_ = url_.length();
                param_end_pos_ = url_.length();
            }
        }

    private:
        string url_;
        boost::uint32_t protcalpos_;
        boost::uint32_t host_start_pos_;
        boost::uint32_t host_end_pos_;
        boost::uint32_t domain_end_pos_;
        boost::uint32_t port_start_pos_;
        boost::uint32_t port_end_pos_;
        boost::uint32_t path_start_pos_;
        boost::uint32_t path_end_pos_;
        boost::uint32_t file_start_pos_;
        boost::uint32_t file_end_pos_;
        boost::uint32_t param_start_pos_;
        boost::uint32_t param_end_pos_;
        boost::uint32_t request_start_pos_;
        boost::uint32_t request_end_pos_;
    };
}

#endif  // FRAMEWORK_NETWORK_URI_H
