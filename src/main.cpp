#include <iostream>
#include <fstream>
#include <ctime>
using namespace std;

#include "imgedit.h"

int PrintHelpInfo() {
  cout << "usage: imgedit [-h | -v] image [-r file | "
          "-c boot_sector [file ...] | -a file ...]"
       << endl << endl;
  cout << "options:" << endl;
  cout << "  -h                         display this message" << endl;
  cout << "  -v                         show version info" << endl;
  cout << "  image                      file name of floppy image" << endl;
  cout << "  -r file                    read file from image to stdout"
       << endl;
  cout << "  -c boot_sector [file ...]  create floppy image" << endl;
  cout << "  -a file ...                add file to floppy image" << endl;
  return 0;
}

int PrintVersionInfo() {
  cout << "Image Editor version 0.0.2 by MaxXing" << endl;
  cout << "A FAT12 format (floppy) image editor." << endl;
  return 0;
}

int PrintIllegalArg() {
  cout << "illegal argument, run 'imgedit -h' for help" << endl;
  return 1;
}

bool CheckIsError(const ImageEditor &ie, const char *filename) {
  if (ie.is_error()) {
    cout << "illegal FAT12 image file '" << filename << "'" << endl;
    return true;
  }
  return false;
}

string GetFileName(const string &path) {
  auto pos = path.rfind('/');
  if (pos == string::npos) {
    return path;
  }
  else {
    return path.substr(pos + 1, path.size() - pos - 1);
  }
}

int main(int argc, const char *argv[]) {
  if (argc < 2) {
    return !PrintHelpInfo();
  }
  // parse command arguments
  if (argv[1][0] == '-') {
    switch (argv[1][1]) {
      case 'h':
        return PrintHelpInfo();
      case 'v':
        return PrintVersionInfo();
      default:
        return PrintIllegalArg();
    }
  }
  // parse & process
  ImageEditor ie(argv[1]);
  if (argc == 2) {
    if (CheckIsError(ie, argv[1])) return 1;
    ie.PrintDirInfo(cout);
  }
  else if (argc >= 4 && argv[2][0] == '-') {
    int start = 3;
    switch (argv[2][1]) {
      case 'r': {
        if (!ie.ReadFile(cout, argv[3])) {
          cout << "file '" << argv[3] << "' can not ";
          cout << "be found in current image" << endl;
          return 1;
        }
        break;
      }
      case 'c': {
        ie.CreateImage(argv[1], argv[3]);
        ++start;
        // fall through
      }
      case 'a': {
        for (int i = start; i < argc; ++i) {
          ifstream ifs(argv[i], ios::binary);
          if (!ie.AddFile(ifs, GetFileName(argv[i]))) {
            cout << "can not create image '";
            cout << argv[1] << "'" << endl;
            return 1;
          }
        }
        break;
      }
      default: return PrintIllegalArg();
    }
  }
  else {
    return PrintIllegalArg();
  }
  return 0;
}
