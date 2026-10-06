#pragma once

// Every piece of brand text on the panel, in one place (build prompt §4, DD-72). The plugin's
// name and vendor for hosts are set in CMakeLists.txt (PRODUCT_NAME / COMPANY_NAME).

namespace sgui::brand
{
inline constexpr const char* kMaker    = "S\xc2\xb7P\xc2\xb7K\xc2\xb7R";   // S·P·K·R (UTF-8)
inline constexpr const char* kFooter   = "SPKR";
inline constexpr const char* kProduct  = "Somii";
inline constexpr const char* kSubtitle = "BI-TIMBRAL POLYPHONIC SYNTHESIZER";
} // namespace sgui::brand
