#include "imgedit.h"

#include <iomanip>
#include <cctype>
#include <cstring>
#include <ctime>
#include <cstddef>

inline uint32_t ImageEditor::GetFatPosition() const {
  return boot_.bpb.reserved_sector_count * boot_.bpb.bytes_per_sector;
}

inline uint32_t ImageEditor::GetFatSize() const {
  return boot_.bpb.sectors_per_table * boot_.bpb.bytes_per_sector;
}

inline uint32_t ImageEditor::GetFatCount() const {
  return boot_.bpb.table_count;
}

inline uint32_t ImageEditor::GetDirEntryPosition() const {
  return GetFatPosition() + GetFatSize() * GetFatCount();
}

inline uint32_t ImageEditor::GetDirEntryCount() const {
  return boot_.bpb.root_entry_count;
}

inline uint32_t ImageEditor::GetDataPosition() const {
  return GetDirEntryPosition() + GetDirEntryCount() * sizeof(DirEntry);
}

inline uint32_t ImageEditor::GetDataSectorCount() const {
  return (boot_.bpb.sector_count_16 ? boot_.bpb.sector_count_16
                                    : boot_.bpb.sector_count_32) -
         (GetDataPosition() / boot_.bpb.bytes_per_sector);
}

inline uint32_t ImageEditor::GetClusterSize() const {
  return boot_.bpb.sectors_per_cluster * boot_.bpb.bytes_per_sector;
}

inline uint16_t ImageEditor::GetNextCluster(uint16_t i) const {
  int pos = i + (i >> 1);
  uint16_t word = (fat_[pos + 1] << 8) | fat_[pos];
  return i % 2 ? word >> 4 : word & 0x0fff;
}

void ImageEditor::SetNextCluster(uint16_t i, uint16_t value) {
  int pos = i + (i >> 1);
  uint16_t word = (fat_[pos + 1] << 8) | fat_[pos];
  if (i % 2) {
    word = (value << 4) | (word & 0x000f);
  }
  else {
    word = (word & 0xf000) | (value & 0x0fff);
  }
  fat_[pos + 1] = word >> 8;
  fat_[pos] = word & 0xff;
  // write back to image
  for (uint32_t i = 0; i < GetFatCount(); ++i) {
    fs_.seekp(GetFatPosition() + i * GetFatSize() + pos);
    fs_.write(reinterpret_cast<char *>(fat_.data()) + pos, 2);
  }
}

inline uint16_t ImageEditor::FindNextCluster(uint16_t start) {
  for (int i = start; i < GetFatSize() / 1.5; ++i) {
    if (GetNextCluster(i) == 0x000) return i;
  }
  return 0xff7;
}

inline bool ImageEditor::InitDirEntry(ImageEditor::DirEntry &dir,
                                      const std::string &filename) {
  if (!GetFileName(filename, dir.name)) return false;
  // check if file already exists
  for (const auto &i : dirs_) {
    if (!std::memcmp(i.name, dir.name, 11)) return false;
  }
  // fill the rest fields
  dir.attributes = 0x20;  // archive
  dir.reserved[0] = dir.reserved[1] = 0;
  GetFatTime(dir.creation_time);
  GetFatDate(dir.creation_date);
  GetFatDate(dir.last_access_date);
  dir.zero = 0;
  GetFatTime(dir.last_mod_time);
  GetFatDate(dir.last_mod_date);
  return true;
}

inline bool ImageEditor::FindNextDirEntry(uint32_t start) {
  fs_.seekg(start);
  int final_pos = GetDataPosition();
  while (fs_.tellg() < final_pos) {
    DirEntry current;
    fs_.read(reinterpret_cast<char *>(&current), sizeof(DirEntry));
    if (IsDirEntryEmpty(current)) {
      last_dir_pos_ = fs_.tellg();
      last_dir_pos_ -= sizeof(DirEntry);
      return true;
    }
  }
  return false;
}

inline void ImageEditor::GetFatTime(FatTime &fat_time) {
  auto now = std::time(nullptr);
  auto local = std::localtime(&now);
  fat_time.hour = local->tm_hour;
  fat_time.minute = local->tm_min;
  fat_time.second = local->tm_sec;
}

inline void ImageEditor::GetFatDate(FatDate &fat_date) {
  auto now = std::time(nullptr);
  auto local = std::localtime(&now);
  fat_date.year = local->tm_year + 1900 - 1980;
  fat_date.month = local->tm_mon;
  fat_date.day = local->tm_mday;
}

