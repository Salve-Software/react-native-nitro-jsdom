#pragma once
#include <vector>
#include "HybridNitroJsdomSpec.hpp"

namespace margelo::nitro::nitrojsdom {
class HybridNitroJsdom : public HybridNitroJsdomSpec {
    public:
        HybridNitroJsdom() : HybridObject(TAG), HybridNitroJsdomSpec() {}
       
        double sum(double a, double b) override;
    };
} // namespace margelo::nitro::nitrojsdom
