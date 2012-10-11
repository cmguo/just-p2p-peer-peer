//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "network/upnp/UpnpModule.h"
#include "network/upnp/UpnpThread.h"

#include "statistic/DACStatisticModule.h"
#include "network/upnp/miniupnpc/miniupnpc.h"
#include "network/upnp/miniupnpc/upnpcommands.h"
#include "network/upnp/miniupnpc/upnperrors.h"
#include <algorithm>
using namespace std;
using namespace boost;

using namespace framework::network;


namespace p2sp
{
    UpnpModule::p UpnpModule::inst_;

#ifdef LOG_ENABLE
    static log4cplus::Logger logger_upnp = log4cplus::Logger::getInstance("[upnp_module]");
#endif

    enum UPNP_STAT
    {
        //发现设备失败
        STAT_DISCOVER_FAIL = 1,
        //找不到gid设备
        STAT_GETGID_FAIL,
        //需要删除多余的端口
        STAT_DEL_ONLY,
        //需要删除多余的端口后再映射
        STAT_DEL_ADD,
        //只需要映射，无需删除
        STAT_ADD_ONLY,
        //无需删除，也无需映射
        STAT_NO_DEL_ADD,
    };

    UpnpModule::UpnpModule()
        :upnp_timer_(global_second_timer(), 60 * 1000, boost::bind(&UpnpModule::OnTimerElapsed, this, &upnp_timer_))
        ,isInMapping_(false),lastGetValidIGDTime_(0)
    {
        
    }    

    void UpnpModule::Start()
    {
        upnp_timer_.start();
        UpnpThread::Inst().Start();       
    }

    void UpnpModule::Stop()
    {
        upnp_timer_.stop();
        UpnpThread::Inst().Stop();
    }


    void UpnpModule::AddUpnpPort(const std::map<boost::uint16_t,boost::uint16_t>& tcpPorts,
        const std::map<boost::uint16_t,boost::uint16_t>& udpPorts)
    {
        tcpPorts_.insert(tcpPorts.begin(),tcpPorts.end());
        udpPorts_.insert(udpPorts.begin(),udpPorts.end());
        UpnpThread::IOS().post(boost::bind(&UpnpModule::MapPort, UpnpModule::Inst(), tcpPorts_,udpPorts_));        
    }

