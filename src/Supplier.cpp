#include "Supplier.h"

#include <algorithm>
#include <cctype>

bool Supplier::isValidSupplierId(const std::string& id) {
    if (id.empty()) {
        return false;
    }
    return std::none_of(id.begin(), id.end(), [](unsigned char c) {
        return std::isspace(c);
    });
}