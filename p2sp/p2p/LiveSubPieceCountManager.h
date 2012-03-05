#ifndef LIVE_SUBPIECE_COUNT_MANAGER_H
#define LIVE_SUBPIECE_COUNT_MANAGER_H

namespace p2sp
{
    class LiveSubPieceCountManager
    {
    public:
        LiveSubPieceCountManager(boost::uint32_t live_interval);
        void SetSubPieceCountMap(boost::uint32_t block_id, const std::vector<boost::uint16_t> & subpiece_count);
        void EliminateElapsedSubPieceCountMap(boost::uint32_t block_id);
        bool HasSubPieceCount(boost::uint32_t piece_id) const;
        boost::uint16_t GetSubPieceCount(boost::uint32_t block_id) const;

    private:
        map<uint32_t, uint16_t> subpiece_count_map_;
        boost::uint32_t live_interval_;
    };
}

#endif