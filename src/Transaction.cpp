#include "Transaction.h"

#include <stdexcept>

// Returns the label used for a type on screen and in the data file.
std::string toString(TransactionType type) {
    switch (type) {
        case TransactionType::StockIn:
            return "StockIn";
        case TransactionType::StockOut:
            return "StockOut";
        case TransactionType::Adjustment:
            return "Adjustment";
    }
    throw std::invalid_argument("unknown TransactionType");
}

// Turns a stored label back into a type, throwing if the label is unknown.
TransactionType transactionTypeFromString(const std::string& label) {
    if (label == "StockIn") {
        return TransactionType::StockIn;
    }
    if (label == "StockOut") {
        return TransactionType::StockOut;
    }
    if (label == "Adjustment") {
        return TransactionType::Adjustment;
    }
    throw std::invalid_argument("unknown TransactionType label: " + label);
}