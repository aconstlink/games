# tetrix
This is a tetris clone. 

The main purpose of this game is to check certain engine features and not to fully clone the original game. Another important part of this game should be to start iterating towards a little and usable game framework. 

This game may look simple but there is acutally alot going on in the background!

![sample](https://github.com/aconstlink/games/blob/main/03_tetrix/sample_image.png "Sample Image")


## controls
Keyboard : **a** | left / **d** | right / **s** | down

## application
This application has the most "complicated" collision detection so far and was a little bit tricky to implement. The players' shape is driven by a real physics step with some value of unit "pixels per secons". So per time step the shape may not move a cell. So that value need to be converted to cell space where the collision detection is done. During collision detection, the shape needs to be blocked from going further. For example, at the left and right borders of the window, the shape needs to stop moving further. If the shape is colliding with a already set shape from the side, the shape also needs to stop moving the current direction. Instead, the shape needs to go further straight down blocked by any other shape. In that scenario, the advancement vector needs to be changed during collision detection and the integrated another time for the corrected position.

The application also further tests the primitive renderer which renders the shapes and the gird.

## conculsion
The game is certainly not bug-free but took just a few hours(4-5h) to do to the point.
