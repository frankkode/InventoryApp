#pragma once

#include <string>

// Supplier: a plain value type, held inside Inventory's supplier vector.
struct Supplier {
    std::string id;              // unique identifier, validated by isValidSupplierId()
    std::string name;
    std::string contact;

    static bool isValidSupplierId(const std::string& id);
};