#ifndef P2SP_PUSH_VIDEOHISTORYMANAGER_H
#define P2SP_PUSH_VIDEOHISTORYMANAGER_H

#include "util/serialization/Serialization.h"
#include "util/serialization/stl/deque.h"
#include "protocol/PushServerPacketV2.h"

#include <time.h>

#include <fstream>
#include <string>
#include <set>
#include <deque>
#include <utility>

namespace p2sp
{
#ifdef DISK_MODE

//each object represents a video play record
struct LocalPlayHistoryItem
{
    LocalPlayHistoryItem();
    LocalPlayHistoryItem(const std::string& video_name);

    std::string video_name_;
    boost::uint32_t video_size_;
    boost::uint32_t bitrate_;
    boost::uint32_t start_pos_;
    boost::uint32_t end_pos_;
    time_t play_start_time_;

    void SetPlayPostion(boost::uint32_t play_pos);
    bool IsValidHistoryItem() const;

    boost::uint32_t GetTotalDuration() const;
    boost::uint32_t GetPlayDuration() const;
    time_t GetPlayStartTime() const;

};

template <typename Archive>
void serialize(Archive & ar, LocalPlayHistoryItem & t)
{
    ar & util::serialization::make_sized<boost::uint16_t>(t.video_name_);
    ar & t.video_size_;
    ar & t.bitrate_;
    ar & t.start_pos_;
    ar & t.end_pos_;
    ar & t.play_start_time_;
}

//manager user's watch history(record, save, load)
class PlayHistoryManager 
    : 
    public boost::noncopyable, 
    public boost::enable_shared_from_this<PlayHistoryManager>
{
public:
    typedef boost::shared_ptr<PlayHistoryManager> p;
    static p create(const std::string& path_file)
    {
        return p(new PlayHistoryManager(path_file));
    }

    ~PlayHistoryManager() {}

    typedef LocalPlayHistoryItem* PlayHistoryItemHandle;
    static PlayHistoryItemHandle InvalidHandle() { return NULL; }

    PlayHistoryItemHandle StartVideoPlay(const std::string& video_name);
    void SetVideoPlayPosition(PlayHistoryItemHandle history_handle, boost::uint32_t play_pos);
    void SetVideoSize(PlayHistoryItemHandle history_handle, boost::uint32_t video_size);
    void SetVideoBitrate(PlayHistoryItemHandle history_handle, boost::uint32_t bitrate);
    void StopVideoPlay(PlayHistoryItemHandle history_handle);

    const std::deque<LocalPlayHistoryItem>& GetPlayHistory() const { return play_history_deq_; }

    void LoadFromFile();
    void SaveToFile() const;

private:
    bool LoadFromFileImp(const std::string& path_file);
    void BackupFile(const std::string& from, const std::string& to) const;

    PlayHistoryManager(const std::string& path_file) : path_file_(path_file) {}

private:
    std::string path_file_;

    std::set<PlayHistoryItemHandle> playing_item_set_;

    std::deque<LocalPlayHistoryItem> play_history_deq_;
};

//


//convert local play history to the one push server understand
class LocalHistoryConverter
{
public:
    struct VideoPlayRecordItem
    {
        boost::uint16_t episode_index;
        boost::uint16_t segment_index;
        boost::uint32_t start_time;
        boost::uint32_t duration;
        std::string video_name;

        bool operator < (const VideoPlayRecordItem& r) const
        {
            return start_time < r.start_time;
        }
    };
    
    typedef std::pair<boost::uint16_t, boost::uint16_t> VideoDownloadRecordItem;
    typedef std::map<std::string, std::set<VideoPlayRecordItem> > VideoPlayRecordMap;
    typedef std::map<std::string, std::set<VideoDownloadRecordItem> > VideoDownloadRecordMap;

    template<typename I, typename O>
    static void Convert(I begin, I end, O out)
    {
        VideoPlayRecordMap video_play_record_map;
        VideoDownloadRecordMap video_download_record_map;

        GetVideoPlayRecordMap(begin, end, video_play_record_map);
        GetVideoDownloadRecordMap(video_download_record_map);

        MergePlayRecordAndDownloadRecord(video_play_record_map, video_download_record_map, out);
    }
    
    template<typename O>
    static void MergePlayRecordAndDownloadRecord(const VideoPlayRecordMap& video_play_record_map, 
        const VideoDownloadRecordMap& video_download_record_map, O out)
    {
        std::map<boost::uint32_t, protocol::PlayHistoryItem> time2_history_map;
        VideoPlayRecordMap::const_iterator iter;
        for (iter = video_play_record_map.begin(); iter != video_play_record_map.end(); ++iter) {
            BOOST_ASSERT(iter->second.size() > 0);
            boost::uint32_t duration = GetContinuouslyPlayedDuration(iter->second);
            const VideoPlayRecordItem& play_record = *iter->second.rbegin();
            boost::uint16_t segment_num = 0;//buffered segment number since last play point
            VideoDownloadRecordMap::const_iterator download_record_iter = video_download_record_map.find(iter->first);
            if (download_record_iter != video_download_record_map.end()) {
                segment_num = GetDownloadedSegmentNum(play_record.episode_index, play_record.segment_index, download_record_iter->second);
            }
            time2_history_map[play_record.start_time] = protocol::PlayHistoryItem(play_record.video_name, duration, segment_num);
        }
        
        std::map<boost::uint32_t, protocol::PlayHistoryItem>::reverse_iterator time_map_iter;
        for (time_map_iter = time2_history_map.rbegin(); time_map_iter != time2_history_map.rend(); time_map_iter++) {
            out++ = time_map_iter->second;
        }
    }
    
    static bool IsAdjacentSegment(boost::uint16_t episode_index1, boost::uint16_t segment_index1, 
        boost::uint16_t episode_index2, boost::uint16_t segment_index2);

    //get video name, episode and segment information from file name
    static bool GetVideoInfo(const std::string& raw_name, std::string& name, 
        boost::uint16_t& episode_index, boost::uint16_t& segment_index);

    //get continuously played duration of some video from play history
    static boost::uint32_t GetContinuouslyPlayedDuration(const std::set<VideoPlayRecordItem>& play_record_set);
    
    //get downloaded segment number of some video since last play position
    static boost::uint16_t GetDownloadedSegmentNum(boost::uint16_t episode_index, boost::uint16_t segment_index, 
        const std::set<VideoDownloadRecordItem>& download_record_set);

private:
    template<typename I>
    static void GetVideoPlayRecordMap(I begin, I end, std::map<std::string, std::set<VideoPlayRecordItem> >& play_record_map)
    {
        for (I iter = begin; iter != end; ++iter) {
            if (iter->IsValidHistoryItem()) {
                std::string name;
                boost::uint16_t episode_index;
                boost::uint16_t segment_index;

                if (GetVideoInfo(iter->video_name_, name, episode_index, segment_index)) {
                    VideoPlayRecordItem play_record;
                    play_record.episode_index = episode_index;
                    play_record.segment_index = segment_index;
                    play_record.video_name = iter->video_name_;
                    play_record.start_time = iter->GetPlayStartTime();
                    play_record.duration = iter->GetPlayDuration();
                    play_record_map[name].insert(play_record);
                }
            }
        }
    }

    static void GetVideoDownloadRecordMap(std::map<std::string, std::set<VideoDownloadRecordItem> >& download_record_map);

};



#endif
}

#endif