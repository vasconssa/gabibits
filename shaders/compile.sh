#!/bin/bash

glslangValidator -V composition.vert -o composition.vert.spv 
glslangValidator -V composition.frag -o composition.frag.spv 

glslangValidator -V objects.vert -o objects.vert.spv 
glslangValidator -V objects.frag -o objects.frag.spv 

glslangValidator -V sky.vert -o sky.vert.spv 
glslangValidator -V sky.frag -o sky.frag.spv 

glslangValidator -V transparent.vert -o transparent.vert.spv 
glslangValidator -V transparent.frag -o transparent.frag.spv 

glslangValidator -V terrain.vert -o terrain.vert.spv 
glslangValidator -V terrain.tesc -o terrain.tesc.spv 
glslangValidator -V terrain.tese -o terrain.tese.spv 
glslangValidator -V terrain.frag -o terrain.frag.spv 
