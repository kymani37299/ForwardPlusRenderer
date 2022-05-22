# ForwardPlusRenderer

## Overview

<b>Depth Prepass</b> - Drawing depth information into a texture that will be used for an additional information what lights can be culled </br>
<b>Light culling</b> - Tile based culling, the screen is divided in tiles, and then for each tile we calculate which lights are affecting that area of screen </br>
<b>Geometry rendering</b> - Rendering geometry in forward fashion using the light info from the last step </br>

## Performance analysis

The more lights we have in the scene the bigger the performance gain we get. In the scene with a lot of lights only very small part of it will be rendered in the specific tile.
For example in the Sponza scene with 10k lights randomly placed around the map, most tiles will be calculating less than 30 lights. This is a huge gain, without this culling we would be calculating
10k lights per pixel instead of 30 lights per pixel.</br>
</br>
![Alt text](Images/Comparison.png?raw=true "ForwardPlusVersusForward")
