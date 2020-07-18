#!/bin/bash

glslc  composition.vert -o composition.vert.spv 
glslc  composition.frag -o composition.frag.spv 

glslc  objects.vert -o objects.vert.spv 
glslc  objects.frag -o objects.frag.spv 

glslc  sky.vert -o sky.vert.spv 
glslc  sky.frag -o sky.frag.spv 

glslc  transparent.vert -o transparent.vert.spv 
glslc  transparent.frag -o transparent.frag.spv 

glslc  nk_gui.vert -o nk_gui.vert.spv 
glslc  nk_gui.frag -o nk_gui.frag.spv 

glslc  terrain.vert -o terrain.vert.spv 
glslc  terrain.tesc -o terrain.tesc.spv 
glslc  terrain.tese -o terrain.tese.spv 
glslc  terrain.frag -o terrain.frag.spv 
