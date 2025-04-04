#include <chrono>
#include <ctime>
#include <memory>
#include <mutex>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/console.h>
#else
#include <cpptrace/cpptrace.hpp>
#endif

#include <cubos/core/data/fs/file.hpp>
#include <cubos/core/data/fs/file_system.hpp>
#include <cubos/core/data/fs/standard_archive.hpp>
#include <cubos/core/data/ser/debug.hpp>
#include <cubos/core/reflection/external/string.hpp>
#include <cubos/core/reflection/traits/enum.hpp>
#include <cubos/core/tel/logging.hpp>

using cubos::core::data::File;
using cubos::core::data::FileSystem;
using cubos::core::data::StandardArchive;
using cubos::core::reflection::EnumTrait;
using cubos::core::tel::Level;
using cubos::core::tel::Logger;

namespace
{
    /// @brief Private type which stores the state of the logger.
    struct State
    {
        std::mutex mutex;
        std::vector<Logger::Entry> entries;
        std::unique_ptr<cubos::core::memory::Stream> logFileStream;
    };
} // namespace

/// @brief Logger state singleton. Guarantees it is initialized exactly once, when needed.
/// @return Logger state.
static State& state()
{
    static State state{};
    return state;
}

/// @brief Converts the given string to lower case.
/// @param string String.
/// @return Lower case string.
static std::string toLower(std::string string)
{
    for (auto& chr : string)
    {
        chr = static_cast<char>(std::tolower(static_cast<int>(chr)));
    }
    return string;
}

static constexpr const char* ColorResetCode = "\033[m";

/// @brief Returns an ASCII color code for the given log level.
/// @param level Log level.
/// @return ASCII color code.
static const char* levelColor(Level level)
{
    switch (level)
    {
    case Level::Trace:
        return "\033[37m"; // White
    case Level::Debug:
        return "\033[36m"; // Cyan
    case Level::Info:
        return "\033[32m"; // Green
    case Level::Warn:
        return "\033[33m\033[1m"; // Yellow bold
    case Level::Error:
        return "\033[31m\033[1m"; // Red bold
    case Level::Critical:
        return "\033[1m\033[41m"; // Bold on red
    default:
        return ColorResetCode;
    }
}

void cubos::core::tel::abort()
{
#ifdef __EMSCRIPTEN__
    char callstack[4096];
    emscripten_get_callstack(EM_LOG_C_STACK, callstack, sizeof(callstack));
    std::puts(callstack);
#else
    cpptrace::generate_trace(1).print();
#endif
    std::abort();
}

std::string Logger::Location::string() const
{
    // Extract the file name.
    auto name = this->file;
    if (name.find('/') != std::string::npos)
    {
        name = name.substr(name.find_last_of('/') + 1);
    }
    else if (name.find('\\') != std::string::npos)
    {
        name = name.substr(name.find_last_of('\\') + 1);
    }

    return name + ':' + std::to_string(this->line) + ' ' + this->function;
}

auto Logger::Timestamp::now() -> Timestamp
{
    const auto now = std::chrono::system_clock::now();
    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return Timestamp{.timestamp = timestamp};
}

std::string Logger::Timestamp::string() const
{
    const auto time = std::chrono::system_clock::from_time_t(0) + std::chrono::milliseconds{this->timestamp};
    const auto cTime = std::chrono::system_clock::to_time_t(time);

    std::string hoursMinsSecs{};
    hoursMinsSecs.resize(8, ' ');
    std::strftime(hoursMinsSecs.data(), hoursMinsSecs.size() + 1, "%H:%M:%S", std::localtime(&cTime));

    // Get the number of milliseconds since the time_t timestamp (that format only stores up to seconds).
    auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(time - std::chrono::system_clock::from_time_t(cTime))
            .count();
    std::string msStr = std::to_string(ms);

    // Pad milliseconds string to 3 digits
    if (msStr.size() < 3)
    {
        msStr.insert(msStr.begin(), 3 - msStr.size(), '0');
    }

    return hoursMinsSecs + '.' + msStr;
}