    void UpnpModule::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        UpnpThread::IOS().post(boost::bind(&UpnpModule::MapPort, UpnpModule::Inst(), tcpPorts_,udpPorts_));
    }

    boost::uint16_t UpnpModule::GetUpnpExternalTcpPort(boost::uint16_t inPort)
    {
        if(finalMappedTcpPort_.count(inPort) > 0)
        {
            //如果DoDelPortMapping里出现了失败的情况，那么finalMappedTcpPort_.count(inPort) 就可能大于1，正常就是等于1
            return finalMappedTcpPort_.lower_bound(inPort)->second;
        }
        else
        {
            return 0;
        }
    }

        //获取路由器的控制信息
    void UpnpModule::GetValidIGD()
    {
        if((time(NULL) -lastGetValidIGDTime_ > 600)||controlUrl_.empty()||serviceType_.empty()||landAddr_.empty())
        {
            //超过600秒，才考虑重新获取路由器的信息
            lastGetValidIGDTime_ = time(NULL);
        }
        else
        {
            return;
        }

        UPNPDev* structDeviceList= upnpDiscover(5000, NULL, NULL, 0);
        if (NULL == structDeviceList)
        {
            statistic::DACStatisticModule::Inst()->SubmitUpnpStat(STAT_DISCOVER_FAIL);

            LOG4CPLUS_INFO_LOG(logger_upnp, "upnpDiscover failed");
            //没有UPnP设备发现
            return;
        }
        else
        {
            LOG4CPLUS_DEBUG_LOG(logger_upnp, "upnpDiscover success");
            //发现upnp设备
        }


        UPNPUrls upnpUrls;
        IGDdatas igdData;
        //遍历upnp设备，找到IGD
        UPNPDev* pDevice = NULL;
        for(pDevice = structDeviceList; pDevice != NULL; pDevice = pDevice->pNext)
        {               
            memset(&upnpUrls,0,sizeof(UPNPUrls));            
            memset(&igdData,0,sizeof(IGDdatas));
            char	lanaddr[16]={0};
            //     1 = A valid connected IGD has been found
            //     2 = A valid IGD has been found but it reported as not connected

            //底层是通过getsockname获取到的lanaddr
            int iResult = UPNP_GetValidIGD(structDeviceList, &upnpUrls, &igdData, lanaddr, sizeof(lanaddr));
            if(iResult == 1)
            {
                //找到IGD设备了
                controlUrl_ = upnpUrls.controlURL;
                serviceType_ =  igdData.servicetype;
                landAddr_ = lanaddr;
                break;
            }
        }
        if(pDevice != NULL)
        {
            //找到了设备
            LOG4CPLUS_DEBUG_LOG(logger_upnp, "UPNP_GetValidIGD success");
        }
        else
        {
            //没有找到设备       
            statistic::DACStatisticModule::Inst()->SubmitUpnpStat(STAT_DISCOVER_FAIL);
            LOG4CPLUS_INFO_LOG(logger_upnp, "UPNP_GetValidIGD failed");
        }

        //释放内存
        freeUPNPDevlist(structDeviceList);     
        FreeUPNPUrls(&upnpUrls);      
    }

    void UpnpModule::MapPort(const std::map<boost::uint16_t,boost::uint16_t>& tcpPorts,
        const std::map<boost::uint16_t,boost::uint16_t>& udpPorts)
    {
        if(tcpPorts.empty() && udpPorts.empty())
        {
            LOG4CPLUS_INFO_LOG(logger_upnp, "don't need to map");
            return;
        }
        if(isInMapping_)
        {
            //正在映射中，不映射了。定时器的时间到了，但是上次映射还没有完成就会走到这里
            return;
        }

        isInMapping_ = true;
        GetValidIGD();        

        if(controlUrl_.empty()||serviceType_.empty()||landAddr_.empty())
        {
            //如果没有路由器的信息，那么就返回吧
            isInMapping_ = false;
            return;
        }

        //找到已经映射了的集合
        GetMappedPort();

        std::multimap<boost::uint16_t,boost::uint16_t> toDelTcpPorts,toDelUdpPorts;
        std::map<boost::uint16_t,boost::uint16_t> toAddTcpPorts,toAddUdpPorts;

        FilterProcess(tcpPorts,mappedTcpPort_,toDelTcpPorts,toAddTcpPorts);

        FilterProcess(udpPorts,mappedUdpPort_,toDelUdpPorts,toAddUdpPorts);

        SetDelAddStat(toDelTcpPorts,toDelUdpPorts,toAddTcpPorts,toAddUdpPorts);
       
        for(std::multimap<boost::uint16_t,boost::uint16_t>::const_iterator it =  toDelTcpPorts.begin(); it != toDelTcpPorts.end();++it)
        {
            DoDelPortMapping(it->first,it->second,"TCP");
        }    

        for(std::multimap<boost::uint16_t,boost::uint16_t>::const_iterator it =  toDelUdpPorts.begin(); it != toDelUdpPorts.end();++it)
        {
            DoDelPortMapping(it->first,it->second,"UDP");
        } 

        //上面的两个DoDel如果都成功的话，那么mappedTcpPort_和mappedUdpPort_的key就不会有重复的了。multimap实际上就是map了。

        for(std::map<boost::uint16_t,boost::uint16_t>::const_iterator it =  toAddTcpPorts.begin(); it != toAddTcpPorts.end();++it)
        {
            DoAddPortMapping(it->first,it->second,"PPLive","TCP");
        }    

        for(std::map<boost::uint16_t,boost::uint16_t>::const_iterator it =  toAddUdpPorts.begin(); it != toAddUdpPorts.end();++it)
        {
            DoAddPortMapping(it->first,it->second,"PPLive","UDP");
        } 
        
        isInMapping_ = false;   

        finalMappedTcpPort_ = mappedTcpPort_;

        return;
    }

    void UpnpModule::FilterProcess(const std::map<boost::uint16_t,boost::uint16_t>& requirePorts,
            const std::multimap<boost::uint16_t,boost::uint16_t>& mappedPorts,
            std::multimap<boost::uint16_t,boost::uint16_t>& toDelPorts,
            std::map<boost::uint16_t,boost::uint16_t>& toAddPorts)
    {
        //找到需要添加和需要删除的集合,规则：1.如果要求映射的export不为0，那么把export不正确的都删掉
        //2,如果要求映射的export为0，那么：A原来已经映射大于1个，都删掉，重新映射，B原来已经映射等于1个，ok了，不删也不加
        for(std::map<uint16_t,uint16_t>::const_iterator it = requirePorts.begin();it!=requirePorts.end();++it)
        {
            if(it->second != 0)
            {
                //指定了要映射的export
                for(multimap<uint16_t,uint16_t>::const_iterator itdel = mappedPorts.lower_bound(it->first);
                    itdel != mappedPorts.upper_bound(it->first);++itdel)
                {
                    if(itdel->second != it->second)
                    {
                        toDelPorts.insert(make_pair(itdel->first,itdel->second));
                    }
                }
                if(toDelPorts.count(it->first) != mappedPorts.count(it->first))
                {
                    assert(toDelPorts.count(it->first)+1 == mappedPorts.count(it->first));
                    //删除的数目和已有映射的数目不同，说明已有的不是全被删除的，即requirePorts里头的正在mappedPorts里面
                }
                else
                {
                    toAddPorts[it->first]= it->second;
                }
            }
            else
            {
                //没有指定export
                if(mappedPorts.count(it->first) > 1)
                {
                    toDelPorts.insert(mappedPorts.lower_bound(it->first),mappedPorts.upper_bound(it->first));
                    toAddPorts[it->first]= it->second;
                }
                else if(mappedPorts.count(it->first) == 1)
                {
                    //不用删除和添加了
                }
                else 
                {
                    //已经映射的集合里没有，需要新建映射
                    toAddPorts[it->first]= it->second;
                }
            }
        }

    }

    void UpnpModule::SetDelAddStat(const std::multimap<boost::uint16_t,boost::uint16_t>& toDelTcpPorts,
        const std::multimap<boost::uint16_t,boost::uint16_t>& toDelUdpPorts,
        const std::map<boost::uint16_t,boost::uint16_t>& toAddTcpPorts,
        const std::map<boost::uint16_t,boost::uint16_t>& toAddUdpPorts)
    {
         if(toDelTcpPorts.empty() && toDelUdpPorts.empty() && ( !toAddUdpPorts.empty() || !toAddTcpPorts.empty()) )
        {
            statistic::DACStatisticModule::Inst()->SubmitUpnpStat(STAT_ADD_ONLY);
        }
        else if((!toDelTcpPorts.empty() || !toDelUdpPorts.empty()) && toAddUdpPorts.empty() && toAddTcpPorts.empty())
        {
            statistic::DACStatisticModule::Inst()->SubmitUpnpStat(STAT_DEL_ONLY);
        }
        else if(toAddUdpPorts.empty() && toAddTcpPorts.empty() && toDelTcpPorts.empty() && toDelUdpPorts.empty())
        {
             statistic::DACStatisticModule::Inst()->SubmitUpnpStat(STAT_NO_DEL_ADD);
        }
        else if((!toDelTcpPorts.empty() || !toDelUdpPorts.empty()) && (!toAddUdpPorts.empty() || !toAddTcpPorts.empty()))
        {
            statistic::DACStatisticModule::Inst()->SubmitUpnpStat(STAT_DEL_ADD);
        }
        else
        {
            //不该走到这里的
            assert(false);
        }

    }

    void UpnpModule::GetMappedPort()
    {
        //获得已经映射过的port
        //有这个函数，UPNP_GetPortMappingNumberOfEntries，可以获取映射的port的个数，但是不是所有的路由器都支持
        unsigned maxindex = 1024;
        if(UPNPCOMMAND_SUCCESS != UPNP_GetPortMappingNumberOfEntries(controlUrl_.c_str(),serviceType_.c_str(),&maxindex))
        {
            maxindex = 1024;
        }

        unsigned index = 0;

        mappedTcpPort_.clear();
        mappedUdpPort_.clear();

        //假设不超过1024个映射
        while(index<maxindex)
        {
            char cIndex[10]={0};
            sprintf(cIndex, "%u", index);
            ++index;
            char protocol[10]={0};
            char inport[10]={0};
            char exPort[10]={0};

            //TODO 需要调试看看enable是什么东西
            char enable[10]={0};
            char inclient[20]={0};

            int ret = UPNP_GetGenericPortMappingEntry(controlUrl_.c_str(),serviceType_.c_str(),
                cIndex,exPort,inclient,inport,protocol,NULL,enable,NULL,NULL);
            if( UPNPCOMMAND_SUCCESS == ret)
            {
                //todo:转换成大写,要判断inclient是否是本地的inclient
                std::transform(&protocol[0], protocol+strlen(protocol),&protocol[0], ::toupper);
                if(strcmp(inclient,landAddr_.c_str()) != 0 || strcmp(enable,"1") !=0)
                {
                    //不是本机ip相关的信息
                    continue;
                }

                if(strcmp(protocol,"TCP") == 0)
                {
                    //mappedTcpPort_[atoi(inport)] = atoi(exPort);
                    mappedTcpPort_.insert(std::make_pair(atoi(inport),atoi(exPort)));
                }
                else if(strcmp(protocol,"UDP") == 0)
                {
                    //mappedUdpPort_[atoi(inport)] = atoi(exPort);
                    mappedUdpPort_.insert(std::make_pair(atoi(inport),atoi(exPort)));
                }
                else
                {
                    LOG4CPLUS_INFO_LOG(logger_upnp, "unknown protocol:"<<protocol);
                }
            }
            else
            {
                LOG4CPLUS_INFO_LOG(logger_upnp, "UPNP_GetGenericPortMappingEntry failed:"<<ret);
                break;
            }
        }
       
    }

    //从multimap里移除指定的 （key，value）
    bool UpnpModule::RemoveFromMutiMap(multimap<boost::uint16_t,boost::uint16_t>& mappedPort,boost::uint16_t key,boost::uint16_t value)
    {
        for(multimap<boost::uint16_t,boost::uint16_t>::iterator it = mappedPort.lower_bound(key);it != mappedPort.upper_bound(key);++key)
        {
            if(value == it->second)
            {
                mappedPort.erase(it);
                return true;
            }
        }
        return false;
    }

    void UpnpModule::DoDelPortMapping(boost::uint16_t inPort,boost::uint16_t exPort,const char * proto)
    {
        LOG4CPLUS_DEBUG_LOG(logger_upnp, "to delete  inport:"<<inPort<<"export:"<<exPort<<" proto:"<<proto);

        char cExPort[10]={0};
        sprintf(cExPort, "%u", exPort);

        char cInPort[10]={0};
        sprintf(cInPort, "%u", inPort);

        int ret = UPNP_DeletePortMapping(controlUrl_.c_str(),serviceType_.c_str(),cExPort,proto,NULL);
        if(ret != 0)
        {
             LOG4CPLUS_INFO_LOG(logger_upnp, "UPNP_DeletePortMapping failed ,return:"<<ret);
        }
        else
        {
            if(strcmp("TCP",proto)==0)
            {
                RemoveFromMutiMap(mappedTcpPort_,inPort,exPort);
            }
            else
            {
                RemoveFromMutiMap(mappedUdpPort_,inPort,exPort);
            }
        }       
    }

    void UpnpModule::DoAddPortMapping(boost::uint16_t inPort,boost::uint16_t exPort,const char * desc,const char * proto)
    {
        LOG4CPLUS_DEBUG_LOG(logger_upnp, "to add inport:"<<inPort<<"export:"<<exPort<<"desc:"<< desc <<" proto:"<<proto);
        
        char cInPort[10]={0};
        sprintf(cInPort, "%u", inPort);

        int ret = -1;
        int retryTimes = 0;
        while(ret != 0 && retryTimes++<5)
        {
            srand(time(NULL) + retryTimes);
            if(0 == exPort)
            {
                //没有指定要映射的port，就随机生成一个
                exPort = rand() % 30000 + 2000;
            }
            char cExPort[10]={0};
            sprintf(cExPort, "%u", exPort);
            ret = UPNP_AddPortMapping(controlUrl_.c_str(),serviceType_.c_str(),cExPort,cInPort,landAddr_.c_str(),desc,proto,NULL);
            if(ret != 0)
            {
                LOG4CPLUS_INFO_LOG(logger_upnp, "UPNP_AddPortMapping failed ,return:"<<ret);
            }  
            else
            {
                if(strcmp("UDP",proto)==0)
                {
                    //mappedUdpPort_[inPort] = exPort;
                    mappedUdpPort_.insert(std::make_pair(inPort,exPort));
                }
                else
                {
                    mappedTcpPort_.insert(std::make_pair(inPort,exPort));
                    //mappedTcpPort_[inPort] = exPort;
                }
            }
        }

        statistic::DACStatisticModule::Inst()->SubmitUpnpPortMapping(strcmp("TCP",proto)==0,ret == 0);         
    }
}
