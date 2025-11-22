# Maze Game 
Learned from a working maze in openGL[1]
Used wall texture from a bmp file from github repo [2]
Texture wall has been implemented from additional features 
I faced issues with texture mapping that solved when I found that the bmp file had BGR encoding and by converting to RGB it showes as it was supposed to be. 
Also had issue with collisions. I solved it by not onoly considering one point as the player. Instead adding radius that checks the 4 corner points around the player.


[1] https://www.opengl.org/archives/resources/code/samples/more_samples/
[2] https://github.com/sergey-tihon/Ravent-App-Store/tree/master/SuperSDG2/src



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
