/******************************************************************************
*
* Copyright (c) 2012 PPLive Inc.  All rights reserved.
*
* UpnpModule.h
* 
* Description: 端口映射的模块
*             
* 
* --------------------
* 2012-08-23,  kelvinchen create
* --------------------
******************************************************************************/

#ifndef UPNP_MODULE_H_CK_20120823
#define UPNP_MODULE_H_CK_20120823

#include <p2sp/AppModule.h>
#include <set>

namespace p2sp
{
    class UpnpModule
        : public boost::noncopyable
        , public boost::enable_shared_from_this<UpnpModule>
    {        
    public:
        typedef boost::shared_ptr<UpnpModule> p;
    public:
        // 启动 停止
        void Start();
        void Stop();

        //map的前一项是innerport，后一项是希望的export，如果export指定为0，那么就随机，否则必须映射为设置的export
        void AddUpnpPort(const std::map<boost::uint16_t,boost::uint16_t>& tcpPorts,
            const std::map<boost::uint16_t,boost::uint16_t>& udpPorts);

        //获取映射了upnp的tcp port,如果没有，则返回0
        boost::uint16_t GetUpnpExternalTcpPort(boost::uint16_t inPort);

    private:
        UpnpModule();

        void MapPort(const std::map<boost::uint16_t,boost::uint16_t>& tcpPorts,
            const std::map<boost::uint16_t,boost::uint16_t>& udpPorts);

        //获取igd设备的控制信息等
        void GetValidIGD();

        //获得已经映射了的端口
        void GetMappedPort();

        //删除端口映射
        void DoDelPortMapping(boost::uint16_t inPort,boost::uint16_t exPort,const char * proto);

        //添加端口映射，映射失败返回0，映射成功返回对应的外网端口
        boost::uint16_t DoAddPortMapping(boost::uint16_t inPort,boost::uint16_t exPort,const char * desc,const char * proto);

        //通过已有的映射和需要的映射，区分出哪些要删除，哪些要添加       
        void FilterProcess(const std::map<boost::uint16_t,boost::uint16_t>& requirePorts,
            const std::multimap<boost::uint16_t,boost::uint16_t>& mappedPorts,
            std::multimap<boost::uint16_t,boost::uint16_t>& toDelPorts,
            std::map<boost::uint16_t,boost::uint16_t>& toAddPorts);

        //设置统计项
        void SetDelAddStat(const std::multimap<boost::uint16_t,boost::uint16_t>& toDelTcpPorts,
            const std::multimap<boost::uint16_t,boost::uint16_t>& toDelUdpPorts,
            const std::map<boost::uint16_t,boost::uint16_t>& toAddTcpPorts,
            const std::map<boost::uint16_t,boost::uint16_t>& toAddUdpPorts);

        //从multimap里删除指定的key-value对
        bool RemoveFromMutiMap(std::multimap<boost::uint16_t,boost::uint16_t>& mappedPort,
            boost::uint16_t key,boost::uint16_t value);

        void OnTimerElapsed(framework::timer::Timer * pointer);

        //获取igd设备的厂商信息
        void GetManufacturer();

        //促发一次nat类型的检测
        void EvokeNatcheck(boost::uint16_t innerUdpPort,boost::uint16_t exUdpPort);

    private:
        framework::timer::PeriodicTimer upnp_timer_;

        std::map<boost::uint16_t,boost::uint16_t> tcpPorts_;
        std::map<boost::uint16_t,boost::uint16_t> udpPorts_;

        //igd的控制url
        std::string controlUrl_;
        //igd的服务类型
        std::string serviceType_;
        //本机的ip
        std::string landAddr_;
        //上次获取igd设备的时间
        boost::uint32_t lastGetValidIGDTime_;

        //用来获取idgManufacturer_的url
        std::string descUrl_;
        //路由器名称
        std::string idgModName_;
        //是否正在映射中。
        bool isInMapping_;

        //记录第一个要映射的tcpport对应的外网端口
        boost::uint32_t upnpExternalTcpPort_;

        //本机相关的port映射信息，可能不止是pplive相关的映射，每次查询了之后都会添加
        std::multimap<boost::uint16_t,boost::uint16_t> mappedTcpPort_;
        std::multimap<boost::uint16_t,boost::uint16_t> mappedUdpPort_;

        //finalMappedTcpPort_ 保留了 mappedTcpPort_的一个最终的状态。因为mappedTcpPort_在检测过程中会清空的。
        std::multimap<boost::uint16_t,boost::uint16_t> finalMappedTcpPort_;

        static UpnpModule::p inst_;       

    public:
        static UpnpModule::p Inst()
        {
            if (!inst_)
            {
                inst_.reset(new UpnpModule());
            }
            return inst_;
        }
    };
}
#endif