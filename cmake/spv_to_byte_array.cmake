# Read a binary file and write a comma-separated byte literal sequence
# suitable for `#include` inside a `unsigned char[] = { ... };` initializer.
#
# Usage (driven from add_custom_command):
#   ${CMAKE_COMMAND} -DIN=<path.spv> -DOUT=<path.spv.inc>
#                    -P cmake/spv_to_byte_array.cmake
#
# Portable replacement for the C++26 `#embed` directive (which MSVC does
# not yet support).

if(NOT IN OR NOT OUT)
  message(FATAL_ERROR "spv_to_byte_array.cmake requires -DIN=<file> -DOUT=<file>")
endif()

file(READ "${IN}" _hex HEX)
string(LENGTH "${_hex}" _hex_len)
math(EXPR _byte_count "${_hex_len} / 2")

set(_out "")
set(_i 0)
set(_col 0)
while(_i LESS _hex_len)
  string(SUBSTRING "${_hex}" ${_i} 2 _byte)
  if(_col EQUAL 0)
    string(APPEND _out "  ")
  endif()
  string(APPEND _out "0x${_byte},")
  math(EXPR _col "${_col} + 1")
  if(_col EQUAL 16)
    string(APPEND _out "\n")
    set(_col 0)
  else()
    string(APPEND _out " ")
  endif()
  math(EXPR _i "${_i} + 2")
endwhile()
if(NOT _col EQUAL 0)
  string(APPEND _out "\n")
endif()

file(WRITE "${OUT}" "${_out}")
