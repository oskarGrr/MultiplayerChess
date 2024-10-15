#pragma once
#include <string>
#include <filesystem>
#include <string_view>
#include <optional>
#include <expected>
#include <span>

//This class manages the .txt files that hold settings and information for the chess game.
//Assumtions are made that a SettingsManager will not be used in shared memory from multiple threads,
//and that there will only be one SettingsManager per .txt file.
class SettingsManager
{
public:

    //KVPair is like a JSON key value pair object.
    //Leave out any tokens like : or {} to signify that this is a valid KVPair.
    //Just have the key and value as an alphanumeric string. Any extra tokens will be inserted for you.
    struct KVPair{std::string key; std::string value;};

    SettingsManager(std::filesystem::path const& fileName);

    struct Error
    {
        enum struct Code
        {
            FILE_NOT_FOUND,
            KVPAIR_INCORRECT, //syntax error. missing ':' for example
            FSTREAM_ERROR, //std::ios::bad bit set or couldnt open file etc
            KEY_NOT_FOUND //The key given to getValue() could not be found
        };

        Code const code;
        std::string const msg;
    };
    
    std::expected<std::string, Error> getValue(std::string_view key) const;
    std::optional<Error> setValue(std::string_view key, std::string_view newValue) const;
    std::optional<Error> createKVPair(KVPair const& pair) const;

    //Erase the current file if there is one and generate a new one.
    std::optional<Error> generateNewFile(std::span<std::string const> comments, 
        std::span<KVPair const> kvpairs) const;
    
    void deleteFile() const {std::filesystem::remove(mFileName);}

    ~SettingsManager()=default;
    SettingsManager(SettingsManager const&)=default;
    SettingsManager(SettingsManager&&)=default;
    SettingsManager& operator=(SettingsManager const&)=default;
    SettingsManager& operator=(SettingsManager&&)=default;

private: 

    std::optional<Error> writeComment(std::string_view comment) const;
    std::expected<KVPair, Error> splitKVPairLine(std::string_view line) const;
    std::expected<KVPair, Error> findKVPair(std::string_view key) const;
    static void removeTrailingAndLeadingWhitespace(std::string& outStr);
    std::optional<Error> checkFileExists() const;
    static std::optional<Error> checkStreamOpen(auto const& stream);

    std::filesystem::path const mFileName;
    char const mCommentToken{'#'};
    char const mKVPairSeperatorToken{':'};
};