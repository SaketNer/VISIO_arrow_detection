# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Espressif/frameworks/esp-idf-v5.1.2/components/bootloader/subproject"
  "C:/SOP/esp-tflite-micro/examples/person_detection/build/bootloader"
  "C:/SOP/esp-tflite-micro/examples/person_detection/build/bootloader-prefix"
  "C:/SOP/esp-tflite-micro/examples/person_detection/build/bootloader-prefix/tmp"
  "C:/SOP/esp-tflite-micro/examples/person_detection/build/bootloader-prefix/src/bootloader-stamp"
  "C:/SOP/esp-tflite-micro/examples/person_detection/build/bootloader-prefix/src"
  "C:/SOP/esp-tflite-micro/examples/person_detection/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/SOP/esp-tflite-micro/examples/person_detection/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/SOP/esp-tflite-micro/examples/person_detection/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
