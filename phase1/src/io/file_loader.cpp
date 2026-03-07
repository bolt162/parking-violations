#include "io/file_loader.hpp"
#include <iostream>
#include <chrono>

namespace parking::io {

std::unique_ptr<CsvReader> FileLoader::create_reader() {
    return std::make_unique<SingleFileReader>();
}

size_t FileLoader::load(const std::string& filepath, data::DataStore& store) {
    std::cout << "Loading: " << filepath << std::endl;

    auto reader = create_reader();

    std::vector<core::ViolationRecord> records;
    size_t count = reader->read(filepath, records, store.text_pool());

    if (count > 0) {
        store.add_records(std::move(records));
    }

    double rec_gb = store.size() * sizeof(core::ViolationRecord)
                    / (1024.0 * 1024.0 * 1024.0);
    std::cout << "DataStore now holds " << store.size() << " records ("
              << rec_gb << " GB structs + "
              << store.text_pool().size_gb() << " GB text pool)" << std::endl;

    return count;
}

} // namespace parking::io
