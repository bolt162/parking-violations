#ifndef PARKING_FILE_LOADER_HPP
#define PARKING_FILE_LOADER_HPP

#include <string>
#include <memory>
#include "data/data_store.hpp"
#include "io/csv_reader.hpp"

namespace parking::io {

/// Facade that hides CSV reader selection and data loading details.
///
/// Usage:
///   parking::data::DataStore store;
///   parking::io::FileLoader loader;
///   loader.load("parking_violations_merged.csv", store);
///   // store now contains all records
class FileLoader {
public:
    FileLoader() = default;
    virtual ~FileLoader() = default;

    /// Load a single CSV file into the DataStore.
    /// @param filepath  Path to the merged CSV file
    /// @param store     DataStore to populate
    /// @return          Number of records loaded
    size_t load(const std::string& filepath, data::DataStore& store);

private:
    /// Factory: create the appropriate CsvReader.
    std::unique_ptr<CsvReader> create_reader();
};

} // namespace parking::io

#endif // PARKING_FILE_LOADER_HPP
