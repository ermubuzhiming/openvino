// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "aten_getitem_replacer.hpp"

#include <memory>
#include <utility>

#include "openvino/core/rt_info.hpp"
#include "openvino/op/add.hpp"
#include "openvino/op/ceiling.hpp"
#include "openvino/op/constant.hpp"
#include "openvino/op/convert.hpp"
#include "openvino/op/divide.hpp"
#include "openvino/op/gather.hpp"
#include "openvino/op/multiply.hpp"
#include "openvino/op/range.hpp"
#include "openvino/op/shape_of.hpp"
#include "openvino/op/slice.hpp"
#include "openvino/op/squeeze.hpp"
#include "openvino/op/unsqueeze.hpp"
#include "openvino/op/util/framework_node.hpp"
#include "openvino/op/variadic_split.hpp"
#include "openvino/opsets/opset10.hpp"
#include "openvino/pass/pattern/matcher.hpp"
#include "openvino/pass/pattern/op/wrap_type.hpp"
#include "pt_framework_node.hpp"
#include "utils.hpp"

namespace ov {
namespace frontend {
namespace pytorch {
namespace pass {

AtenGetItemReplacer::AtenGetItemReplacer() {
    auto getitem = ov::pass::pattern::wrap_type<ov::op::util::FrameworkNode>();

    ov::matcher_pass_callback callback = [](ov::pass::pattern::Matcher& m) {
        auto getitem = cast_fw_node(m.get_match_root(), "aten::__getitem__");
        if (!getitem)
            return false;

        auto input_node = getitem->input_value(0).get_node_shared_ptr();
        if (auto torch_split = cast_fw_node(input_node, "aten::split")) {
            auto rank = torch_split->input(1).get_partial_shape().rank();
            if (rank.is_dynamic()) {
                return false;
            }
            if (rank.get_length() == 0) {
                // Based on slice_size and output index select size.
                // Constants required by transformation.
                auto const_1 = ov::op::v0::Constant::create(element::i32, Shape{1}, {1});
                auto const_1_0d = ov::op::v0::Constant::create(element::i32, Shape{}, {1});
                auto const_0 = ov::op::v0::Constant::create(element::i32, Shape{1}, {0});
                auto const_0_0d = ov::op::v0::Constant::create(element::i32, Shape{}, {0});

                // Load and convert op inputs.
                auto input = torch_split->get_input_source_output(0);
                auto split_size = torch_split->get_input_source_output(1);
                auto split_size_1d = std::make_shared<ov::op::v0::Unsqueeze>(split_size, const_0);
                auto axis = torch_split->get_input_source_output(2);
                auto axis_1d = std::make_shared<ov::op::v0::Unsqueeze>(axis, const_0);
                auto getitem_idx = getitem->input(1).get_source_output();

                // Calculate number of splits based on input shape and split_size.
                auto shape = std::make_shared<ov::op::v3::ShapeOf>(input, element::i32);
                auto len_to_split = std::make_shared<ov::op::v8::Gather>(shape, axis, const_0);
                // Convert to f64 from int to calculate reminder - last chunk can be smaller if Shape in given axis is
                // not equally divisible.
                auto len_to_split_float = std::make_shared<ov::op::v0::Convert>(len_to_split, element::f64);
                auto split_size_1d_float = std::make_shared<ov::op::v0::Convert>(split_size_1d, element::f64);
                auto out_div = std::make_shared<ov::op::v1::Divide>(len_to_split_float, split_size_1d_float);
                auto out_num = std::make_shared<ov::op::v0::Ceiling>(out_div);
                auto out_num_0d = std::make_shared<ov::op::v0::Squeeze>(out_num, const_0);

                // Use Range and Gather to convert negative getitem indexes into positive due problems with indexing
                // with -1.
                auto possible_out_idx = std::make_shared<ov::op::v4::Range>(const_0_0d,
                                                                            out_num_0d,
                                                                            const_1_0d,
                                                                            split_size.get_element_type());
                auto always_positive_out_idx =
                    std::make_shared<ov::op::v8::Gather>(possible_out_idx, getitem_idx, const_0);

                // Use Slice to get only split output selected by getitem idx. Couldn't use VariadicSplit due to
                // problems with dynamic inputs.
                auto split_slice_start = std::make_shared<ov::op::v1::Multiply>(always_positive_out_idx, split_size_1d);
                auto split_slice_end = std::make_shared<ov::op::v1::Add>(split_slice_start, split_size_1d);
                auto split =
                    std::make_shared<ov::op::v8::Slice>(input, split_slice_start, split_slice_end, const_1, axis_1d);
                copy_runtime_info({getitem, input_node}, split);
                replace_node(getitem, split);
            } else {
                auto getitem_index_ptr = getitem->input_value(1).get_node_shared_ptr();
                auto getitem_index_const = std::dynamic_pointer_cast<ov::op::v0::Constant>(getitem_index_ptr);
                auto split = std::make_shared<ov::op::v1::VariadicSplit>(torch_split->get_input_source_output(0),
                                                                         torch_split->get_input_source_output(2),
                                                                         torch_split->get_input_source_output(1));
                auto index_val = getitem_index_const->cast_vector<int64_t>();
                if (index_val.size() != 1) {
                    return false;
                }
                auto index = index_val[0];
                if (index < 0) {
                    index = split->outputs().size() + index;
                }
                OutputVector res{split->outputs()[index]};
                copy_runtime_info({getitem, input_node}, split);
                replace_node(getitem, res);
            }
            return true;
        }
        if (auto list_construct = cast_fw_node(input_node, "prim::ListConstruct")) {
            auto getitem_idx = getitem->input_value(1).get_node_shared_ptr();
            auto getitem_idx_const = std::dynamic_pointer_cast<ov::op::v0::Constant>(getitem_idx);
            if (getitem_idx_const) {
                auto idx = getitem_idx_const->cast_vector<int64_t>();
                auto element = list_construct->input_value(idx[0]).get_node_shared_ptr();
                copy_runtime_info({getitem, input_node}, element);
                replace_node(getitem, element);
                return true;
            }
            auto input_concat = concat_list_construct(list_construct);
            auto zero = ov::op::v0::Constant::create(element::i32, Shape{}, {0});
            auto gather = std::make_shared<ov::op::v8::Gather>(input_concat, getitem_idx, zero);
            copy_runtime_info({getitem, input_node}, gather);
            replace_node(getitem, gather);
            return true;
        }
        if (auto chunk = cast_fw_node(input_node, "aten::chunk")) {
            auto input_tensor = chunk->get_input_source_output(0);
            auto chunks_i32 = chunk->get_input_source_output(1);
            auto dim_i32 = chunk->get_input_source_output(2);

            auto const_0 = opset10::Constant::create(element::i64, Shape{1}, {0});
            auto const_1 = opset10::Constant::create(element::i64, Shape{1}, {1});
            auto const_0_nodim = opset10::Constant::create(element::i64, Shape{}, {0});

            auto getitem_index_i32 = getitem->get_input_source_output(1);
            auto getitem_index_i64 = std::make_shared<opset10::Convert>(getitem_index_i32, element::i64);
            auto getitem_index = std::make_shared<opset10::Unsqueeze>(getitem_index_i64, const_0);
            auto dim_i64 = std::make_shared<opset10::Convert>(dim_i32, element::i64);
            auto dim = std::make_shared<opset10::Unsqueeze>(dim_i64, const_0);
            auto chunks = std::make_shared<opset10::Convert>(chunks_i32, element::i64);

            auto input_shape = std::make_shared<opset10::ShapeOf>(input_tensor);
            auto input_dimension = std::make_shared<opset10::Gather>(input_shape, dim, const_0);
            auto input_size = std::make_shared<opset10::Squeeze>(input_dimension);

            auto chunk_size = std::make_shared<opset10::Divide>(input_size, chunks, true);
            auto last_chunk_size = std::make_shared<opset10::Mod>(input_size, chunks);
            auto is_last_nonzero = std::make_shared<opset10::Greater>(last_chunk_size, const_0_nodim);
            auto is_last_nonzero_int = std::make_shared<opset10::Convert>(is_last_nonzero, element::i64);

            auto computed_chunk_size = std::make_shared<opset10::Add>(chunk_size, is_last_nonzero_int);
            auto computed_last_chunk_size = std::make_shared<opset10::Mod>(input_size, computed_chunk_size);
            auto computed_is_last_nonzero = std::make_shared<opset10::Greater>(computed_last_chunk_size, const_0_nodim);
            auto computed_chunks = std::make_shared<opset10::Divide>(input_size, computed_chunk_size, true);

            auto is_slice_normal_size = std::make_shared<opset10::Less>(getitem_index, computed_chunks);
            auto is_slice_not_normal_size = std::make_shared<opset10::GreaterEqual>(getitem_index, computed_chunks);
            auto is_slice_normal_size_int = std::make_shared<opset10::Convert>(is_slice_normal_size, element::i64);
            auto is_slice_not_normal_size_int =
                std::make_shared<opset10::Convert>(is_slice_not_normal_size, element::i64);

            auto slice_size_lhs = std::make_shared<opset10::Multiply>(is_slice_normal_size_int, computed_chunk_size);
            auto slice_size_rhs =
                std::make_shared<opset10::Multiply>(is_slice_not_normal_size_int, computed_last_chunk_size);
            auto slice_size = std::make_shared<opset10::Add>(slice_size_lhs, slice_size_rhs);

            auto slice_begin = std::make_shared<opset10::Multiply>(getitem_index, computed_chunk_size);
            auto slice_end = std::make_shared<opset10::Add>(slice_begin, slice_size);

            auto sliced_chunk = std::make_shared<opset10::Slice>(input_tensor, slice_begin, slice_end, const_1, dim);

            copy_runtime_info({getitem, input_node}, sliced_chunk);
            replace_node(getitem, sliced_chunk);

            return true;
        }

        return false;
    };

    auto m = std::make_shared<ov::pass::pattern::Matcher>(getitem, "ov::frontend::pytorch::pass::AtenGetItemReplacer");
    this->register_matcher(m, callback);
};

}  // namespace pass
}  // namespace pytorch
}  // namespace frontend
}  // namespace ov
