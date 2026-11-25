# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/ncs/F798-ssd1357/zephyr/samples/bluetooth/peripheral_nus")
  file(MAKE_DIRECTORY "C:/ncs/F798-ssd1357/zephyr/samples/bluetooth/peripheral_nus")
endif()
file(MAKE_DIRECTORY
  "C:/ncs/F798-ssd1357/zephyr/samples/bluetooth/peripheral_nus/build/peripheral_nus"
  "C:/ncs/F798-ssd1357/zephyr/samples/bluetooth/peripheral_nus/build/_sysbuild/sysbuild/images/peripheral_nus-prefix"
  "C:/ncs/F798-ssd1357/zephyr/samples/bluetooth/peripheral_nus/build/_sysbuild/sysbuild/images/peripheral_nus-prefix/tmp"
  "C:/ncs/F798-ssd1357/zephyr/samples/bluetooth/peripheral_nus/build/_sysbuild/sysbuild/images/peripheral_nus-prefix/src/peripheral_nus-stamp"
  "C:/ncs/F798-ssd1357/zephyr/samples/bluetooth/peripheral_nus/build/_sysbuild/sysbuild/images/peripheral_nus-prefix/src"
  "C:/ncs/F798-ssd1357/zephyr/samples/bluetooth/peripheral_nus/build/_sysbuild/sysbuild/images/peripheral_nus-prefix/src/peripheral_nus-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/ncs/F798-ssd1357/zephyr/samples/bluetooth/peripheral_nus/build/_sysbuild/sysbuild/images/peripheral_nus-prefix/src/peripheral_nus-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/ncs/F798-ssd1357/zephyr/samples/bluetooth/peripheral_nus/build/_sysbuild/sysbuild/images/peripheral_nus-prefix/src/peripheral_nus-stamp${cfgdir}") # cfgdir has leading slash
endif()
