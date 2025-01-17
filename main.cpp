#include <chrono>
#include <filesystem>
#include <gpiod.hpp>
#include <iostream>
#include <thread>

const std::filesystem::path chip_path("/dev/gpiochip0");
const gpiod::line::offset   ir_in_line_offset  = 4;
const gpiod::line::offset   ir_out_line_offset = 5;

gpiod::line::value operator!(gpiod::line::value val) {
    if ( val == gpiod::line::value::INACTIVE )
        return gpiod::line::value::ACTIVE;
    return gpiod::line::value::INACTIVE;
}

std::vector<uint64_t> recordIrEdges() {
    std::vector<uint64_t> deltas;
    std::vector<uint64_t> timestamps;

    auto settings = gpiod::line_settings()
                        .set_direction(gpiod::line::direction::INPUT)
                        .set_edge_detection(gpiod::line::edge::BOTH)
                        .set_bias(gpiod::line::bias::PULL_UP);

    auto line_req = gpiod::chip(chip_path).prepare_request()
                        .set_consumer("ir_listen")
                        .add_line_settings(ir_in_line_offset, settings)
                        .do_request();

    if ( line_req.wait_edge_events(std::chrono::seconds(5)) ) {
        gpiod::edge_event_buffer buffer(100);

        while ( line_req.wait_edge_events(std::chrono::milliseconds(5)) ) {
            line_req.read_edge_events(buffer);

            for( auto event: buffer )
                timestamps.push_back(event.timestamp_ns().ns());
        }

        if ( timestamps.size() > 1 )
            for (int index=1; index < timestamps.size(); index++)
                deltas.push_back(timestamps[index] - timestamps[index-1]);
    }

    return deltas;
}

void replayIr(std::vector<uint64_t> deltas) {
    auto settings = gpiod::line_settings()
                        .set_direction(gpiod::line::direction::OUTPUT);

    auto line_req = gpiod::chip(chip_path).prepare_request()
                        .set_consumer("ir_play")
                        .add_line_settings(ir_out_line_offset, settings)
                        .do_request();

    line_req.set_value(ir_out_line_offset, gpiod::line::value::ACTIVE);
    for( auto delay: deltas ) {
        std::this_thread::sleep_for( std::chrono::nanoseconds(delay) );
        line_req.set_value( ir_out_line_offset, !line_req.get_value(ir_out_line_offset) );
    }
}

int main() {

    // TODO: loop and call record/replay based on switch event
    auto     deltas = recordIrEdges();
    uint64_t total  = 0;

    std::cout << "----------------" << std::endl;
    std::cout << "Recording:" << std::endl;
    std::cout << "\tDeltas:" << std::endl;
    for( auto delta: deltas ) {
        std::cout << "\t\tdt: " << delta << std::endl;
        total += delta;
    }
    std::cout << "\tSize: " << deltas.size() << std::endl;
    std::cout << "\tTotal: " << total << std::endl;
    std::cout << "----------------" << std::endl;

    std::cout << "Replay in 5s..." << std::endl;
    std::this_thread::sleep_for( std::chrono::seconds(5) );

    replayIr(deltas);

    return 0;
}
