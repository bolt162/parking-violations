#ifndef SOA_CSV_READER_HPP
#define SOA_CSV_READER_HPP

#include <string>
#include <cstddef>

namespace parking {

struct SoADataStore;

class SoACsvReader {
public:
    size_t read(const std::string& filepath, SoADataStore& store);
};

} // namespace parking

#endif
