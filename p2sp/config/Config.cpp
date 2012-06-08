#include "Common.h"
#include <util/archive/BinaryIArchive.h>
#include <util/archive/BinaryOArchive.h>
#include <boost/program_options.hpp>
#include "p2sp/proxy/ProxyModule.h"
#include "struct/UdpBuffer.h"
#include "framework/string/Md5.h"
#include <fstream>
#include "Config.h"

const boost::uint32_t Config::InvalidNumber = std::numeric_limits<boost::uint32_t>::max();
const std::string Config::SectionName = "config";
boost::shared_ptr<Config> Config::inst_;

Config::Config()
{
    config_file_path_ = p2sp::ProxyModule::Inst()->GetConfigPath();

    boost::filesystem::path temp_path(config_file_path_);
    temp_path /= "pph";
    config_file_path_ = temp_path.file_string();
}

boost::shared_ptr<Config> Config::Inst()
{
    if (inst_)
    {
        return inst_;
    }

    inst_.reset(new Config());
    return inst_;
}

void Config::LoadConfig()
{
    std::ifstream ifs(config_file_path_.c_str(), std::ios_base::in | std::ios_base::binary);
    if (ifs)
    {
        util::archive::BinaryIArchive<> ar(ifs);

        try
        {
            ar >> md5_result_ >> config_string_;
            ifs.close();
        }
        catch (...)
        {
            assert(false);
            return;
        }

        if (VerifyMD5())
        {
            GenerateConfig();
        }
    }
}

void Config::SaveConfig()
{
    std::ofstream ofs(config_file_path_.c_str(), std::ios_base::out | std::ios_base::binary);

    if (ofs)
    {
        GenerateConfigString();

        AddMD5();

        util::archive::BinaryOArchive<> ar(ofs);

        try
        {
            ar << md5_result_;
            ar << config_string_;
        }
        catch(...)
        {
            assert(false);
        }

        ofs.close();
    }
}

void Config::SetConfigCount(const std::map<std::string, boost::uint32_t> & config_count)
{
    for (std::map<std::string, boost::uint32_t>::const_iterator iter = config_count.begin();
        iter != config_count.end();
        ++iter)
    {
        assert(iter->second != 0);

        if (config_count_.find(iter->first) == config_count_.end())
        {
            config_count_.insert(std::make_pair(iter->first, iter->second));
        }
        else
        {
            if (config_count_[iter->first] < iter->second)
            {
                config_count_[iter->first] = iter->second;
            }
        }

        if (config_.find(iter->first) != config_.end())
        {
            while(config_[iter->first].size() > iter->second)
            {
                config_[iter->first].pop_front();
            }
        }
    }
}

void Config::GenerateConfig()
{
    try
    {
        namespace po = boost::program_options;

        po::options_description config_desc(SectionName);

        for (std::map<std::string, boost::uint32_t>::const_iterator iter = config_count_.begin();
            iter != config_count_.end();
            ++iter)
        {
            for (boost::uint32_t i = 0; i < iter->second; ++i)
            {
                std::stringstream name;
                name << SectionName << "." << iter->first << "_" << i;
                config_desc.add_options()
                    (name.str().c_str(), po::value<boost::uint32_t>()->default_value(InvalidNumber));
            }
        }

        std::istringstream config_stream(config_string_);

        po::variables_map vm;
        po::store(po::parse_config_file(config_stream, config_desc, true), vm);
        po::notify(vm);

        config_.clear();

        for (std::map<std::string, boost::uint32_t>::const_iterator iter = config_count_.begin();
            iter != config_count_.end();
            ++iter)
        {
            std::list<boost::uint32_t> empty;
            config_.insert(std::make_pair(iter->first, empty));

            for (boost::uint32_t i = 0; i < iter->second; ++i)
            {
                std::stringstream name;
                name << SectionName << "." << iter->first << "_" << i;
                boost::uint32_t value = vm[name.str()].as<boost::uint32_t>();

                if (value != InvalidNumber)
                {
                    config_[iter->first].push_back(value);
                }
            }
        }
    }
    catch (boost::program_options::error & e)
    {
        DebugLog("Exception caught: ", e.what());
        assert(false);
    }
}

void Config::SetConfig(const std::string & name, const std::vector<boost::uint32_t> & data)
{
    assert(config_count_.find(name) != config_count_.end());

    config_[name].clear();

    size_t start = 0;
    if (data.size() > config_count_[name])
    {
        start = data.size() - config_count_[name];
    }

    for(size_t i = start; i < data.size(); ++i)
    {
        config_[name].push_back(data[i]);
    }
}

void Config::GetConfig(const std::string & name, std::vector<boost::uint32_t> & datas)
{
    datas.clear();

    if (config_.find(name) != config_.end())
    {
        for (std::list<boost::uint32_t>::const_iterator iter = config_[name].begin();
            iter != config_[name].end();
            ++iter)
        {
            datas.push_back(*iter);
        }
    }
}

void Config::GenerateConfigString()
{
    std::ostringstream config_stream;
    config_stream << "[" << SectionName << "]" << std::endl;

    for (std::map<string, std::list<boost::uint32_t> >::const_iterator iter = config_.begin();
        iter != config_.end();
        ++iter)
    {
        boost::uint32_t index = 0;

        for (std::list<boost::uint32_t>::const_iterator inner_iter = iter->second.begin();
            inner_iter != iter->second.end();
            ++inner_iter)
        {
            assert(*inner_iter != InvalidNumber);

            config_stream << iter->first << "_" << index++ << " = " << *inner_iter << std::endl;
        }
    }

    config_string_ = config_stream.str();
}

bool Config::VerifyMD5()
{
    return md5_result_ == CalcMd5();
}

void Config::AddMD5()
{
    md5_result_ = CalcMd5();
}

std::string Config::CalcMd5()
{
    framework::string::Md5 md5;
    md5.update(reinterpret_cast<const boost::uint8_t*>(config_string_.c_str()), config_string_.length());
    md5.final();

    return md5.to_string();
}
