#pragma once
#include <fstream>
#include <string>
#include <filesystem>

class LogTail {
public:
    explicit LogTail(const std::string &path)
        : path_(path) {
        reopen();
    }

    bool read_next(std::string &line_out) {
        namespace fs = std::filesystem;
        if (!fs::exists(path_))
            return false;

        auto mod_time = fs::last_write_time(path_);
        auto size = fs::file_size(path_);

        // Datei wurde neu geschrieben oder gekürzt → neu öffnen
        if (size < last_size_ || mod_time != last_mod_time_) {
            reopen();
            last_size_ = size;
            last_mod_time_ = mod_time;
        }

        // immer letzte Zeile lesen
        std::string last;
        {
            std::ifstream f(path_);
            if (!f.is_open())
                return false;

            f.seekg(0, std::ios::end);
            std::streamoff end_off = static_cast<std::streamoff>(f.tellg());
            std::streamoff start_off = std::max<std::streamoff>(0, end_off - static_cast<std::streamoff>(8192));
            f.seekg(start_off, std::ios::beg);

            std::string line;
            while (std::getline(f, line)) {
                if (!line.empty())
                    last = std::move(line);
            }
        }

        if (!last.empty() && last != last_line_) {
            last_line_ = last;
            line_out = std::move(last);
            last_size_ = size;
            last_mod_time_ = mod_time;
            return true;
        }

        return false;
    }

private:
    std::string path_;
    uintmax_t last_size_ = 0;
    std::filesystem::file_time_type last_mod_time_{};
    std::string last_line_;

    void reopen() {
        last_size_ = 0;
        last_line_.clear();
    }
};






