#include "joyson_glove/encoder_reader.hpp"
#include "joyson_glove/udp_client.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <csignal>

using namespace joyson_glove;

static volatile std::sig_atomic_t g_running = 1;

static void signal_handler(int /*signum*/) {
    g_running = 0;
}

static void print_adc_values(const EncoderData& data, const std::string& label) {
    std::cout << "[" << label << "] ADC: ";
    for (size_t i = 0; i < NUM_ENCODER_CHANNELS; ++i) {
        std::cout << std::setw(5) << data.adc_values[i];
        if (i < NUM_ENCODER_CHANNELS - 1) std::cout << " ";
    }
    std::cout << std::endl;
}

static void print_voltages(const std::array<float, NUM_ENCODER_CHANNELS>& voltages,
                           const std::string& label) {
    std::cout << "[" << label << "] V:  ";
    for (size_t i = 0; i < NUM_ENCODER_CHANNELS; ++i) {
        std::cout << std::fixed << std::setprecision(3) << std::setw(6) << voltages[i];
        if (i < NUM_ENCODER_CHANNELS - 1) std::cout << " ";
    }
    std::cout << std::endl;
}

static void print_angles(const std::array<float, NUM_ENCODER_CHANNELS>& angles,
                         const std::string& label) {
    std::cout << "[" << label << "] Ang: ";
    for (size_t i = 0; i < NUM_ENCODER_CHANNELS; ++i) {
        std::cout << std::fixed << std::setprecision(1) << std::setw(6) << angles[i] << "°";
        if (i < NUM_ENCODER_CHANNELS - 1) std::cout << " ";
    }
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);

    std::cout << "=== EncoderReader Standalone Test ===" << std::endl;

    // ── Step 1: Configure and connect UDP client ──
    UdpConfig udp_config;
    udp_config.server_ip       = (argc > 1) ? argv[1] : "192.168.10.123";
    udp_config.server_port     = (argc > 2) ? static_cast<uint16_t>(std::stoi(argv[2])) : 8080;
    udp_config.local_port      = udp_config.server_port;
    udp_config.send_timeout    = std::chrono::milliseconds(250);
    udp_config.receive_timeout = std::chrono::milliseconds(250);

    std::cout << "\n[1] Connecting to " << udp_config.server_ip
              << ":" << udp_config.server_port << " ..." << std::endl;

    auto udp_client = std::make_shared<UdpClient>(udp_config);
    if (!udp_client->connect()) {
        std::cerr << "Failed to connect UDP client" << std::endl;
        return 1;
    }
    std::cout << "    UDP client connected" << std::endl;

    // ── Step 2: Initialize EncoderReader (without auto-start thread) ──
    EncoderReaderConfig enc_config;
    enc_config.auto_start_thread = false;
    enc_config.update_interval   = std::chrono::milliseconds(200);  // 5Hz

    EncoderReader encoder(udp_client, enc_config);

    std::cout << "\n[2] Initializing EncoderReader..." << std::endl;
    if (!encoder.initialize()) {
        std::cerr << "Failed to initialize EncoderReader (hardware not responding?)" << std::endl;
        udp_client->disconnect();
        return 1;
    }

    // ── Step 3: Direct blocking read (thread not running) ──
    std::cout << "\n[3] Direct blocking read (5 samples):" << std::endl;
    for (int i = 0; i < 5; ++i) {
        auto data = encoder.read_encoders();
        if (data) {
            std::string label = "raw " + std::to_string(i + 1);
            print_adc_values(*data, label);

            auto voltages = encoder.adc_to_voltages(data->adc_values);
            print_voltages(voltages, label);

            auto angles = encoder.voltages_to_angles(voltages);
            print_angles(angles, label);
            std::cout << std::endl;
        } else {
            std::cerr << "    Read failed at sample " << (i + 1) << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // ── Step 4: Calibrate zero point ──
    std::cout << "[4] Calibrating zero point (current position = zero)..." << std::endl;
    if (!encoder.calibrate_zero_point()) {
        std::cerr << "    Calibration failed" << std::endl;
    }

    // Verify calibration applied: cached angles should now be near zero
    auto cached_angles = encoder.get_cached_angles();
    print_angles(cached_angles, "post-cal");

    // ── Step 5: Start background thread and read cached data ──
    std::cout << "\n[5] Starting background update thread (10Hz)..." << std::endl;
    encoder.start_update_thread();
    std::cout << "    Thread running: " << std::boolalpha << encoder.is_thread_running() << std::endl;

    std::cout << "\n    Reading cached angles for 5 seconds (Ctrl+C to stop early)...\n" << std::endl;
    const auto start = std::chrono::steady_clock::now();
    const auto duration = std::chrono::seconds(5);

    while (g_running) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= duration) break;

        auto angles = encoder.get_cached_angles();
        auto age    = encoder.get_data_age();

        // Print first 6 channels in compact form for terminal readability
        std::cout << "\r    ";
        for (size_t i = 0; i < 6 && i < NUM_ENCODER_CHANNELS; ++i) {
            std::cout << "ch" << i << "="
                      << std::fixed << std::setprecision(1) << std::setw(6) << angles[i] << "° ";
        }
        std::cout << "age=" << std::setw(4) << age.count() << "ms" << std::flush;

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    std::cout << std::endl;

    // ── Step 6: Stop and report ──
    std::cout << "\n[6] Stopping update thread..." << std::endl;
    encoder.stop_update_thread();
    std::cout << "    Thread running: " << std::boolalpha << encoder.is_thread_running() << std::endl;

    // Print final calibration state
    auto cal = encoder.get_calibration();
    std::cout << "\n[7] Final calibration state:" << std::endl;
    std::cout << "    Calibrated: " << std::boolalpha << cal.is_calibrated << std::endl;
    if (cal.is_calibrated) {
        std::cout << "    Zero voltages: ";
        for (size_t i = 0; i < NUM_ENCODER_CHANNELS; ++i) {
            std::cout << std::fixed << std::setprecision(3) << cal.zero_voltages[i];
            if (i < NUM_ENCODER_CHANNELS - 1) std::cout << ", ";
        }
        std::cout << std::endl;
    }

    // Network stats
    auto stats = udp_client->get_statistics();
    std::cout << "\n[8] Network statistics:" << std::endl;
    std::cout << "    Sent: " << stats.packets_sent
              << "  Received: " << stats.packets_received
              << "  Errors: " << (stats.send_errors + stats.receive_errors)
              << "  Timeouts: " << stats.timeouts << std::endl;

    udp_client->disconnect();
    std::cout << "\n=== Test completed ===" << std::endl;
    return 0;
}
