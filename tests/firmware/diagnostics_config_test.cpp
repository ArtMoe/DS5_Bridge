#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::string read_text(std::filesystem::path const &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Unable to open " + path.string());
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

void require_contains(
    std::string const &source,
    std::string const &expected,
    std::string const &message
) {
    if (source.find(expected) == std::string::npos) {
        throw std::runtime_error(message + ": missing " + expected);
    }
}

}  // namespace

int main() {
    try {
        const std::filesystem::path root = DS5_SOURCE_ROOT;
        const std::string cmake = read_text(root / "CMakeLists.txt");
        const std::string main = read_text(root / "src" / "main.cpp");
        const std::string presets = read_text(root / "CMakePresets.json");

        require_contains(
            cmake,
            "PICO_DEFAULT_UART_BAUD_RATE=921600",
            "Diagnostic UART firmware must match the persistent host collector"
        );
        require_contains(
            cmake,
            "PICO_STACK_SIZE=4096",
            "Diagnostic logging must retain the larger firmware stack"
        );
        require_contains(
            main,
            "#if DS5_DEBUG_LOGS_ENABLED",
            "The debug build must override TinyUSB's board-level UART default"
        );
        require_contains(
            main,
            "stdio_init_all();",
            "The debug build must reinitialize stdio at the configured UART baud"
        );
        require_contains(
            presets,
            "\"name\": \"pico2-w-debug-uart-companion-on\"",
            "The supported UART diagnostic configure/build preset must remain available"
        );
        require_contains(
            presets,
            "\"DS5_DIAGNOSTICS_PRESET\": \"custom\"",
            "The UART preset must use explicit diagnostic switches"
        );
        require_contains(
            presets,
            "\"ENABLE_COMPANION\": \"ON\"",
            "The UART preset must preserve companion support"
        );
        require_contains(
            presets,
            "\"ENABLE_DEBUG_LOGS\": \"ON\"",
            "The UART preset must compile firmware logging in"
        );
        require_contains(
            presets,
            "\"WAVESHARE_RP2350B_PLUS_W_BUILD\": \"OFF\"",
            "The UART preset must explicitly target the Pico 2 W"
        );

        std::cout << "Diagnostics configuration checks passed.\n";
        return 0;
    } catch (std::exception const &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
