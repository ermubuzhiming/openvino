# Copyright (C) 2018-2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET_NAME ov_snippets_func_tests)

addIeTargetTest(
        NAME ${TARGET_NAME}
        ROOT ${CMAKE_CURRENT_SOURCE_DIR}
        INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            $<TARGET_PROPERTY:inference_engine_snippets,INTERFACE_INCLUDE_DIRECTORIES>
        LINK_LIBRARIES
            openvino::runtime::dev
            commonTestUtils
            snippetsNgraphFunctions
        ADD_CPPLINT
        LABELS
        IE OV SNIPPETS
)

# LTO
set_target_properties(${TARGET_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ${ENABLE_LTO})

ie_faster_build(${TARGET_NAME}
    UNITY
    PCH PRIVATE "src/precomp.hpp"
)

add_library(snippets_test_utils STATIC ${CMAKE_CURRENT_SOURCE_DIR}/include/lowering_utils.hpp
                                       ${CMAKE_CURRENT_SOURCE_DIR}/src/lowering_utils.cpp)
target_include_directories(snippets_test_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(snippets_test_utils PRIVATE commonTestUtils snippetsNgraphFunctions)
