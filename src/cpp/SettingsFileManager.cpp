#include "SettingsFileManager.hpp"
#include <fstream>
#include <algorithm>//std::for_each()
#include <exception>
#include <cassert>

SettingsManager::SettingsManager(std::filesystem::path const& fileName)
    : mFileName{fileName}
{
}

auto SettingsManager::generateNewFile(std::span<std::string const> comments,
    std::span<KVPair const> kvPairs) const -> std::optional<Error>
{
    for(auto const& comment : comments)
    {
        if(auto maybeError{writeComment(comment)})
            return maybeError;
    }

    std::ofstream ofs {mFileName, std::ios::app};
    ofs << std::endl;
    ofs.close();

    for(auto const& kvPair : kvPairs)
    {
        if(auto maybeError{createKVPair(kvPair)})
            return maybeError;
    }

    return std::nullopt;
}

auto SettingsManager::findKVPair(std::string_view key) const
    -> std::expected<KVPair, Error>
{
    if(auto maybeError{checkFileExists()})
        return std::unexpected(*maybeError);

    std::ifstream ifs {mFileName};

    if(auto maybeError{checkStreamOpen(ifs)})
        return std::unexpected(*maybeError);

    for(std::string line; std::getline(ifs, line);)
    {
        if(line.empty() || line[0] == mCommentToken)
            continue;

        //If the line was only whitespace (tabs and spaces).
        if(line.find_first_not_of("\t ") == std::string::npos)
            continue;
        
        auto const maybePair = splitKVPairLine(line);
        if(maybePair)
        {
            if(maybePair->key == key)
                return *maybePair;
        }
        else
        {
            return std::unexpected(maybePair.error());
        }
    }

    if(ifs.bad())
    {
        char errMsg[1024]{};
        strerror_s(errMsg, sizeof(errMsg), errno);
        return std::unexpected(Error
        {
            .code = Error::Code::FSTREAM_ERROR, 
            .msg  = errMsg
        });
    }

    return std::unexpected(Error
    {
        .code = Error::Code::KEY_NOT_FOUND,
        .msg  = std::string("Could not find the key: ").append(key)
    });
}

void SettingsManager::removeTrailingAndLeadingWhitespace(std::string& outStr)
{
    size_t first {0};
    size_t last {outStr.size()};

    //find first non-whitespace char
    while(first < outStr.size() && (outStr[first] == ' ' || outStr[first] == '\t'))
        ++first;

    //find last non-whitespace char
    while(last > first && (outStr[last - 1] == ' ' || outStr[last - 1] == '\t'))
        --last;
    
    outStr = outStr.substr(first, last - first);
}

auto SettingsManager::splitKVPairLine(std::string_view KVPairLine) const
    -> std::expected<KVPair, Error>
{
    assert( ! KVPairLine.empty() );

    std::size_t const colonIdx {KVPairLine.find(mKVPairSeperatorToken)};
    
    if(colonIdx == std::string::npos)
    {
        std::string errMsg {"missing a '"};
        errMsg.push_back(mKVPairSeperatorToken);
        errMsg.append("' in: ").append(KVPairLine);
        return std::unexpected(Error
        {
            .code = Error::Code::KVPAIR_INCORRECT,
            .msg  = std::move(errMsg)
        });
    }

    if(colonIdx == 0)
    {
        std::string errMsg{"nothing to the left of '"};
        errMsg.push_back(mKVPairSeperatorToken);
        errMsg.append("' in: ").append(KVPairLine);
        return std::unexpected(Error
        {
            .code = Error::Code::KVPAIR_INCORRECT,
            .msg  = std::move(errMsg)
        });
    }

    std::string_view leftSide {KVPairLine.substr(0, colonIdx)}, rightSide;
    try {rightSide = KVPairLine.substr(colonIdx + 1);}
    catch(std::out_of_range const& e)
    {
        (void)e;
        std::string errMsg {"nothing to the right of '"};
        errMsg.push_back(mKVPairSeperatorToken);
        errMsg.append("' in: ").append(KVPairLine);
        return std::unexpected(Error
        {
            .code = Error::Code::KVPAIR_INCORRECT,
            .msg  = std::move(errMsg)
        });
    }

    std::string key {leftSide};
    std::string value {rightSide};
    removeTrailingAndLeadingWhitespace(key);
    removeTrailingAndLeadingWhitespace(value);

    return KVPair{key, value};
}

auto SettingsManager::checkFileExists() const
    -> std::optional<Error>
{
    if( ! std::filesystem::exists(mFileName) )
    {
        return Error
        {
            .code = Error::Code::FILE_NOT_FOUND, 
            .msg = std::string("could not find file: ").append(mFileName.string())
        };
    }

    return std::nullopt;
}

auto SettingsManager::checkStreamOpen(auto const& stream)
    -> std::optional<Error>
{
    if( ! stream.is_open() )
    {
        char errMsg[1024]{};
        strerror_s(errMsg, sizeof(errMsg), errno);
        return Error
        {
            .code = Error::Code::FSTREAM_ERROR,
            .msg = errMsg
        };
    }

    return std::nullopt;
}

auto SettingsManager::createKVPair(KVPair const& kvpair) const
    -> std::optional<Error>
{
    if(auto maybeError{checkFileExists()})
        return maybeError;

    std::ofstream ofs{mFileName, std::ios::app};

    if(auto maybeError{checkStreamOpen(ofs)})
        return maybeError;

    ofs << kvpair.key << ' ' << mKVPairSeperatorToken << ' ' << kvpair.value << std::endl;

    return std::nullopt;
}

auto SettingsManager::writeComment(std::string_view comment) const
    -> std::optional<Error>
{
    std::ofstream ofs {mFileName, std::ios::app};

    if(auto maybeError{checkStreamOpen(ofs)})
        return maybeError;

    ofs << mCommentToken << comment << std::endl;

    return std::nullopt;
}

auto SettingsManager::getValue(std::string_view key) const
    -> std::expected<std::string, Error>
{
    auto maybePair {findKVPair(key)};
    if(maybePair)
        return maybePair->value;

    return std::unexpected(maybePair.error());
}

auto SettingsManager::setValue(std::string_view key, std::string_view newValue) const
    -> std::optional<Error>
{
    //This will just copy the whole file over, change the kvpair that needs to be updated,
    //and then write the whole thing to a new file. This is not the most efficient way to do this,
    //but it is ok since the .txt files used will be very small here.

    if(auto maybeError{checkFileExists()})
        return maybeError;

    std::ifstream ifs {mFileName};

    if(auto maybeError{checkStreamOpen(ifs)})
        return maybeError;

    std::vector<std::string> lines{};
    lines.reserve(20);
    for(std::string line; std::getline(ifs, line);)
    {
        //If this line represents a key value pair.
        if( ! line.empty() && line[0] != mCommentToken )
        {
            auto const maybePair {splitKVPairLine(line)};
            if( ! maybePair.has_value() )
                return maybePair.error();

            if(maybePair->key == key)
            {
                std::string newLine{key};

                newLine.push_back(' ');
                newLine.push_back(mKVPairSeperatorToken);
                newLine.push_back(' ');

                newLine.append(newValue);

                lines.push_back(std::move(newLine));
            }
            else lines.push_back(std::move(line));
        }
        else lines.push_back(std::move(line));
    }

    ifs.close();

    std::ofstream ofs {mFileName};

    if(auto maybeError{checkStreamOpen(ofs)})
        return maybeError;

    std::for_each(lines.begin(), lines.end(),
        [&](auto const& line){ofs << line << std::endl;});

    return std::nullopt;//return no error
}