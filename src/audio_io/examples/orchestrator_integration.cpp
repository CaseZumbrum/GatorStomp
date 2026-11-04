/** @file orchestrator_integration.cpp
 * @brief Hardware-free integration with the real IO_Controller and PCM API.
 * @details This finite demonstration generates a raw fixture, launches separate
 * capture/playback workers, forwards every byte unchanged, and compares output.
 * A simulated peripheral connects during startup. Blocking accept happens before
 * audio starts; blocking recv_msg runs on a dedicated control thread, never in
 * the PCM loop. This is a test/example, not an alternative product orchestrator.
 */
#include "audio_io_process.h"
#include "io_system/io_controller.h"
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <unistd.h>

/** @brief Controlled peer for the current constructor's blocking accept(). */
static bool simulate_peripheral(const std::string &path)
{
    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return false;
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        if (std::chrono::steady_clock::now() >= deadline) {
            close(fd);
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    gs_socket_msg message{GS_MSG_VOLUME, 0, 42, 123456};
    bool ok = send(fd, &message, sizeof(message), MSG_NOSIGNAL) == sizeof(message);
    close(fd);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc != 2 && argc != 3) {
        std::cerr << "Usage: " << argv[0] << " /path/to/audio_io_worker [block_frames]\n";
        return EXIT_FAILURE;
    }
    char temporary[] = "/tmp/gatorstomp-process-XXXXXX";
    if (!mkdtemp(temporary))
        return EXIT_FAILURE;
    std::string directory = temporary;
    std::string input = directory + "/input.raw", output = directory + "/output.raw";
    std::string socket_path = directory + "/control.sock";
    AudioIOProcess *audio = nullptr;
    int result = EXIT_FAILURE;
    try {
        std::vector<uint8_t> fixture(32771 * AUDIO_IO_FRAME_BYTES);
        for (size_t i = 0; i < fixture.size(); ++i)
            fixture[i] = static_cast<uint8_t>(i * 37 + i / 3);
        {
            std::ofstream file(input, std::ios::binary);
            file.write(reinterpret_cast<const char *>(fixture.data()), fixture.size());
            file.close();
            if (!file)
                throw std::runtime_error("Cannot write fixture");
        }
        // The real constructor blocks until this known test peer connects.
        // Product startup needs an explicit policy for absent peripherals.
        auto peer = std::async(std::launch::async, simulate_peripheral, socket_path);
        IO_Controller controller(socket_path);
        if (!peer.get())
            throw std::runtime_error("Control peer failed");
        auto control = std::async(std::launch::async, [&controller] {
            gs_socket_msg message{};
            return controller.recv_msg(&message, 0) &&
                   message.type == GS_MSG_VOLUME && message.value == 42 &&
                   message.time_ms == 123456;
        });

        AudioIOProcessConfig config = AUDIO_IO_PROCESS_CONFIG_INIT;
        config.worker_path = argv[1];
        if (argc == 3) {
            char *end;
            errno = 0;
            unsigned long frames = std::strtoul(argv[2], &end, 10);
            if (errno || *end || !frames || frames > AUDIO_IO_MAX_BLOCK_FRAMES)
                throw std::runtime_error("Invalid block size");
            config.frames_per_buffer = frames;
        }
        config.input_file = input.c_str();
        config.output_file = output.c_str();
        if (audio_io_process_start(&config, &audio) < 0)
            throw std::runtime_error(std::string("Audio startup: ") + std::strerror(errno));
        std::vector<uint8_t> block(config.frames_per_buffer * AUDIO_IO_FRAME_BYTES);
        size_t total = 0;
        for (;;) {
            ssize_t frames = audio_io_process_read_frames(audio, block.data(), config.frames_per_buffer);
            if (frames < 0)
                throw std::runtime_error("Capture transfer failed");
            if (frames == 0)
                break;
            // Insert the team's DSP callback here. Pass-through preserves bytes.
            if (audio_io_process_write_frames(audio, block.data(), static_cast<size_t>(frames)) < 0)
                throw std::runtime_error("Playback transfer failed");
            total += static_cast<size_t>(frames);
        }
        AudioIOProcessResult status{};
        if (audio_io_process_finish(&audio, &status) < 0)
            throw std::runtime_error("Worker completion failed");
        if (!control.get())
            throw std::runtime_error("Real control API check failed");
        std::ifstream recorded(output, std::ios::binary);
        std::vector<uint8_t> actual((std::istreambuf_iterator<char>(recorded)), {});
        if (recorded.bad() || actual != fixture || total != 32771)
            throw std::runtime_error("PCM byte comparison failed");
        std::cout << "Separate capture/playback processes preserved " << total
                  << " frames (" << actual.size() << " bytes); real IO_Controller passed.\n";
        result = EXIT_SUCCESS;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
    }
    audio_io_process_abort(&audio);
    unlink(input.c_str());
    unlink(output.c_str());
    unlink(socket_path.c_str());
    rmdir(directory.c_str());
    return result;
}
