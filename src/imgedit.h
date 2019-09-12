#ifndef OSLAB_IMGEDIT_H_
#define OSLAB_IMGEDIT_H_

#include <fstream>
#include <ostream>
#include <string>
#include <vector>
#include <cstdint>

class ImageEditor {
 public:
  ImageEditor() : is_error_(false) {}
  ImageEditor(const std::string &image_file) { LoadImageFile(image_file); }
  ~ImageEditor() {}

  bool LoadImageFile(const std::string &image_file);
  bool CreateImage(const std::string &image_file,
                   const std::string &boot_sector_file);
  bool PrintDirInfo(std::ostream &os);
  bool ReadFile(std::ostream &os, const std::string &filename);
  bool AddFile(std::istream &is, const std::string &filename);

  bool is_open() const { return fs_.is_open(); }
  bool is_error() const { return is_error_; }

 private:
#pragma pack(1)
  // the first sector
  struct BiosParameterBlock {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t table_count;
    uint16_t root_entry_count;
    uint16_t sector_count_16;
    uint8_t media;
    uint16_t sectors_per_table;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sector_count;
    uint32_t sector_count_32;
  };

  struct BootSector {
    uint8_t jump_inst[3];
    uint8_t oem[8];
    BiosParameterBlock bpb;
    uint8_t drive_number;
    uint8_t reserved;
    uint8_t signature;
    uint8_t id[4];
    uint8_t volume_label[11];
    uint8_t file_system_id[8];
  };

  // directories field
  struct FatTime {
    uint16_t second : 5;
    uint16_t minute : 6;
    uint16_t hour : 5;
  };

  struct FatDate {
    uint16_t day : 5;
    uint16_t month : 4;
    uint16_t year : 7;
  };

  struct DirEntry {
    char name[11];
    uint8_t attributes;
    uint8_t reserved[2];
    FatTime creation_time;
    FatDate creation_date;
    FatDate last_access_date;
    uint16_t zero;
    FatTime last_mod_time;
    FatDate last_mod_date;
    uint16_t first_cluster;
    uint32_t file_size;
  };
#pragma pack()

  uint32_t GetFatPosition() const;
  uint32_t GetFatSize() const;
  uint32_t GetFatCount() const;
  uint32_t GetDirEntryPosition() const;
  uint32_t GetDirEntryCount() const;
  uint32_t GetDataPosition() const;
  uint32_t GetDataSectorCount() const;
  uint32_t GetClusterSize() const;
  uint16_t GetNextCluster(uint16_t i) const;
  void SetNextCluster(uint16_t i, uint16_t value);
  uint16_t FindNextCluster(uint16_t start = 2);
  bool InitDirEntry(DirEntry &dir, const std::string &filename);
  bool FindNextDirEntry(uint32_t start);
  void GetFatTime(FatTime &fat_time);
  void GetFatDate(FatDate &fat_date);
  bool IsValidBootSector() const;
  bool IsValidFat() const;
  bool IsDirEntryEmpty(const DirEntry &dir) const;

  void ReadFat();
  void ReadDirectories();
  void PrintFileSize(std::ostream &os, uint32_t size);
  bool GetFileName(const std::string &filename, char *buffer);
  void ReadFileEntry(std::ostream &os, const DirEntry &dir);

  bool is_error_;
  std::fstream fs_;
  // data of FAT12 file system
  BootSector boot_;
  std::vector<uint8_t> fat_;
  std::vector<DirEntry> dirs_;
  int last_dir_pos_;
};

#endif  // OSLAB_IMGEDIT_H_
