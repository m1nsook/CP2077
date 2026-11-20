#include "diagnostics.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#define CHECK(e) do { if (!(e)) { std::cerr << "Check failed: " #e << " (line " << __LINE__ << ")\n"; std::exit(EXIT_FAILURE); } } while (false)

int main()
{
    const auto directory = std::filesystem::temp_directory_path() / "m1core-diagnostics-tests";
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);

    auto& logs = m1core::Diagnostics::Get();
    CHECK(!logs.Write(m1core::LogLevel::Info, "core", "Not initialized"));
    logs.Initialize(directory);
    logs.SetDiskEnabled(false);
    CHECK(logs.Write(m1core::LogLevel::Info, "boot", "Title screen log is memory-only"));
    CHECK(!std::filesystem::exists(directory / "m1core.log"));
    CHECK(logs.Snapshot().size() == 1);
    logs.ClearView();
    logs.SetDiskEnabled(true);
    CHECK(!logs.Write(m1core::LogLevel::Info, "", "missing source"));
    CHECK(!logs.Write(m1core::LogLevel::Info, "core", ""));
    CHECK(!logs.Write(static_cast<m1core::LogLevel>(99), "core", "invalid level"));
    CHECK(logs.Write(m1core::LogLevel::Info, "sample", "First\nline"));
    CHECK(logs.Write(m1core::LogLevel::Error, "sample", "failure"));
    auto entries = logs.Snapshot();
    CHECK(entries.size() == 2);
    CHECK(entries[0].message == "First line");
    CHECK(entries[1].level == m1core::LogLevel::Error);

    for (int i = 0; i < 520; ++i)
        CHECK(logs.Write(m1core::LogLevel::Debug, "test", "bounded event"));
    entries = logs.Snapshot();
    CHECK(entries.size() == 512);
    CHECK(entries.front().message == "bounded event");
    logs.ClearView();
    CHECK(logs.Snapshot().empty());

    std::ifstream input(directory / "m1core.log", std::ios::binary);
    CHECK(static_cast<bool>(input));
    const std::string content((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
    CHECK(content.find("[ERROR] [sample] failure") != std::string::npos);
    CHECK(content.find("First line") != std::string::npos);
    std::filesystem::remove_all(directory, ignored);
}
