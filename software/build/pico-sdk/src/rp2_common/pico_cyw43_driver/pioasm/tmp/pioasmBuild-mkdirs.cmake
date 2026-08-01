# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/Cristiano/.pico-sdk/sdk/2.2.0/tools/pioasm"
  "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/pioasm"
  "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/pioasm-install"
  "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/tmp"
  "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
  "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src"
  "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
