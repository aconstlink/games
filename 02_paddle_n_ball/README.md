# space intruders
This is a brakeout clone. 

The main purpose of this game is to check certain engine features and not to fully clone the original game. Another important part of this game should be to start iterating towards a little and usable game framework. 

This game may look simple but there is acutally alot going on in the background!

![sample](https://github.com/aconstlink/games/blob/main/02_paddle_n_ball/sample_image.png "Sample Image")


## controls
Keyboard : **a** | left / **d** | right
XBox Ctrl : **left analog** | left and right  
Choose Level : **1-4**  

## application
The physics are a little bit off. The reflection of the ball from the paddle depends on the hit region. The collision detection is not implemented right for the corners of the paddle as well as the bricks.

Levels are loaded from the files in the __layout__ folder. A '.' means an empty space and a '#' means a brick.

## key-issues
This games' main purpose is to test the async task system. Loading all the assets is done using the new task system. A level is loaded using the task system from the disk if all bricks have been hit. 

## further issues
At the moment, there is a subtle stuttering in the continuous movement of everything. This issue was reduced due to using a "global" app wide delta time but it still remain. Especially for the OpenGL backend on windows.

## conculsion
As with the first game, everything works the same. 
