#pragma once

#include <string>

enum class TransactionType {
    StockIn,
    StockOut,
    Adjustment
};

// Converts a TransactionType to its on-disk and display label ("StockIn",
// "StockOut", "Adjustment").
std::string toString(TransactionType type);

// Parses a label produced by toString() back into a TransactionType. Throws
// std::invalid_argument if the label is not one of the three values, so a
// corrupted persistence file is caught at load time rather than silently
// misread.
TransactionType transactionTypeFromString(const std::string& label);

// Transaction: a record of one stock movement. A plain value type, held
// inside Inventory's transaction vector.
struct Transaction {
    std::string id;
    std::string timestamp;
    std::string productSku;      // references Product::sku by value, not by pointer
    TransactionType type = TransactionType::StockIn;
    int quantityDelta = 0;       // signed: positive for StockIn, negative for
                                  // StockOut, either sign for Adjustment; derived
                                  // by the logic layer from a non-negative
                                  // user-entered quantity.
    std::string note;
};