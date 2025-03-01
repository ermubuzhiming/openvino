// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "acl_reduce.hpp"

namespace ov {
namespace intel_cpu {

using namespace arm_compute;

static arm_compute::ReductionOperation getAclReductionOperationByAlgorithm(Algorithm algorithm) {
    switch (algorithm) {
        case Algorithm::ReduceMax:  return arm_compute::ReductionOperation::MAX;
        case Algorithm::ReduceMin:  return arm_compute::ReductionOperation::MIN;
        case Algorithm::ReduceSum:  return arm_compute::ReductionOperation::SUM;
        case Algorithm::ReduceProd: return arm_compute::ReductionOperation::PROD;
        default:                    IE_THROW() << "Unsupported reduction operation: " << static_cast<int>(algorithm);
    }
}

AclReduceExecutor::AclReduceExecutor(const ExecutorContext::CPtr context) : ReduceExecutor(context) {}

bool AclReduceExecutor::init(const ReduceAttrs& reduceAttrs,
                          const std::vector<MemoryDescPtr>& srcDescs,
                          const std::vector<MemoryDescPtr>& dstDescs,
                          const dnnl::primitive_attr &attr) {
    if (reduceAttrs.operation != Algorithm::ReduceMax &&
        reduceAttrs.operation != Algorithm::ReduceMin &&
        reduceAttrs.operation != Algorithm::ReduceSum &&
        reduceAttrs.operation != Algorithm::ReduceProd &&
        reduceAttrs.operation != Algorithm::ReduceMean) {
            DEBUG_LOG("Unknown reduce algorithm passed into AclReduceExecutor: ", static_cast<int>(reduceAttrs.operation));
            return false;
        }

    this->reduceAttrs = reduceAttrs;

    auto srcDims = srcDescs[0]->getShape().getStaticDims();
    auto dstDims = dstDescs[0]->getShape().getStaticDims();

    TensorInfo srcTensorInfo = TensorInfo(shapeCast(srcDims), 1,
    precisionToAclDataType(srcDescs[0]->getPrecision()), getAclDataLayoutByMemoryDesc(srcDescs[0]));
    TensorInfo dstTensorInfo = TensorInfo(shapeCast(dstDims), 1,
    precisionToAclDataType(dstDescs[0]->getPrecision()), getAclDataLayoutByMemoryDesc(dstDescs[0]));

    srcTensor.allocator()->init(srcTensorInfo);
    dstTensor.allocator()->init(dstTensorInfo);

    switch (reduceAttrs.operation) {
        case Algorithm::ReduceMean: {
            for (size_t i = 0; i < reduceAttrs.axes.size(); ++i) {
                auto axe = axisCast(reduceAttrs.axes[i], srcDims.size());
                auto pos = axisCast(i, reduceAttrs.axes.size());
                axesMean.set(pos, axe);
            }
            Status reduceMeanStatus = NEReduceMean::validate(&srcTensorInfo, axesMean, reduceAttrs.keepDims, &dstTensorInfo);
            if (!reduceMeanStatus) {
                DEBUG_LOG("NEReduceMean validation failed: ", reduceMeanStatus.error_description());
                return false;
            }
            exec_func = [this]{
                auto acl_op = std::make_unique<arm_compute::NEReduceMean>();
                acl_op->configure(&srcTensor, axesMean, this->reduceAttrs.keepDims, &dstTensor);
                acl_op->run();
            };
            break;
        }
        case Algorithm::ReduceMax:
        case Algorithm::ReduceMin:
        case Algorithm::ReduceSum:
        case Algorithm::ReduceProd: {
            Status reductionOperationStatus = NEReductionOperation::validate(&srcTensorInfo, &dstTensorInfo, axisCast(reduceAttrs.axes[0], srcDims.size()),
                                                                             getAclReductionOperationByAlgorithm(reduceAttrs.operation), reduceAttrs.keepDims);
            if (!reductionOperationStatus) {
                DEBUG_LOG("NEReductionOperation validation with indices failed: ", reductionOperationStatus.error_description());
                return false;
            }
            exec_func = [this, srcDims]{
                auto acl_op = std::make_unique<arm_compute::NEReductionOperation>();
                acl_op->configure(&srcTensor, &dstTensor, axisCast(this->reduceAttrs.axes[0], srcDims.size()),
                                    getAclReductionOperationByAlgorithm(this->reduceAttrs.operation), this->reduceAttrs.keepDims);
                acl_op->run();
            };
            break;
        }
        default:
            IE_THROW() << "Unsupported operation type for ACL Reduce executor: " << static_cast<int>(reduceAttrs.operation);
    }

    return true;
}

void AclReduceExecutor::exec(const std::vector<MemoryCPtr>& src, const std::vector<MemoryPtr>& dst, const void *post_ops_data_) {
    srcTensor.allocator()->import_memory(src[0]->GetPtr());
    dstTensor.allocator()->import_memory(dst[0]->GetPtr());

    exec_func();

    srcTensor.allocator()->free();
    dstTensor.allocator()->free();
}

}   // namespace intel_cpu
}   // namespace ov