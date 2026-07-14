# Les démos sont des fichiers dans FufuLab/src/demos/ et FufuLab/include/demos/,
# ramassés automatiquement par le GLOB_RECURSE du CMakeLists.txt parent.
#
# Pour ajouter une démo :
#   1. Créer MonAlgoDemo.h et MonAlgoDemo.cpp dans FufuLab/src/demos/
#   2. Dans le .cpp : #include "Demo/DemoRegistry.h" + FUFULAB_REGISTER_DEMO(...)
#   3. Rebuild — la démo apparaît automatiquement dans la liste FufuLab
#
# Ce fichier est un placeholder nécessaire à add_subdirectory(demos).
