cd ..
mkdir build
cd build
cmake .. -DRELEASE=OFF
cmake --build . --config Debug

ECHO "Finished building Debug"