inline bool ImageEditor::IsValidBootSector() const {
  if (std::strncmp(reinterpret_cast<const char *>(boot_.file_system_id),
                   "FAT12   ", 8))
    return false;
  if (boot_.bpb.reserved_sector_count != 1 || boot_.bpb.table_count == 0 ||
      boot_.bpb.hidden_sector_count) return false;
  return true;
}

inline bool ImageEditor::IsValidFat() const {
  return fat_[0] == 0xf0 && fat_[1] == 0xff && fat_[2] == 0xff;
}

bool ImageEditor::IsDirEntryEmpty(const DirEntry &dir) const {
  auto ptr = reinterpret_cast<const char *>(&dir);
  for (size_t i = 0; i < sizeof(DirEntry); ++i) {
    if (ptr[i] != '\0') return false;
  }
  return true;
}

void ImageEditor::ReadFat() {
  fs_.seekg(GetFatPosition());
  fat_.resize(GetFatSize());
  fs_.read(reinterpret_cast<char *>(fat_.data()), fat_.size());
}

void ImageEditor::ReadDirectories() {
  fs_.seekg(GetDirEntryPosition());
  dirs_.clear();
  dirs_.reserve(GetDirEntryCount());
  last_dir_pos_ = -1;
  for (uint32_t i = 0; i < GetDirEntryCount(); ++i) {
    DirEntry cur;
    fs_.read(reinterpret_cast<char *>(&cur), sizeof(DirEntry));
    if (std::memcmp(cur.name, "\0\0\0\0\0\0\0\0\0\0", 11) &&
        cur.first_cluster && cur.file_size) {
      dirs_.push_back(cur);
    }
    if (last_dir_pos_ == -1 && IsDirEntryEmpty(cur)) {
      last_dir_pos_ = fs_.tellg();
      last_dir_pos_ -= sizeof(DirEntry);
    }
  }
}

void ImageEditor::PrintFileSize(std::ostream &os, uint32_t size) {
  if (size < 1000) {
    os << size << "B";
  }
  else if (size < 1000 * 1000) {
    os << std::setprecision(3) << (size / 1024.0) << "KB";
  }
  else {
    os << std::setprecision(3) << (size / 1024.0 / 1024.0) << "MB";
  }
}

bool ImageEditor::GetFileName(const std::string &filename, char *buffer) {
  std::memset(buffer, ' ', 11);
  auto pos = filename.rfind('.');
  if (pos == std::string::npos) {
    if (filename.size() > 8) return false;
    pos = 8;
  }
  if (pos > 8 || filename.size() - pos - 1 > 3) return false;
  for (size_t i = 0; i < filename.size(); ++i) {
    if (i < pos) {
      buffer[i] = std::toupper(filename[i]);
    }
    else if (i > pos) {
      buffer[i - pos - 1 + 8] = std::toupper(filename[i]);
    }
  }
  return true;
}

void ImageEditor::ReadFileEntry(std::ostream &os, const DirEntry &dir) {
  // current cluster
  auto current = dir.first_cluster;
  // buffer to transfer file content
  std::vector<uint8_t> buffer;
  buffer.resize(GetClusterSize());
  // read each cluster
  int size = 0, act_size;
  while (current < 0xff0) {
    if (size + buffer.size() > dir.file_size) {
      act_size = dir.file_size - size;
    }
    else {
      act_size = buffer.size();
      size += buffer.size();
    }
    fs_.seekg(GetDataPosition() + (current - 2) * buffer.size());
    fs_.read(reinterpret_cast<char *>(buffer.data()), act_size);
    os.write(reinterpret_cast<char *>(buffer.data()), act_size);
    current = GetNextCluster(current);
  }
}

bool ImageEditor::LoadImageFile(const std::string &image_file) {
  // open file
  fs_.close();
  fs_.open(image_file, std::ios::in | std::ios::out | std::ios::binary);
  if (!fs_.is_open()) return !(is_error_ = true);
  fs_ >> std::noskipws;
  // check boot sector
  fs_.read(reinterpret_cast<char *>(&boot_), sizeof(BootSector));
  if (!IsValidBootSector()) return !(is_error_ = true);
  // read more information
  ReadFat();
  if (!IsValidFat()) return !(is_error_ = true);
  ReadDirectories();
  // success
  return !(is_error_ = false);
}

