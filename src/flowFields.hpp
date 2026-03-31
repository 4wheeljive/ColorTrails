#pragma once

#include "flowFields_detail.hpp"

namespace flowFields {
    extern bool flowFieldsInstance;

    void initFlowFields(uint16_t (*xy_func)(uint8_t, uint8_t));
    void runFlowFields();
}
