# Maze Game 

# Compile 
g++ MazeGame.cpp glad/glad.c -o MazeGame \
    -I./glad -I./glm \
    -I/opt/homebrew/include \
    -L/opt/homebrew/lib \
    -lSDL2 -framework OpenGL

# Run 
./MazeGame [map_file]

# Maps
Three map files have been made from map1.txt being the simplest and only contain 1 key and 1 door

# Demo 

<p align="center">
  <img src="demo.gif" width="560">
</p>
