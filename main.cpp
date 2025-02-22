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

        while ( line_req.wait_edge_events(std::chrono::milliseconds(65)) ) {
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

using Clock     = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;

// Thread sleep wasn't consistent enough, so I've resorted to busy waiting;
//  For reference, when attempting to perform 34 toggles in 22.5 ms with thread
//  sleep, I would run about 2.5 ms long. With this method, I run 7 us long.
void busyWaitUntil(const TimePoint& go_time) {
    while( Clock::now() < go_time );
}

void replayIr(std::vector<uint64_t> deltas) {
    auto settings = gpiod::line_settings()
                        .set_direction(gpiod::line::direction::OUTPUT);

    auto line_req = gpiod::chip(chip_path).prepare_request()
                        .set_consumer("ir_play")
                        .add_line_settings(ir_out_line_offset, settings)
                        .do_request();

    // The pin SHOULD already be low, but let's make sure
    line_req.set_value(ir_out_line_offset, gpiod::line::value::INACTIVE);

    // Build the list of times at which we will toggle the IR LED.
    //  To allow computation time, anticipate starting 10ms in the future;
    std::vector<TimePoint> toggle_times;
    toggle_times.push_back( Clock::now() + std::chrono::milliseconds(10) );
    for ( auto delay: deltas )
        toggle_times.push_back( toggle_times.back() + std::chrono::duration<uint64_t, std::nano>(delay) );

    TimePoint          tp;
    gpiod::line::value clv;

    busyWaitUntil( toggle_times.front() );
    for ( int i=0; i < (toggle_times.size()-1); i++ ) {
        if ( i % 2 == 0 ) { // HIGH
            tp  = toggle_times[i];
            clv = gpiod::line::value::ACTIVE;
            line_req.set_value(ir_out_line_offset, clv);
            while ( tp < toggle_times[i+1] ) {
                // Wait half of the 26.316 us period (38kHz)
                busyWaitUntil( tp + std::chrono::nanoseconds(13158) );
                tp  = Clock::now();
                clv = !clv;
                line_req.set_value( ir_out_line_offset, clv );
            }
        } else { // LOW
            line_req.set_value(ir_out_line_offset, gpiod::line::value::INACTIVE);
            busyWaitUntil(toggle_times[i+1]);
        }
    }
    line_req.set_value(ir_out_line_offset, gpiod::line::value::INACTIVE);
}

int main() {
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
