#pragma once
#include <string>
#include <filesystem>
#include <string_view>
#include <optional>
#include <expected>

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
        std::string const msg;//set with strerror() for more information
    };

    std::expected<std::string, Error> getValue(std::string_view key);
    std::optional<Error> setValue(std::string_view key, std::string_view newValue);
    std::optional<Error> createKVPair(KVPair const& pair);
    std::optional<Error> writeComment(std::string_view comment);
    void deleteFile() const {std::filesystem::remove(mFileName);}

    ~SettingsManager()=default;
    SettingsManager(SettingsManager const&)=default;
    SettingsManager(SettingsManager&&)=default;
    SettingsManager& operator=(SettingsManager const&)=default;
    SettingsManager& operator=(SettingsManager&&)=default;

private: 

    std::expected<KVPair, Error> splitKVPairLine(std::string_view line);
    std::expected<KVPair, Error> findKVPair(std::string_view key);
    std::optional<Error> checkFileExists();

    template <typename S> 
    requires std::same_as<S, std::fstream>  || 
             std::same_as<S, std::ifstream> || 
             std::same_as<S, std::ofstream>
    static std::optional<Error> checkStreamOpen(S const& stream);

    std::filesystem::path const mFileName;
    char const mCommentToken{'#'};
    char const mKVPairSeperatorToken{':'};
};