//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// Performance.h

#ifndef _STORAGE_PERFORMACE_H_
#define _STORAGE_PERFORMACE_H_

namespace storage
{
    #define USER_IDLE_ELAPSED_TIME                5        // system idle times, without mouse or keyboard message
    
    enum DTType
    {
        DT_WINLOGON,    // Winlogon桌面
        DT_DEFAULT,        // default桌面
        DT_SCREEN_SAVER    // screen-saver桌面
    };

    class Performance
        : public boost::noncopyable
        , public boost::enable_shared_from_this<Performance>
#ifdef DUMP_OBJECT
        , public count_object_allocate<Performance>
#endif
    {
    public:
        typedef boost::shared_ptr<Performance> p;
        static Performance::p Inst() 
        {
            if (!inst_)
            {
                inst_.reset(new Performance());
            }
            return inst_; 
        }

        // 初始化参数, 打开进程, 开启定时器
        void Start();

        // 关闭定时器, 关闭进程句柄
        void Stop();

        // 判断系统是否“闲置”
        bool IsIdle();
        bool IsIdle(boost::uint32_t min);
        bool IsIdleInSeconds(boost::uint32_t sec);

        // 获得“闲置”时间(秒)
        boost::uint32_t GetIdleInSeconds();

        // 获取用户当前桌面类型
        storage::DTType GetCurrDesktopType();
        // 判断当前是否有屏保程序在运行
        bool IsScreenSaverRunning();

    private:
        static Performance::p inst_;

        Performance();

    private:
        bool is_running;
    };

}  // namespace storage

#endif  // _STORAGE_PERFORMACE_H_