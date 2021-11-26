// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <locale>
#include <map>
#include <mutex>
#include <ngraph/factory.hpp>
#include <ngraph/node.hpp>
#include <openvino/core/preprocess/input_tensor_info.hpp>
#include <openvino/core/variant.hpp>
#include <openvino/core/any.hpp>
#include <set>
#include <transformations/rt_info/disable_constant_folding.hpp>
#include <transformations/rt_info/disable_fp16_compression.hpp>
#include <transformations/rt_info/fused_names_attribute.hpp>
#include <transformations/rt_info/nms_selected_indices.hpp>
#include <transformations/rt_info/old_api_map_element_type_attribute.hpp>
#include <transformations/rt_info/old_api_map_order_attribute.hpp>
#include <transformations/rt_info/primitives_priority_attribute.hpp>
#include <transformations/rt_info/strides_property.hpp>
#include <transformations/rt_info/decompression.hpp>
#include <transformations_visibility.hpp>
#include <utility>

namespace ov {
namespace pass {
class TRANSFORMATIONS_API Attributes {
public:
    Attributes();
    ~Attributes();

    std::shared_ptr<Variant> create_by_type_info(const ov::DiscreteTypeInfo& type_info);

private:
    template <class T>
    void register_factory() {
        m_factory_registry.emplace(T::get_type_info_static(), [] {
            return std::make_shared<T>();
        });
    }

    std::unordered_map<DiscreteTypeInfo, std::function<std::shared_ptr<Variant>()>> m_factory_registry;
};
}  // namespace pass
}  // namespace ov
