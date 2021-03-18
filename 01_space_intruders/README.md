# space intruders
This is a space invaders clone. 

The main purpose of this game is to check certain engine features and not to fully clone the original game. Another important part of this game should be to start iterating towards a little and usable game framework. 

This game may look simple but there is acutally alot going on in the background!

![sample](https://github.com/aconstlink/games/blob/main/01_space_intruders/sample_image.png "Sample Image")

Since this is the first "real" interactive application besides the simplified [test apps](https://github.com/aconstlink/natus_tests) some bugs were expected. 

## controls
Keyboard : **a** | left / **d** | right / **space** | shoot / **F2** | Debug menu  
XBox Ctrl : **left analog** | left and right / **A** | shoot

## application

The design space is 800:600 meaning that the projection matrix is set to those value and the game lifes in this space. If the window is scaled, the backbuffer is scaled along with it too and a outside aspect ratio area will be shown. Here the render target quad and the rendered game framebuffer is scaled along with the window in the right aspect. It is good to see that reconfiguring the framebuffer at run-time work well because rescaling the framebuffer here actually creates a new one in the graphics backends. 

All the apps' user callbacks work as expected and run at the set hz. The physics callback is changed so that when it runs at 120 hz and it misses that time window, the physics callback is call again so that no time is lost. There could be several strategies to the issue of loosing time frames. 

## graphics
This game uses the sprite renderer in a bigger extended where more sprites are rendered animated than in any test app so far. The game revealed some issues with the aspect ratio of the final render so as with the sprite atlas ratio. Also all the space transformation matrices for the sprite_render_2d and all the primitive renderer where not correctly implemented.

A bigger issue with the graphics shader variable update revealed here too. The solution really doubled the frame rate. Before, the render thread locked too long so that the user graphics callback was not able to prepare the next frame. This was mainly due to the uniform variable update in the gl backends where the variables where updated just before the render_object was rendered. This was changed so that all variables are now stored per variable set and per render object so that when a render object is executed, all variables for the rendered variable set are copied and so the state is captured. This is necessary because a gl program can be used multiple times and the variable would be overwritten if multiple render objects use the same shader. 
In essence, the frame lock could be moved infront of the section where all render objects are rendered and so the lock is released earlier and the user callback is called earlier. 

It is always nice to see that the reconfiguration of graphics objects is working well. So the framebuffer is resized if the window size changes. This is very important to keep the framebuffer sharp and not blurred out.

Across all graphics backends, the game renders well. The OpenGL 3 backend shows some stuttering on windows which is not observable using the d3d11 backend. This is not the case on linux where gl3 and es3 render well. The gl3 backend stuttering issue may be related to the uniform variable update which might be fixed when using uniform buffers. d3d11 uses constant buffers for app->shader variable exchange.

## physics and collision
The physics and collision is very simple. Projectiles hitting objects will be noticed but there is no collision response besides that.

## animation
The animation "system" is implemented in this application in order to check what is needed to implement such a thing in the engine directly. The sprite renderer is used for showing the currently animated sprite image. The animation and the image reference is imported from the natus animation files and show correct working.

Although the animation is working well and is more complex than in other tests before, there is no dying animation.

## audio
Audio just worked right out of the box as implemented in the test applications. There I found two issues. One where the OpenAL buffer was allocated too big which just revealed when playing the audio in looping mode. The seconds issue was that looping was not implemented.

## conclusion
There need to be done something in order to handle objects better.
- Assets need to be handled in some sort of manager or database which can be passed around and any instance can just grap the required assets.
- Physics and collision need to be encapsulated in a separate state that a game object could use
- Task system with plug-able tasks for conditional and dependent task processing
- Event system for queuing events for later processing
