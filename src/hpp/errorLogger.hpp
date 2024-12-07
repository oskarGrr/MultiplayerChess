#pragma once
#include <string_view>
#include <memory>
#include <mutex>
#include <fstream>

//simple header only thread safe file logger
class FileErrorLogger
{
public:

    static FileErrorLogger& get()
    {
        static FileErrorLogger errLogger;
        return errLogger;
    }

    //creates as many log entries as there are parameters in the errors param pack.
    //It does this by folding the errors pack over the << operator and calling
    //operator << for every error in errors.
    //Do not add new lines at the end of the error msg. They will be added by log().
    void log(auto const&... errors)
    {
        std::lock_guard lk {m_logMutex};
        std::ofstream errLoggStream {"chessErrorLog.txt", std::ios_base::app};
        (errLoggStream << ... << errors) << '\n';
    }

private:
    std::mutex m_logMutex;

    FileErrorLogger()=default;
    ~FileErrorLogger()=default;

    FileErrorLogger(FileErrorLogger const&)=delete;
    FileErrorLogger(FileErrorLogger&&)=delete;
    FileErrorLogger& operator=(FileErrorLogger const&)=delete;
    FileErrorLogger& operator=(FileErrorLogger&&)=delete;
};