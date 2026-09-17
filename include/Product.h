#pragma once

#include <string>

// Product: one inventory item. A plain value type, no owning pointers, no
// heap allocation beyond what std::string manages internally. Instances
// live by value inside Inventory's product vector (added in a later
// commit), so copying a Product is always safe and cheap enough for a
// catalogue of a few hundred SKUs.
struct Product {
    std::string sku;             // unique identifier, validated by isValidSku()
    std::string name;
    std::string category;
    double unitPrice = 0.0;
    int quantityOnHand = 0;
    int reorderThreshold = 0;
    std::string supplierId;      // references Supplier::id by value, not by pointer
    std::string imagePath;       // optional path to a product photo, empty if none.
                                  // The CLI never touches this field. The GUI, added
                                  // in a later commit, lets the user attach a photo
                                  // and stores its path here: a terminal cannot show
                                  // an image, but the photo is still part of the
                                  // product's data, so it belongs in the data layer.

    // A SKU must be non-empty and contain no whitespace, so it can be typed
    // at a CLI prompt and round-tripped through the persistence file's
    // delimiter format without escaping surprises.
    static bool isValidSku(const std::string& sku);
};