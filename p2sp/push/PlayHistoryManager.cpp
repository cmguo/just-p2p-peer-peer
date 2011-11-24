#include "Common.h"
#include "PlayHistoryManager.h"
#include "storage/Storage.h"

#include <algorithm>

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>

#include <util/archive/BinaryIArchive.h>
#include <util/archive/BinaryOArchive.h>


namespace p2sp
{
#ifdef DISK_MODE

const static boost::uint32_t INVALID_PLAY_POS = (boost::uint32_t)-1;
const static boost::uint32_t MAX_PLAY_HISTORY_NUM = 500;

const static std::string BAK_EXT_NAME = ".bak";

LocalPlayHistoryItem::LocalPlayHistoryItem()
:
video_size_(0),
bitrate_(0),
start_pos_(INVALID_PLAY_POS),
end_pos_(INVALID_PLAY_POS),
play_start_time_(time(0))
{
}

LocalPlayHistoryItem::LocalPlayHistoryItem(const std::string& video_name)
:
video_name_(video_name),
video_size_(0),
bitrate_(0),
start_pos_(INVALID_PLAY_POS),
end_pos_(INVALID_PLAY_POS),
play_start_time_(time(0))
{
}

bool LocalPlayHistoryItem::IsValidHistoryItem() const
{
    return start_pos_ != INVALID_PLAY_POS && end_pos_ != INVALID_PLAY_POS && video_size_ != 0 && bitrate_ != 0;
}

void LocalPlayHistoryItem::SetPlayPostion(boost::uint32_t play_pos)
{
    if (INVALID_PLAY_POS == start_pos_) {
        start_pos_ = play_pos;
    }
    else {
        end_pos_ = play_pos;
    }
}

boost::uint32_t LocalPlayHistoryItem::GetTotalDuration() const
{
    if (bitrate_ != 0) {
        return video_size_/bitrate_;
    }
    else {
        return 0;
    }
}

boost::uint32_t LocalPlayHistoryItem::GetPlayDuration() const
{
    if (bitrate_ != 0) {
        return (end_pos_-start_pos_)/bitrate_;
    }
    else {
        return 0;
    }
}

time_t LocalPlayHistoryItem::GetPlayStartTime() const
{
    return play_start_time_;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PlayHistoryManager::PlayHistoryItemHandle PlayHistoryManager::StartVideoPlay(const std::string& video_name)
{
    PlayHistoryItemHandle hItem = new LocalPlayHistoryItem(video_name);
    
    playing_item_set_.insert(hItem);

    return hItem;
}

void PlayHistoryManager::SetVideoPlayPosition(PlayHistoryManager::PlayHistoryItemHandle history_handle, boost::uint32_t play_pos)
{
    BOOST_ASSERT(playing_item_set_.find(history_handle) != playing_item_set_.end());

    history_handle->SetPlayPostion(play_pos);
}

void PlayHistoryManager::SetVideoSize(PlayHistoryManager::PlayHistoryItemHandle history_handle, boost::uint32_t video_size)
{
    BOOST_ASSERT(playing_item_set_.find(history_handle) != playing_item_set_.end());

    history_handle->video_size_ = video_size;
}

void PlayHistoryManager::SetVideoBitrate(PlayHistoryManager::PlayHistoryItemHandle history_handle, boost::uint32_t bitrate)
{
    BOOST_ASSERT(playing_item_set_.find(history_handle) != playing_item_set_.end());

    history_handle->bitrate_ = bitrate;
}

void PlayHistoryManager::StopVideoPlay(PlayHistoryManager::PlayHistoryItemHandle history_handle)
{
    BOOST_ASSERT(playing_item_set_.find(history_handle) != playing_item_set_.end());

    if (history_handle->IsValidHistoryItem()) {
        play_history_deq_.push_front(*history_handle);
        SaveToFile();
    }
    
    playing_item_set_.erase(history_handle);
    delete history_handle;
}

void PlayHistoryManager::LoadFromFile()
{
    std::string backup_file = path_file_ + BAK_EXT_NAME;
    if (!LoadFromFileImp(path_file_)) {
        if (LoadFromFileImp(backup_file)) {
            BackupFile(backup_file, path_file_);
        }
    }
    else {
        BackupFile(path_file_, backup_file);
    }

    if (play_history_deq_.size() > MAX_PLAY_HISTORY_NUM) {
        play_history_deq_.resize(MAX_PLAY_HISTORY_NUM);
    }
}

bool PlayHistoryManager::LoadFromFileImp(const std::string& path_file)
{
    std::ifstream ifs(path_file.c_str());
    util::archive::BinaryIArchive<> bia(ifs);

    BOOST_ASSERT(play_history_deq_.size() == 0);

    play_history_deq_.clear();
    bia & play_history_deq_;
    
    if (play_history_deq_.size() > 0) {
        return true;
    }

    return bia;
}

void PlayHistoryManager::BackupFile(const std::string& from, const std::string& to) const
{
    boost::system::error_code ec;
    boost::filesystem::remove(boost::filesystem::path(to), ec);
    try {
        boost::filesystem::copy_file(boost::filesystem::path(from), boost::filesystem::path(to));
    }
    catch(std::exception& e) {
    }
}

void PlayHistoryManager::SaveToFile() const
{
    std::ofstream ofs(path_file_.c_str());
    util::archive::BinaryOArchive<> boa(ofs);

    boa & play_history_deq_;
    if (boa) {
        ofs.close();
        BackupFile(path_file_, path_file_ + BAK_EXT_NAME);
    }
}
    
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LocalHistoryConverter::GetVideoDownloadRecordMap(std::map<std::string, std::set<VideoDownloadRecordItem> >& download_record_map)
{
    std::vector<std::string> file_name_vec;
    storage::Storage::Inst_Storage()->GetAllCompletedFiles(file_name_vec);

    for (boost::uint32_t i = 0; i < file_name_vec.size(); i++) {
        std::string name;
        boost::uint16_t episode_index;
        boost::uint16_t segment_index;

        if (GetVideoInfo(file_name_vec[i], name, episode_index, segment_index)) {
            download_record_map[name].insert(std::make_pair(episode_index, segment_index));
        }
    }
}

boost::uint32_t LocalHistoryConverter::GetContinuouslyPlayedDuration(const std::set<VideoPlayRecordItem>& play_record_set)
{
    BOOST_ASSERT(play_record_set.size() > 0);

    std::set<VideoPlayRecordItem>::const_reverse_iterator pre_iter = play_record_set.rbegin();
    std::set<VideoPlayRecordItem>::const_reverse_iterator cur_iter = play_record_set.rbegin();

    boost::uint32_t duration = pre_iter->duration;
    
    while (++cur_iter != play_record_set.rend()) {
        if (cur_iter->episode_index == pre_iter->episode_index && cur_iter->segment_index == pre_iter->segment_index) {
            //same segment
            duration += cur_iter->duration;
        }
        else if (IsAdjacentSegment(cur_iter->episode_index, cur_iter->segment_index, pre_iter->episode_index, pre_iter->segment_index)) {
            duration += cur_iter->duration;
        }
        else {
            break;
        }
        pre_iter = cur_iter;
    }

    return duration;
}

boost::uint16_t LocalHistoryConverter::GetDownloadedSegmentNum(boost::uint16_t episode_index, boost::uint16_t segment_index, 
                                        const std::set<VideoDownloadRecordItem>& download_record_set)
{
    boost::uint16_t segment_num = 0;

    std::set<VideoDownloadRecordItem>::const_iterator iter = download_record_set.find(std::make_pair(episode_index, segment_index));
    while(iter != download_record_set.end()) {
        if (iter->first == episode_index && iter->second == segment_index) {
            ++segment_num;
            ++iter;
        }
        else if (IsAdjacentSegment(episode_index, segment_index, iter->first, iter->second)) {
            episode_index = iter->first;
            segment_index = iter->second;
            ++segment_num;
            ++iter;
        }
        else {
            break;
        }
    }

    return segment_num;
}

bool LocalHistoryConverter::IsAdjacentSegment(boost::uint16_t episode_index1, boost::uint16_t segment_index1, boost::uint16_t episode_index2, boost::uint16_t segment_index2)
{
    if (episode_index1 == episode_index2) {
        return segment_index2 == segment_index1 + 1;
    }
    else {
        return episode_index2 == episode_index1 + 1 && segment_index2 == 0;
    }
}

bool LocalHistoryConverter::GetVideoInfo(const std::string& raw_name, std::string& name, boost::uint16_t& episode_index, boost::uint16_t& segment_index)
{
    std::string reg_exp1 = "(.+)\\(µÚ(\\d+)¼¯\\).*\\[(\\d+)\\]\\..+";
    std::string reg_exp2 = "(.+)\\[(\\d+)\\]\\..+";

    try {
        boost::smatch what1;
        if (boost::regex_match(raw_name, what1, boost::regex(reg_exp1))) {
            name.assign(what1[1].first, what1[1].second);
            episode_index = boost::lexical_cast<boost::uint16_t>(std::string(what1[2].first, what1[2].second));
            segment_index = boost::lexical_cast<boost::uint16_t>(std::string(what1[3].first, what1[3].second));
            return true;
        }

        boost::smatch what2;
        if (boost::regex_match(raw_name, what2, boost::regex(reg_exp2))) {
            name.assign(what2[1].first, what2[1].second);
            episode_index = 0;
            segment_index =  boost::lexical_cast<boost::uint16_t>(std::string(what2[2].first, what2[2].second));
            return true;
        }

        return false;
    }
    catch(boost::bad_lexical_cast& e) {
        return false;
    }
}

#endif


}