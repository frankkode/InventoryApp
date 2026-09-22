// Optional GUI: a Dear ImGui + GLFW + OpenGL3 desktop window over the same
// Inventory/Persistence core the CLI uses. The task only requires a CLI;
// this is the explicitly allowed optional extension, built so that either
// interface can be pointed at the same three data files interchangeably.

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "misc/cpp/imgui_stdlib.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "tinyfiledialogs.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "Inventory.h"

namespace {

struct Texture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
};

std::unordered_map<std::string, Texture> textureCache;

// Loads an image file into a GL texture the first time it's seen and
// reuses it after that. Returns nullptr if the path is empty, missing, or
// isn't a format stb_image understands, so a bad photo path degrades to
// "no photo" in the table rather than crashing the GUI.
const Texture* getTexture(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }
    auto it = textureCache.find(path);
    if (it != textureCache.end()) {
        return &it->second;
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (data == nullptr) {
        return nullptr;
    }

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);

    Texture texture;
    texture.id = id;
    texture.width = width;
    texture.height = height;
    auto result = textureCache.emplace(path, texture);
    return &result.first->second;
}

// Opens the OS's native "choose a file" dialog filtered to common image
// types. Returns an empty string if the user cancels.
std::string pickImageFile() {
    const char* filters[] = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.gif"};
    const char* picked =
        tinyfd_openFileDialog("Choose a product photo", "", 5, filters, "Image files", 0);
    return picked != nullptr ? std::string(picked) : std::string();
}

std::string formatPrice(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", value);
    return std::string(buffer);
}

// Auto-generates a product SKU so the person using the GUI never has to
// invent an identifier by hand. Millisecond-resolution epoch time is
// practically guaranteed unique for a human clicking "Add product", which
// is the only place this is called from.
std::string generateSku() {
    auto now = std::chrono::system_clock::now();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return "PRD-" + std::to_string(millis);
}

// Auto-generates the next Supplier ID by scanning existing suppliers for the
// highest "SUP-####" number and incrementing it, so IDs stay sequential and
// never collide even after a supplier is removed.
std::string generateSupplierId(const Inventory& inv) {
    int maxSeen = 0;
    for (const auto& s : inv.suppliers()) {
        if (s.id.rfind("SUP-", 0) == 0) {
            try {
                int n = std::stoi(s.id.substr(4));
                if (n > maxSeen) maxSeen = n;
            } catch (...) {
                // Non-numeric or malformed suffix: ignore, keep scanning.
            }
        }
    }
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "SUP-%04d", maxSeen + 1);
    return std::string(buffer);
}

void applyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 8);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.19f, 0.29f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.09f, 0.19f, 0.29f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.09f, 0.19f, 0.29f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.13f, 0.27f, 0.40f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.09f, 0.19f, 0.29f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.95f, 0.73f, 0.24f, 0.80f);
    colors[ImGuiCol_TabActive] = ImVec4(0.13f, 0.27f, 0.40f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.13f, 0.27f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.95f, 0.73f, 0.24f, 0.80f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.95f, 0.73f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.17f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.13f, 0.14f, 0.18f, 1.00f);
}

struct StatusMessage {
    std::string text;
    bool isError = false;
};

struct AddProductForm {
    std::string sku = generateSku();
    std::string name;
    std::string category;
    std::string unitPrice = "0.00";
    std::string quantityOnHand = "0";
    std::string reorderThreshold = "0";
    std::string supplierId;
    std::string imagePath;
};

struct EditProductForm {
    std::string originalSku;
    std::string name;
    std::string category;
    std::string unitPrice;
    std::string quantityOnHand;
    std::string reorderThreshold;
    std::string supplierId;
    std::string imagePath;
};

struct AddSupplierForm {
    std::string id;
    std::string name;
    std::string contact;
};

struct MovementForm {
    std::string sku;
    int typeIndex = 0;  // 0 = StockIn, 1 = StockOut, 2 = Adjustment
    std::string quantity = "0";
    std::string note;
};

