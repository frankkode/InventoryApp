#include "Transaction.h"

#include <stdexcept>

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