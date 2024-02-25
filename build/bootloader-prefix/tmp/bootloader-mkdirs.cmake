# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/saket/esp/esp-idf/components/bootloader/subproject"
  "/Users/saket/devlopment/v3-FOMO+CLASSIFICATION-working/esp-tflite-micro/examples/person_detection/build/bootloader"
  "/Users/saket/devlopment/v3-FOMO+CLASSIFICATION-working/esp-tflite-micro/examples/person_detection/build/bootloader-prefix"
  "/Users/saket/devlopment/v3-FOMO+CLASSIFICATION-working/esp-tflite-micro/examples/person_detection/build/bootloader-prefix/tmp"
  "/Users/saket/devlopment/v3-FOMO+CLASSIFICATION-working/esp-tflite-micro/examples/person_detection/build/bootloader-prefix/src/bootloader-stamp"
  "/Users/saket/devlopment/v3-FOMO+CLASSIFICATION-working/esp-tflite-micro/examples/person_detection/build/bootloader-prefix/src"
  "/Users/saket/devlopment/v3-FOMO+CLASSIFICATION-working/esp-tflite-micro/examples/person_detection/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/saket/devlopment/v3-FOMO+CLASSIFICATION-working/esp-tflite-micro/examples/person_detection/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/saket/devlopment/v3-FOMO+CLASSIFICATION-working/esp-tflite-micro/examples/person_detection/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