void beginEditProduct(EditProductForm& form, const Product& p) {
    form.originalSku = p.sku;
    form.name = p.name;
    form.category = p.category;
    form.unitPrice = formatPrice(p.unitPrice);
    form.quantityOnHand = std::to_string(p.quantityOnHand);
    form.reorderThreshold = std::to_string(p.reorderThreshold);
    form.supplierId = p.supplierId;
    form.imagePath = p.imagePath;
}

void drawImagePicker(const char* idSuffix, std::string& imagePath) {
    ImGui::InputText((std::string("Image path##") + idSuffix).c_str(), &imagePath);
    ImGui::SameLine();
    if (ImGui::Button((std::string("Browse...##") + idSuffix).c_str())) {
        std::string picked = pickImageFile();
        if (!picked.empty()) {
            imagePath = picked;
        }
    }
    if (!imagePath.empty()) {
        const Texture* preview = getTexture(imagePath);
        if (preview != nullptr) {
            ImGui::Image((ImTextureID)(intptr_t)preview->id, ImVec2(96, 96));
        } else {
            ImGui::TextColored(ImVec4(0.92f, 0.35f, 0.35f, 1.0f), "Could not load that image.");
        }
    }
}

void drawProductsTab(Inventory& inv, AddProductForm& addForm, EditProductForm& editForm,
                      std::string& pendingRemoveSku, StatusMessage& status) {
    bool openEditPopup = false;
    bool openRemovePopup = false;
    if (ImGui::CollapsingHeader("Add product", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("SKU (product ID): %s", addForm.sku.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Regenerate##sku")) {
            addForm.sku = generateSku();
        }
        ImGui::InputText("Name##add", &addForm.name);
        ImGui::InputText("Category##add", &addForm.category);
        ImGui::InputText("Unit price##add", &addForm.unitPrice);
        ImGui::InputText("Quantity on hand##add", &addForm.quantityOnHand);
        ImGui::InputText("Reorder threshold##add", &addForm.reorderThreshold);
        ImGui::InputText("Supplier ID##add", &addForm.supplierId);
        drawImagePicker("add", addForm.imagePath);
        if (ImGui::Button("Add product##submit")) {
            try {
                Product p;
                p.sku = addForm.sku;
                p.name = addForm.name;
                p.category = addForm.category;
                p.unitPrice = std::stod(addForm.unitPrice);
                p.quantityOnHand = std::stoi(addForm.quantityOnHand);
                p.reorderThreshold = std::stoi(addForm.reorderThreshold);
                p.supplierId = addForm.supplierId;
                p.imagePath = addForm.imagePath;
                inv.addProduct(p);
                status = StatusMessage{"Added " + p.sku + ".", false};
                addForm = AddProductForm{};
            } catch (const std::exception& e) {
                status = StatusMessage{std::string("Error: ") + e.what(), true};
            }
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Products (red = at or below reorder threshold)");
    if (ImGui::BeginTable("products", 8,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Photo");
        ImGui::TableSetupColumn("SKU (ID)");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Category");
        ImGui::TableSetupColumn("Price");
        ImGui::TableSetupColumn("Qty");
        ImGui::TableSetupColumn("Reorder");
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableHeadersRow();

        for (const auto& p : inv.products()) {
            ImGui::PushID(p.sku.c_str());
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            const Texture* texture = getTexture(p.imagePath);
            if (texture != nullptr) {
                ImGui::Image((ImTextureID)(intptr_t)texture->id, ImVec2(32, 32));
            } else {
                ImGui::TextUnformatted("-");
            }

            bool lowStock = p.quantityOnHand <= p.reorderThreshold;
            if (lowStock) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.35f, 0.35f, 1.0f));
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(p.sku.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(p.name.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(p.category.c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.2f", p.unitPrice);
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%d", p.quantityOnHand);
            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%d", p.reorderThreshold);
            if (lowStock) {
                ImGui::PopStyleColor();
            }

            ImGui::TableSetColumnIndex(7);
            if (ImGui::SmallButton("Edit")) {
                beginEditProduct(editForm, p);
                openEditPopup = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) {
                pendingRemoveSku = p.sku;
                openRemovePopup = true;
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (openEditPopup) { ImGui::OpenPopup("Edit product"); openEditPopup = false; }
    if (openRemovePopup) { ImGui::OpenPopup("Confirm remove product"); openRemovePopup = false; }
    if (ImGui::BeginPopupModal("Edit product", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Editing %s", editForm.originalSku.c_str());
        ImGui::InputText("Name##edit", &editForm.name);
        ImGui::InputText("Category##edit", &editForm.category);
        ImGui::InputText("Unit price##edit", &editForm.unitPrice);
        ImGui::InputText("Quantity on hand##edit", &editForm.quantityOnHand);
        ImGui::InputText("Reorder threshold##edit", &editForm.reorderThreshold);
        ImGui::InputText("Supplier ID##edit", &editForm.supplierId);
        drawImagePicker("edit", editForm.imagePath);

        if (ImGui::Button("Save changes")) {
            try {
                Product updated;
                updated.sku = editForm.originalSku;
                updated.name = editForm.name;
                updated.category = editForm.category;
                updated.unitPrice = std::stod(editForm.unitPrice);
                updated.quantityOnHand = std::stoi(editForm.quantityOnHand);
                updated.reorderThreshold = std::stoi(editForm.reorderThreshold);
                updated.supplierId = editForm.supplierId;
                updated.imagePath = editForm.imagePath;
                inv.editProduct(editForm.originalSku, updated);
                status = StatusMessage{"Updated " + editForm.originalSku + ".", false};
                editForm = EditProductForm{};
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& e) {
                status = StatusMessage{std::string("Error: ") + e.what(), true};
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##edit")) {
            editForm = EditProductForm{};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Confirm remove product", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Remove product %s? This cannot be undone.", pendingRemoveSku.c_str());
        if (ImGui::Button("Remove")) {
            try {
                inv.removeProduct(pendingRemoveSku);
                status = StatusMessage{"Removed " + pendingRemoveSku + ".", false};
            } catch (const std::exception& e) {
                status = StatusMessage{std::string("Error: ") + e.what(), true};
            }
            pendingRemoveSku.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##removeProduct")) {
            pendingRemoveSku.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void drawSuppliersTab(Inventory& inv, AddSupplierForm& form, std::string& pendingRemoveSupplierId,
                       StatusMessage& status) {
    bool openRemoveSupplierPopup = false;
    if (ImGui::CollapsingHeader("Add supplier", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("Supplier ID is generated for you. Fill in the name and contact, then click Add supplier.");
        if (form.id.empty()) {
            form.id = generateSupplierId(inv);
        }
        ImGui::Text("Supplier ID: %s", form.id.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Regenerate##sup")) {
            form.id = generateSupplierId(inv);
        }
        ImGui::InputText("Name##sup", &form.name);
        ImGui::InputText("Contact##sup", &form.contact);
        if (ImGui::Button("Add supplier##submit")) {
            try {
                Supplier s;
                s.id = form.id;
                s.name = form.name;
                s.contact = form.contact;
                inv.addSupplier(s);
                status = StatusMessage{"Added supplier " + s.id + ".", false};
                form = AddSupplierForm{};
            } catch (const std::exception& e) {
                status = StatusMessage{std::string("Error: ") + e.what(), true};
            }
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Suppliers");
    ImGui::TextWrapped("A supplier can only be removed once no product still lists it as the supplier.");
    if (ImGui::BeginTable("suppliers", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Contact");
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableHeadersRow();
        for (const auto& s : inv.suppliers()) {
            ImGui::PushID(s.id.c_str());
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(s.id.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(s.name.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(s.contact.c_str());
            ImGui::TableSetColumnIndex(3);
            if (ImGui::SmallButton("Remove")) {
                pendingRemoveSupplierId = s.id;
                openRemoveSupplierPopup = true;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (openRemoveSupplierPopup) { ImGui::OpenPopup("Confirm remove supplier"); openRemoveSupplierPopup = false; }
    if (ImGui::BeginPopupModal("Confirm remove supplier", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Remove supplier %s?", pendingRemoveSupplierId.c_str());
        ImGui::TextWrapped("This is rejected if any product still references this supplier.");
        if (ImGui::Button("Remove")) {
            try {
                inv.removeSupplier(pendingRemoveSupplierId);
                status = StatusMessage{"Removed supplier " + pendingRemoveSupplierId + ".", false};
            } catch (const std::exception& e) {
                status = StatusMessage{std::string("Error: ") + e.what(), true};
            }
            pendingRemoveSupplierId.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##removeSupplier")) {
            pendingRemoveSupplierId.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void drawMovementTab(Inventory& inv, MovementForm& form, StatusMessage& status) {
    ImGui::TextUnformatted("Record a stock movement");
    ImGui::InputText("Product SKU##mv", &form.sku);
    const char* typeLabels[] = {"Stock in", "Stock out", "Adjustment (positive correction)"};
    ImGui::Combo("Type##mv", &form.typeIndex, typeLabels, IM_ARRAYSIZE(typeLabels));
    ImGui::InputText("Quantity##mv", &form.quantity);
    ImGui::InputText("Note##mv", &form.note);
    if (ImGui::Button("Record movement")) {
        try {
            TransactionType type = form.typeIndex == 0   ? TransactionType::StockIn
                                    : form.typeIndex == 1 ? TransactionType::StockOut
                                                           : TransactionType::Adjustment;
            int quantity = std::stoi(form.quantity);
            Transaction t = inv.recordMovement(form.sku, type, quantity, form.note);
            status = StatusMessage{"Recorded " + toString(t.type) + " of " + std::to_string(quantity) +
                                        " for " + form.sku + ".",
                                    false};
            form = MovementForm{};
        } catch (const std::exception& e) {
            status = StatusMessage{std::string("Error: ") + e.what(), true};
        }
    }
}

void drawTransactionsTab(const Inventory& inv) {
    ImGui::TextUnformatted("Transaction history");
    if (ImGui::BeginTable("transactions", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Timestamp");
        ImGui::TableSetupColumn("SKU");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Delta");
        ImGui::TableSetupColumn("Note");
        ImGui::TableHeadersRow();
        for (const auto& t : inv.transactions()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(t.timestamp.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(t.productSku.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(toString(t.type).c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", t.quantityDelta);
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(t.note.c_str());
        }
        ImGui::EndTable();
    }
}

}  // namespace

int main() {
    glfwSetErrorCallback([](int error, const char* description) {
        std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
    });
    if (!glfwInit()) {
        return 1;
    }

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    const char* glslVersion = "#version 150";
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    const char* glslVersion = "#version 130";
#endif

    GLFWwindow* window = glfwCreateWindow(1100, 750, "Smart Inventory Management System", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    applyTheme();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    Inventory inventory("products.dat", "suppliers.dat", "transactions.dat");
    AddProductForm addProductForm;
    EditProductForm editProductForm;
    AddSupplierForm addSupplierForm;
    MovementForm movementForm;
    StatusMessage status;
    std::string pendingRemoveSku;
    std::string pendingRemoveSupplierId;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
        ImGui::Begin("Smart Inventory Management System");

        if (!status.text.empty()) {
            ImGui::TextColored(status.isError ? ImVec4(0.92f, 0.35f, 0.35f, 1.0f)
                                               : ImVec4(0.40f, 0.80f, 0.45f, 1.0f),
                                "%s", status.text.c_str());
            ImGui::Separator();
        }

        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem("Products")) {
                drawProductsTab(inventory, addProductForm, editProductForm, pendingRemoveSku, status);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Suppliers")) {
                drawSuppliersTab(inventory, addSupplierForm, pendingRemoveSupplierId, status);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Stock movement")) {
                drawMovementTab(inventory, movementForm, status);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Transactions")) {
                drawTransactionsTab(inventory);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();

        ImGui::Render();
        int displayW = 0;
        int displayH = 0;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.11f, 0.12f, 0.16f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
