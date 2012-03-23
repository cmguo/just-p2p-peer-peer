#ifndef _CONFIG_H_
#define _CONFIG_H_

class Config
{
public:
    static boost::shared_ptr<Config> Inst();

    void GetConfig(const std::string & name, std::vector<boost::uint32_t> & datas);
    void SetConfigCount(const std::map<std::string, boost::uint32_t> & config_count);
    void SetConfig(const std::string & name, const std::vector<boost::uint32_t> & data);
    void LoadConfig();
    void SaveConfig();

private:
    Config();
    Config(const Config & config);

    void GenerateConfig();
    void GenerateConfigString();
    bool VerifyMD5();
    void AddMD5();
    std::string CalcMd5();

private:
    std::string config_file_path_;
    std::string config_string_;
    std::string md5_result_;
    std::map<std::string, boost::uint32_t> config_count_;
    std::map<std::string, std::list<boost::uint32_t> > config_;

    static boost::shared_ptr<Config> inst_;
    static const boost::uint32_t InvalidNumber;
    static const std::string SectionName;
};

#endif  // _CONFIG_H_