bool Logger::logToFile(const std::string& filePath)
{
    auto file = FileSystem::create(filePath, /*directory=*/false);
    if (file == nullptr)
    {
        return false;
    }

    state().logFileStream = file->open(File::OpenMode::Write);

    // sync old logs to file
    for (const auto& e : state().entries)
    {
        state().logFileStream->printf("[{}] [{}] {}: {}\n", e.timestamp.string(), e.location.string(),
                                      toLower(EnumTrait::toString(e.level)), e.message);
    }

    return true;
}

bool Logger::logToFile()
{
    if (!FileSystem::mount("/logs", std::make_unique<cubos::core::data::StandardArchive>("./logs", /*isDirectory=*/true,
                                                                                         /*readOnly=*/false)))
    {
        return false;
    }

    auto timestamp = Timestamp::now();
    auto filePath = "cubos_" + timestamp.string();
    auto fullPath = "/logs/" + filePath + ".log";
    return logToFile(fullPath);
}

void Logger::write(Level level, Location location, std::string message)
{
    auto timestamp = Timestamp::now();

    std::lock_guard<std::mutex> guard{state().mutex};

    if (static_cast<int>(tel::level()) > static_cast<int>(level))
    {
        // If the log level is higher than the message level, then just ignore it.
        return;
    }

    // Cache so we dont loop spans twice.
    auto spanName = SpanManager::current().path;

    // Print to file if opened
    if (state().logFileStream != nullptr)
    {
        state().logFileStream->printf("[{}] [{}] [{}] {}: {}\n", timestamp.string(), location.string(), spanName,
                                      toLower(EnumTrait::toString(level)), message);
    }

#ifndef __EMSCRIPTEN__
    // Print the message to stderr.
    memory::Stream::stdErr.printf("{}[{}] [{}] [{}] {}: {}{}\n", levelColor(level), timestamp.string(),
                                  location.string(), spanName, toLower(EnumTrait::toString(level)), message,
                                  ColorResetCode);
#else
    (void)levelColor;
    std::string logMessage = "[";
    logMessage.append(timestamp.string());
    logMessage.append("] [");
    logMessage.append(location.string());
    logMessage.append("] [");
    logMessage.append(spanName);
    logMessage.append("] ");
    logMessage.append(toLower(EnumTrait::toString(level)));
    logMessage.append(": ");
    logMessage.append(message);
    if (level == Level::Critical || level == Level::Error)
    {
        emscripten_console_error(logMessage.c_str());
    }
    else if (level == Level::Warn)
    {
        emscripten_console_warn(logMessage.c_str());
    }
    else
    {
        emscripten_console_log(logMessage.c_str());
    }
#endif

    // Store the log entry.
    state().entries.emplace_back(Entry{.level = level,
                                       .timestamp = timestamp,
                                       .location = std::move(location),
                                       .message = std::move(message),
                                       .span = SpanManager::current()});
}

bool Logger::read(std::size_t& cursor, Entry& entry)
{
    std::lock_guard<std::mutex> guard{state().mutex};

    if (cursor >= state().entries.size())
    {
        return false;
    }

    entry = state().entries[cursor];
    cursor += 1;
    return true;
}

const char* Logger::streamFormat(memory::Stream& stream, const char* format, const reflection::Type& type,
                                 const void* value)
{
    bool foundArgument = false;

    for (; format[0] != '\0'; ++format)
    {
        if (format[0] == '{' && format[1] == '}')
        {
            if (foundArgument)
            {
                // This is the second argument, just return a pointer to it.
                break;
            }

            foundArgument = true;
            data::DebugSerializer{stream}.write(type, value);
            ++format;
        }
        else
        {
            stream.put(format[0]);
        }
    }

    return format;
}