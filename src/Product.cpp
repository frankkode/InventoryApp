#include "Product.h"

#include <algorithm>
#include <cctype>

bool Product::isValidSku(const std::string& sku) {
    if (sku.empty()) {
        return false;
    }
    return std::none_of(sku.begin(), sku.end(), [](unsigned char c) {
        return std::isspace(c);
    });
}