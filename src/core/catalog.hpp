#pragma once

#include <string>
#include <vector>

namespace broom {

// A command's public identity, collected at startup so /help and /stats can
// describe the command set without hardcoding it.
struct CatalogEntry {
    std::string name;
    std::string description;
};

struct CommandCatalog {
    std::vector<CatalogEntry> entries;
};

} // namespace broom
