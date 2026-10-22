#pragma once

#include <string>

// Product: one inventory item. A plain value type, no owning pointers, no
// heap allocation beyond what std::string manages internally; held by
// value in Inventory's product vector, so copying one is cheap.
struct Product {
    std::string sku;             // unique identifier, validated by isValidSku()
    std::string name;
    std::string category;
    double unitPrice = 0.0;
    int quantityOnHand = 0;
    int reorderThreshold = 0;
    std::string supplierId;      // references Supplier::id by value, not by pointer
    std::string imagePath;       // optional product photo path, empty if none; only
                                  // the GUI uses it (a terminal can't show an image),
                                  // but it's still product data, so it lives here.

    // A SKU must be non-empty and contain no whitespace, so it can be typed
    // at a CLI prompt and round-tripped through the persistence file's
    // delimiter format without escaping surprises.
    static bool isValidSku(const std::string& sku);
};