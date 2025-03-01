# Configurations for Intel® Processor Graphics (GPU) with OpenVINO™ {#openvino_docs_install_guides_configurations_for_intel_gpu}


@sphinxdirective

.. _gpu guide:


In case if you are intended to use OpenVINO GPU plugin and offload network inference to Intel® graphics processor, the Intel Graphics Driver should be properly configured on your system.

If it is already installed, and you want to keep it, you can skip the installation steps.

Linux
#####

To install the latest available **Intel® Graphics Compute Runtime for OpenCL™** for your OS, see the `Install Guides <https://github.com/intel/compute-runtime/releases/latest>`__ .

.. note::
   If you use RedHat 8 OS please install OpenCL library as prerequisite via following command line: ``sh rpm -ivh http://mirror.centos.org/centos/8-stream/AppStream/x86_64/os/Packages/ocl-icd-2.2.12-1.el8.x86_64.rpm``

.. note::
   For instructions specific to discrete graphics platforms, refer to `the dgpu guide <https://dgpu-docs.intel.com/installation-guides/index.html>`__ (Intel® Arc™ A-Series Graphics, Intel® Data Center GPU Flex Series, Intel® Data Center GPU MAX Series, Intel® processor graphics Gen12, and Intel® Iris Xe MAX codename DG1).

You may consider installing one of the earlier versions of the driver, based on your particular setup needs.

It is recommended that you refer to the `Intel® Graphics Compute Runtime Github page <https://github.com/intel/compute-runtime/>`__ for instructions and recommendations on GPU driver installation specific to particular releases, including the list of supported hardware platforms.


.. _gpu guide windows:


Windows
#######

To install the Intel Graphics Driver for Windows on your hardware, please proceed with the `instruction <https://www.intel.com/content/www/us/en/support/articles/000005629/graphics.html>`__ . 

To check if you have this driver installed:

1. Type **device manager** in your **Search Windows** box and press Enter. The **Device Manager** opens.
2. Click the drop-down arrow to view the **Display adapters**. You can see the adapter that is installed in your computer:  

   .. image:: _static/images/DeviceManager.PNG

3. Right-click the adapter name and select **Properties**.
4. Click the **Driver** tab to see the driver version.  

   .. image:: _static/images/DeviceDriverVersion.PNG

You are done updating your device driver and are ready to use your GPU.

Additional info
###############

In the internal OpenVINO validation the following versions of Intel Graphics Driver were used:

+------------------+-------------------------------------------------------------------------------------+
| Operation System | Driver version                                                                      |
+==================+=====================================================================================+
| Ubuntu 20.04     | `22.35.24055 <https://github.com/intel/compute-runtime/releases/tag/22.35.24055>`__ |
+------------------+-------------------------------------------------------------------------------------+
| Ubuntu 18.04     | `21.38.21026 <https://github.com/intel/compute-runtime/releases/tag/21.38.21026>`__ |
+------------------+-------------------------------------------------------------------------------------+
| CentOS 7         | `19.41.14441 <https://github.com/intel/compute-runtime/releases/tag/19.41.14441>`__ |
+------------------+-------------------------------------------------------------------------------------+
| RHEL 8           | `22.28.23726 <https://github.com/intel/compute-runtime/releases/tag/22.28.23726>`__ |
+------------------+-------------------------------------------------------------------------------------+


What’s Next?
############

You can try out the toolkit with:

Developing in Python:

* `Start with tensorflow models with OpenVINO™ <notebooks/101-tensorflow-to-openvino-with-output.html>`__
* `Start with ONNX and PyTorch models with OpenVINO™ <notebooks/102-pytorch-onnx-to-openvino-with-output.html>`__
* `Start with PaddlePaddle models with OpenVINO™ <notebooks/103-paddle-to-openvino-classification-with-output.html>`__

Developing in C++:

* :doc:`Image Classification Async C++ Sample <openvino_inference_engine_samples_classification_sample_async_README>`
* :doc:`Hello Classification C++ Sample <openvino_inference_engine_samples_hello_classification_README>`
* :doc:`Hello Reshape SSD C++ Sample <openvino_inference_engine_samples_hello_reshape_ssd_README>`


@endsphinxdirective