bool ImageEditor::CreateImage(const std::string &image_file,
                              const std::string &boot_sector_file) {
  is_error_ = false;
  // open file
  std::ifstream ifs(boot_sector_file, std::ios::binary);
  if (!ifs.is_open()) return !(is_error_ = true);
  // check boot sector
  ifs.read(reinterpret_cast<char *>(&boot_), sizeof(BootSector));
  if (!IsValidBootSector()) return !(is_error_ = true);
  // initialize FAT & directories
  fat_.clear();
  fat_.resize(GetFatSize(), 0x00);
  fat_[0] = 0xf0;
  fat_[1] = 0xff;
  fat_[2] = 0xff;
  dirs_.clear();
  dirs_.reserve(GetDirEntryCount());
  // create file
  fs_.close();
  fs_.open(image_file, std::ios::out | std::ios::binary);
  if (!fs_.is_open()) return !(is_error_ = true);
  // save boot sector
  std::vector<uint8_t> buffer;
  buffer.resize(GetFatPosition() - sizeof(BootSector));
  ifs.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
  fs_.write(reinterpret_cast<char *>(&boot_), sizeof(BootSector));
  fs_.write(reinterpret_cast<char *>(buffer.data()), buffer.size());
  // save FAT & directories
  for (uint32_t i = 0; i < GetFatCount(); ++i) {
    fs_.write(reinterpret_cast<char *>(fat_.data()), fat_.size());
  }
  for (uint32_t i = 0; i < GetDirEntryCount() * sizeof(DirEntry); ++i) {
    fs_.write("", 1);
  }
  last_dir_pos_ = GetDirEntryPosition();
  // save data area
  int data_size = GetDataSectorCount() * boot_.bpb.bytes_per_sector;
  for (int i = 0; i < data_size; ++i) fs_.write("", 1);
  // reopen image file
  fs_.close();
  fs_.open(image_file, std::ios::in | std::ios::out | std::ios::binary);
  if (!fs_.is_open()) return !(is_error_ = true);
  // success
  return true;
}

bool ImageEditor::PrintDirInfo(std::ostream &os) {
  if (!fs_.is_open() || is_error_) return false;
  for (const auto &i : dirs_) {
    // file name
    os << std::string(i.name, 11);
    // date
    os << "     " << i.last_mod_date.year + 1980 << '-';
    os << std::setw(2) << std::setfill('0');
    os << i.last_mod_date.month << '-';
    os << std::setw(2) << std::setfill('0');
    os << i.last_mod_date.day;
    // time
    os << "     ";
    os << std::setw(2) << std::setfill('0');
    os << i.last_mod_time.hour << ':';
    os << std::setw(2) << std::setfill('0');
    os << i.last_mod_time.minute << ':';
    os << std::setw(2) << std::setfill('0');
    os << i.last_mod_time.second;
    // file size
    os << "     ";
    PrintFileSize(os, i.file_size);
    os << std::endl;
  }
  return true;
}

bool ImageEditor::ReadFile(std::ostream &os, const std::string &filename) {
  if (!fs_.is_open() || is_error_) return false;
  // extract file name
  char name[11];
  if (!GetFileName(filename, name)) return false;
  // find the directory entry
  for (const auto &i : dirs_) {
    if (!std::memcmp(i.name, name, 11)) {
      ReadFileEntry(os, i);
      return true;
    }
  }
  return false;
}

bool ImageEditor::AddFile(std::istream &is, const std::string &filename) {
  if (!fs_.is_open() || is_error_) return false;
  if (dirs_.size() >= GetDirEntryCount()) return false;
  // create new directory entry
  DirEntry new_dir;
  if (!InitDirEntry(new_dir, filename)) return false;
  // buffer to store the file content temporary
  std::vector<uint8_t> buffer;
  buffer.resize(GetClusterSize());
  // store file into image
  uint16_t current = FindNextCluster(), last = 0xff7;
  new_dir.first_cluster = current;
  new_dir.file_size = 0;
  while (!is.eof()) {
    // run out of the cluster
    if (current >= 0xff0) return !(is_error_ = true);
    // write FAT
    if (last < 0xff0) SetNextCluster(last, current);
    // read file
    is.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
    new_dir.file_size += is.gcount();
    // write to image
    fs_.seekp(GetDataPosition() + (current - 2) * buffer.size());
    fs_.write(reinterpret_cast<char *>(buffer.data()), is.gcount());
    // select next cluster
    last = current;
    current = FindNextCluster(current + 1);
  }
  // write last FAT & directories
  SetNextCluster(last, 0xff8);
  fs_.seekp(last_dir_pos_);
  fs_.write(reinterpret_cast<char *>(&new_dir), sizeof(DirEntry));
  // can not create new directory entry
  if (!FindNextDirEntry(fs_.tellp())) return !(is_error_ = true);
  dirs_.push_back(new_dir);
  return true;
}
