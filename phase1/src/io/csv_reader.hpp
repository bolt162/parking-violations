#ifndef PARKING_CSV_READER_HPP
#define PARKING_CSV_READER_HPP

#include <string>
#include <vector>
#include "core/violation_record.hpp"
#include "data/text_pool.hpp"

namespace parking::io {

/// Abstract base class for reading CSV files into ViolationRecords.
///
/// Provides polymorphism: different readers can be swapped in for
/// different file formats without changing calling code.
class CsvReader {
public:
    CsvReader() = default;
    virtual ~CsvReader() = default;

    /// Read a CSV file and populate records.
    /// @param filepath  Path to the CSV file
    /// @param records   Output vector to append records into
    /// @param pool      TextPool to append string field data into
    /// @return          Number of records loaded from this file
    virtual size_t read(const std::string& filepath,
                        std::vector<core::ViolationRecord>& records,
                        data::TextPool& pool) = 0;

    /// Expected number of columns in the CSV.
    virtual int expected_columns() const = 0;
};

/// Concrete reader for the merged/normalized parking violations CSV.
///
/// Reading strategy (line-by-line, two-pass):
///   1. Pass 1: Stream through file counting lines for pre-allocation
///   2. Reserve records vector and TextPool
///   3. Pass 2: Read each line with getline(), parse fields, populate record
///   4. String fields are appended to the shared TextPool;
///      records store (offset, length) references into the pool.
class SingleFileReader : public CsvReader {
public:
    SingleFileReader() = default;
    ~SingleFileReader() override = default;

    size_t read(const std::string& filepath,
                std::vector<core::ViolationRecord>& records,
                data::TextPool& pool) override;

    int expected_columns() const override { return 44; }

private:
    /// Parse fields from a single line and populate a ViolationRecord.
    /// String fields are appended to the TextPool.
    /// Returns true if the record was successfully populated.
    static bool populate_record(const char* line, int line_len,
                                core::ViolationRecord& rec,
                                data::TextPool& pool);
};

} // namespace parking::io

#endif // PARKING_CSV_READER_HPP
