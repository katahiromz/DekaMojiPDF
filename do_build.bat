del CMakeCache.txt
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS:BOOL=OFF .
ninja
strip build\DMPDF.exe
