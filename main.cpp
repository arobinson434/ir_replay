#include <chrono>
#include <filesystem>
#include <gpiod.hpp>
#include <iostream>

const std::filesystem::path chip_path("/dev/gpiochip0");
const gpiod::line::offset   ir_in_line_offset  = 4;
const gpiod::line::offset   ir_out_line_offset = 5;

inline uint64_t ns_now() {
    return std::chrono::nanoseconds(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::vector<uint64_t> recordIrEdges() {
    uint64_t now      = ns_now();
    uint64_t begining = now;
    uint64_t end      = now + 5e9;

    auto settings = gpiod::line_settings()
					    .set_direction(gpiod::line::direction::INPUT)
					    .set_edge_detection(gpiod::line::edge::BOTH)
					    .set_bias(gpiod::line::bias::PULL_DOWN)
					    .set_debounce_period(std::chrono::microseconds(15)); 

	auto line_req = gpiod::chip(chip_path).prepare_request()
			            .set_consumer("ir_listen")
			            .add_line_settings(ir_in_line_offset, settings)
			            .do_request();

    std::vector<uint64_t> events;
    uint64_t s, e;
    while ( now < end ) {
        s = ns_now();
        if ( line_req.wait_edge_events(std::chrono::milliseconds(500)))
            events.push_back(ns_now());
        e = ns_now();

        now = ns_now();
    }

    std::cout << "start: " << s << std::endl;
    std::cout << "end:   " << e << std::endl;
 

    return std::vector<uint64_t>();
}

int main() {

    // TODO: loop and call record/replay based on switch event
    recordIrEdges();

    return 0;
}
