# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/_deps/picotool-src"
  "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/_deps/picotool-build"
  "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/_deps"
  "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/picotool/tmp"
  "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/picotool/src/picotoolBuild-stamp"
  "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/picotool/src"
  "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/picotool/src/picotoolBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/picotool/src/picotoolBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/Cristiano/OneDrive/Personal/11. Electronics/RetroLink/GitHub/retrolink/software/build/picotool/src/picotoolBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
