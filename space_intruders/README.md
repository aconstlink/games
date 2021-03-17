# space intruders
This is a space invaders clone. 

The main purpose of this game is to check certain engine features and not to fully clone the original game.

![sample](https://github.com/aconstlink/games/tree/main/space_intruders/sample_image.png "Sample Image")

Since this is the first "real" interactive application besides the simplified [test apps](https://github.com/aconstlink/natus_tests) some bugs were expected. 

## application

- aspect ratio
- fixed render target dimensions and "desin space" and upscaling

## graphics
This game uses the sprite renderer in a bigger extended where more sprites are rendered animated than in any test app so far. The game revealed some issues with the aspect ratio of the final render so as with the sprite atlas ratio. Also all the space transformation matrices for the sprite_render_2d and all the primitive renderer where not correctly implemented.

A bigger issue with the graphics shader variable  update revealed here too. The solution really doubled the frame rate. Before, the render thread locked too long so that the user graphics callback was not able to prepare the next frame. This was mainly due to the uniform variable update in the gl backends where the variables where updated just before the render_object was rendered. This was changed so that all variables are not stored per variable set and per render object so that when a render object is executed, all variables for the rendered variable set are copied and so the state is captured. This is necessary because a gl program can be used multiple times and the variable would be overwritten if multiple render objects use the same shader. 
In essence, the frame lock could be moved infront of the section where all render objects are rendered and so the lock is released earlier and the user callback is called earlier. 

It is always nice to see that the reconfiguration of graphics objects is working well. So the framebuffer is resized if the window size changes. This is very important to keep the framebuffer sharp and not blurred out.

## audio

## physics
app s physics update 
- bug in update time if update is slower than 120 hz

## other issues