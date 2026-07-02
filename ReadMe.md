# Fufu Engine

<div align="center">

![Hero Section](./.github/assets/editos.png)
   
</div>

**Fufu** is a lightweight  **3D path tracing** engine. It focuses on rendering 3D scenes using path tracing, enabling experimentation with light transport simulation in virtual environments. While not as feature-packed as larger engines, Fufu provides a simple and approachable platform to learn and experiment with 3D rendering concepts and engine architecture.


## Roadmap

### FufuEngine

- [x] Application core logic for implementing multiple "app" type (editor and runtime) 
- [x] Event system
- [x] Asset management system (Bare minimum)
- [x] Introduce ECS system for handling all the entities
- [x] Add a scene SceneSerializer
- [x] Path tracer renderer
- [x] Add Project Management system
- [ ] Add a Skybox / HDR environment for the path tracer 

### FufuStudio

 - [x] Add viewport and renderer settings panel
 - [x] Hierarchy Panel + Detail Panel
 - [x] Saving and Loading Scenes from FufuStudio
 - [ ] Gizmos de transformation dans le viewport (translate, rotate, scale)
 - [ ] Contrôle avancé de la caméra (orbite, zoom, scroll)
 - [ ] Add Export and Run button 
 - [x] Add an Project Browser/Panel to manager project folders and assets.

### FufuRuntime

 - [x] Run a .fufuscene
 - [ ] 


## Troubleshooting

- If CMake fails to configure, ensure you have an internet connection for the first run — all dependencies are fetched via `FetchContent`.
- If you change CMake options or update dependency versions, delete the `build/` directory entirely and reconfigure from scratch.
- If fonts or icons don't render correctly in the editor, ensure `FufuStudio/assets/fonts/` contains `NotoSans-Regular.ttf`, `fa-solid-900.ttf`, and `codicon.ttf`.

## Contributions

This is primarily a personal learning project, but suggestions and discussions are welcome.