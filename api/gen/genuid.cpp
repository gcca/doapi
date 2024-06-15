#include <chrono>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>

#include "genuid.hpp"

using namespace genuid;

static std::size_t base_timestamp = 0;
static std::size_t last_timestamp = 0;
static std::size_t datacenter_id = 0;
static std::size_t machine_id = 0;
static std::size_t sequence_number = 0;
static std::mutex uid_mutex;

static constexpr std::size_t datacenter_id_bits = 5;
static constexpr std::size_t machine_id_bits = 5;
static constexpr std::size_t sequence_number_bits = 12;

static constexpr std::size_t max_datacenter_id =
    -1L ^ (-1L << datacenter_id_bits);
static constexpr std::size_t max_machine_id = -1L ^ (-1L << machine_id_bits);
static constexpr std::size_t sequence_number_mask =
    -1L ^ (-1L << sequence_number_bits);

static constexpr std::size_t machine_id_shift = sequence_number_bits;
static constexpr std::size_t datacenter_id_shift =
    sequence_number_bits + machine_id_bits;
static constexpr std::size_t timestamp_shift =
    sequence_number_bits + datacenter_id_bits + machine_id_bits;

static inline void InitBaseTimestamp() {
  std::tm db = {0};
  db.tm_year = 2024 - 1900;
  db.tm_mon = 5 - 1;
  db.tm_mday = 3;

  std::time_t tb = std::mktime(&db);

  std::size_t timestamp =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::from_time_t(tb).time_since_epoch())
          .count();

  base_timestamp = timestamp;
}

void genuid::InitParameters() {
  const char *raw_datacenter_id = std::getenv("DATACENTER_ID");
  const char *raw_machine_id = std::getenv("MACHINE_ID");

  if (raw_datacenter_id == nullptr) {
    if (raw_machine_id == nullptr) {
      throw std::runtime_error{
          "Undefined environment variables DATACENTER_ID and MACHINE_ID"};
    } else {
      throw std::runtime_error{"Undefined environment variable DATACENTER_ID"};
    }
  }

  if (raw_machine_id == nullptr) {
    throw std::runtime_error{"Undefined environment variable MACHINE_ID"};
  }

  datacenter_id = std::stoull(raw_datacenter_id);
  machine_id = std::stoull(raw_machine_id);

  if (datacenter_id > max_datacenter_id) {
    std::ostringstream oss;
    oss << "datacenter Id cannot be greather than " << max_datacenter_id;
    throw std::runtime_error{oss.str()};
  }

  if (machine_id > max_machine_id) {
    std::ostringstream oss;
    oss << "machine Id cannot be greather than " << max_machine_id;
    throw std::runtime_error{oss.str()};
  }

  InitBaseTimestamp();
}

[[nodiscard]] static inline std::size_t GetCurrentTimestamp() {
  std::chrono::time_point now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::seconds>(
             now.time_since_epoch())
      .count();
}

[[nodiscard]] static inline std::size_t
GetNextTimestamp(const std::size_t base_timestamp) {
  std::size_t timestamp = GetCurrentTimestamp();
  while (timestamp <= base_timestamp) {
    timestamp = GetCurrentTimestamp();
  }
  return timestamp;
}

[[nodiscard]] std::uint64_t genuid::GenerateUID() {
  std::lock_guard<std::mutex> guard{uid_mutex};
  std::size_t timestamp = GetCurrentTimestamp();

  if (timestamp < last_timestamp) {
    // improve reset clock timestamp instead of raise exception
    throw std::runtime_error{"time moved backward"};
  }

  if (timestamp == last_timestamp) {
    sequence_number = (sequence_number + 1) & sequence_number_mask;
    if (sequence_number == 0) {
      timestamp = GetNextTimestamp(last_timestamp);
    }
  } else {
    sequence_number = 0;
  }

  last_timestamp = timestamp;

  const std::size_t uid = ((timestamp - base_timestamp) << 22) |
                          (datacenter_id << 17) | (machine_id << 12) |
                          sequence_number;
  return uid;
}
