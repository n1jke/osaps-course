#include "cli.h"
#include "codec.h"
#include "file_io.h"

#include <exception>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <system_error>

void print_stats(const file_io::Stats& stats) {
    std::ostringstream line;

    line << std::fixed << std::setprecision(6)
         << "count=" << stats.count << " file_bytes=" << stats.file_size
         << " processed_bytes=" << stats.processed_bytes
         << " elapsed_ms=" << stats.elapsed_ms << " verified=yes\n";

    file_io::output(line.str());
}

int main(int argc, char* argv[]) {
    try {
        const auto args = cli::parse(argc, argv);

        switch (args.command) {
        case cli::Command::Help:
            file_io::output(cli::help());
            break;

        case cli::Command::Create:
            print_stats(file_io::create(args.path));
            break;

        case cli::Command::Add: {
            const auto result = file_io::add(args.path, args.text);

            file_io::output("added id=" + file_io::format_uuid(result.id) + "\n");
            print_stats(result.stats);
            break;
        }

        case cli::Command::List: {
            const auto stats = file_io::list(args.path, [](const codec::Record& record) {
                file_io::output(codec::format_record(record, file_io::format_uuid(record.id)));
            });

            if (!stats.count) {
                file_io::output("Database is empty\n");
            }

            print_stats(stats);
            break;
        }

        case cli::Command::Find: {
            const auto id = file_io::parse_uuid(args.id);
            const auto result = file_io::find(args.path, id);

            if (result.found) {
                file_io::output(codec::format_record(result.record, file_io::format_uuid(result.record.id)));
            } else {
                file_io::output("Not found\n");
            }

            print_stats(result.stats);
            break;
        }

        case cli::Command::Generate: {
            std::random_device seed;
            std::mt19937 random(seed());
            std::uniform_int_distribution<std::uint32_t> lengths(1, args.max_length);
            std::uniform_int_distribution<int> letters(0, 51);
            constexpr char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

            const auto result = file_io::generate(args.path, args.count, [&](std::uint64_t) {
                codec::Record record;
                record.id = file_io::create_uuid();
                record.text.resize(lengths(random));

                for (char& c : record.text) {
                    c = alphabet[letters(random)];
                }

                return record;
            });

            file_io::output("first_uuid=" + file_io::format_uuid(result.first) +
                "\nmiddle_uuid=" + file_io::format_uuid(result.middle) +
                "\nlast_uuid=" + file_io::format_uuid(result.last) +
                "\nmaxLen=" + std::to_string(args.max_length) + "\n");

            print_stats(result.stats);
            break;
        }
        }

        return 0;
    } catch (const std::invalid_argument& error) {
        std::cerr << "[ARGUMENT ERROR] " << error.what() << "\nUse --help\n";
        return 2;
    } catch (const std::system_error& error) {
        file_io::printWinError(error.what(), static_cast<unsigned long>(error.code().value()));
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "[ERROR] " << error.what() << '\n';
        return 1;
    }
}
