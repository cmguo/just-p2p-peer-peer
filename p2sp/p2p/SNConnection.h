//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef SN_CONNECTION_H
#define SN_CONNECTION_H

#include "p2sp/p2p/ConnectionBase.h"

namespace p2sp
{
    class P2PDownloader;
    typedef boost::shared_ptr<P2PDownloader> P2PDownloader__p;

    class SubPieceRequestManager;
    typedef boost::shared_ptr<SubPieceRequestManager> SubPieceRequestManager__p;

    class SNConnection
        : public boost::noncopyable
        , public ConnectionBase
    {
    public:
        SNConnection(P2PDownloader__p p2p_downloader,
            const boost::asio::ip::udp::endpoint & end_point)
            : ConnectionBase(p2p_downloader, end_point)
            , is_subpiece_requested_(false)
        {
        }
        
        void Start();

        virtual void Stop();
        virtual void OnP2PTimer(boost::uint32_t times);
        virtual bool HasRidInfo() const {return true;}
        virtual bool IsRidInfoValid() const {return true;}
        virtual bool CanKick() {return false;}
        virtual bool HasSubPiece(const protocol::SubPieceInfo & subpiece_info) {return true;}


        virtual bool HasBlock(boost::uint32_t block_index) {return true;}
        virtual bool IsBlockFull() {return true;}

        virtual bool LongTimeNoSee() {return false;}
        virtual void KeepAlive() {}

        virtual void SendPacket(const std::vector<protocol::SubPieceInfo> & subpieces,
            boost::uint32_t copy_count);

        virtual void SubmitDownloadedBytes(boost::uint32_t length);

        virtual void SubmitP2PDataBytesWithoutRedundance(boost::uint32_t length);
        virtual void SubmitP2PDataBytesWithRedundance(boost::uint32_t length);
        
        void RequestTillFullWindow(bool need_check = false);

        virtual boost::uint32_t GetConnectRTT() const;

    private:
        bool is_subpiece_requested_;
    };
}

#endif